#include "unibus.h"

#include "arm11.h"
#include "kb11.h"

extern KB11 cpu;

void UNIBUS::write16(const u32 a, const u16 v) {
    if  (a & 1) {
        //printf("unibus: write16 to odd address %06o\n", a);
        trap(INTBUS);
    }
    if (a < MEMSIZE) {
        core[a >> 1] = v;
        return;
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
        case 0777500:
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
                case 0777572:
                    cpu.mmu.SR[0] = v;
                    return;
                case 0777574:
                    cpu.mmu.SR[1] = v;
                    return;
                case 0777576:
                    // do nothing, SR2 is read only
                    return;
                default:
                    cons.write16(a, v);
                    return;
            }
        case 0772200:
        case 0772300:
        case 0777600:
            cpu.mmu.write16(a, v);
            return;
        case 0777700:
            cpu.write16(a, v) ;
        default:
            // gprintf("unibus: write to invalid address %06o at %06o", a, cpu.PC);
            trap(INTBUS);
    }
    return;
}

u16 UNIBUS::read16(const u32 a) {
    if (a & 1) {
        //printf("unibus: read16 from odd address %06o\n", a);
        trap(INTBUS);
    }
    if (a < MEMSIZE) {
        return core[a >> 1];
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
        case 0777500:
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
                case 0777572:
                    return cpu.mmu.SR[0];
                case 0777574:
                    return cpu.mmu.SR[1];
                case 0777576:
                    return cpu.mmu.SR[2];
                default:
                    return cons.read16(a);
            }
        case 0772200:
        case 0772300:
        case 0777600:
            return cpu.mmu.read16(a);
        default:
            //printf("unibus: read from invalid address %06o\n", a);
            trap(INTBUS);
    }
    return 0;
}

void UNIBUS::reset() {
    cons.clearterminal();
    dl11.clearterminal();
    rk11.reset();
    rl11.reset();
    kw11.write16(KW11_CSR, 0x00); // disable line clock INTR
    lp11.reset();
    ptr_ptp.reset() ;
    cpu.mmu.SR[0]=0;
}
