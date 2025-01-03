#include "unibus.h"

#include <circle/alloc.h>
#include <circle/logger.h>
#include "arm11.h"
#include "kb11.h"

extern KB11 cpu;

UNIBUS::UNIBUS() {
    core = (u16 *) malloc(MEMSIZE) ;
}

UNIBUS::~UNIBUS() {
    delete(core) ;
    core = 0 ;
}

void UNIBUS::ub_write16(const u32 a, const u16 v) {
    u32 aa = cpu.mmu.ub_decode(a) ;
    
    if (aa < MEMSIZE) {
        core[aa >> 1] = v ;
        return ;
    }

    CLogger::Get()->Write("UNIBUS", LogError, "ub_write16 non-existent address %08o", aa) ;
    while (1) {}
}

void UNIBUS::write16(const u32 a, const u16 v) {
    if  (a & 1) {
        //printf("unibus: write16 to odd address %06o\n", a);
        trap(INTBUS);
    }

    if (a < MEMSIZE) {
        core[a >> 1] = v;
        return;
    }

    if (a >= 017770200U && a <= 017770376U) {
        cpu.mmu.write16(a, v) ;
        return ;
    }

    switch (a & ~077) {
        case RK11_CSR:      
            rk11.write16(a, v);
            return;
        case RL11_CSR:
            rl11.write16(a, v);
            return;
        case DL11_CSR:
            switch (a & ~7) {
                case DL11_CSR:
                    dl11.write16(a, v);
                    return;
                }
            trap(INTBUS);
        case 017777500:
            switch (a) {
                case LP11_LPS:
                case LP11_LPD:
                    lp11.write16(a, v);
                    return;
                case PC11_PRS:
                case PC11_PRB:
                case PC11_PPS:
                case PC11_PPB:
                    ptr_ptp.write16(a, v) ;
                    return ;
                case KW11_CSR:
                    kw11.write16(a, v);
                    return;
                case 017777572:
                    cpu.mmu.SR[0] = v;
                    return;
                case 017777574:
                    // cpu.mmu.SR[1] = v; // read-only
                    return;
                case 017777576:
                    // cpu.mmu.SR[2] = v; // SR2 is read only
                    return;
                default:
                    cons.write16(a, v);
                    return;
            }
        case 017772200: // KK
        case 017772300: // SS
        case 017777600: // UU
            cpu.mmu.write16(a, v);
            return;
        case 017772500:
            switch (a) {
                case 017772516:
                    cpu.mmu.SR[3] = v & 017 ;
                    return ;
                default:
                    trap(INTBUS);
            }
        case 017777700:
            cpu.write16(a, v) ;
        default:
            // gprintf("unibus: write to invalid address %06o at %06o", a, cpu.PC);
            trap(INTBUS);
    }
    return;
}

u16 UNIBUS::ub_read16(const u32 a) {
    u32 aa = cpu.mmu.ub_decode(a) ;
    
    if (aa < MEMSIZE) {
        return core[aa >> 1] ;
    }

    CLogger::Get()->Write("UNIBUS", LogError, "ub_read16 non-existent address %08o", aa) ;
    while (1) {}
}

u16 UNIBUS::read16(const u32 a) {
    if (a & 1) {
        //printf("unibus: read16 from odd address %06o\n", a);
        trap(INTBUS);
    }

    if (a < MEMSIZE) {
        return core[a >> 1];
    }

    if (a >= 017770200U && a <= 017770376U) {
        return cpu.mmu.read16(a) ;
    }

    switch (a & ~077) {
        case RK11_CSR:
            return rk11.read16(a);
        case RL11_CSR:
            return rl11.read16(a);
        case DL11_CSR:
            switch (a & ~7) {
                case DL11_CSR:
                    return dl11.read16(a);
            }
            trap(INTBUS);
        case 017777500:
            switch (a) {
                case LP11_LPS:
                case LP11_LPD:
                    return lp11.read16(a);
                case PC11_PRS:
                case PC11_PRB:
                case PC11_PPS:
                case PC11_PPB:
                    return ptr_ptp.read16(a) ;
                case KW11_CSR:
                    return kw11.read16(a);
                case 017777572:
                    return cpu.mmu.SR[0];
                case 017777574:
                    return cpu.mmu.SR[1];
                case 017777576:
                    return cpu.mmu.SR[2];
                default:
                    return cons.read16(a);
            }
        case 017772200: // SS
        case 017772300: // KK
        case 017777600: // UU
            return cpu.mmu.read16(a);
        case 017772500:
            switch (a) {
                case 017772516:
                    return cpu.mmu.SR[3] & 017 ;
                default:
                    trap(INTBUS);
            }
        default:
            //printf("unibus: read from invalid address %06o\n", a);
            trap(INTBUS);
    }
    return 0;
}

void UNIBUS::reset(bool i2c) {
    cons.clearterminal();
    dl11.clearterminal();
    rk11.reset();
    rl11.reset();
    kw11.write16(KW11_CSR, 0x00); // disable line clock INTR
    if (i2c) {
        ptr_ptp.reset() ;
        lp11.reset();
    }
}
