#include "unibus.h"

#include <circle/alloc.h>
#include <circle/logger.h>
#include "arm11.h"
#include "kb11.h"

extern KB11 cpu;

XX011 xx011 ;

void UNIBUS::PUT_TBL(const u32 a, const PXX11 v) {
    assert(tbl_xx[(a & 017777) >> 1] == 0) ;
    tbl_xx[(a & 017777) >> 1] = v ;
}

UNIBUS::UNIBUS() {
    core = (u16 *) calloc(1, MEMSIZE) ;
    tbl_xx = (PXX11*) calloc(4096, sizeof xx011) ;
}

UNIBUS::~UNIBUS() {
    delete(core) ;
    delete(tbl_xx) ;
    core = 0 ;
}

void UNIBUS::init() {
    // KT11 Unibus Map
    for (u32 a = 017770200U ; a < 017770400U; a += 2) {
        PUT_TBL(a, &cpu.mmu) ;
    }

    // MK11 Control ans status registers
    for (u32 a = 017772100U; a < 017772140U; a += 2) {
        PUT_TBL(a, &xx011) ;
    }

    // KE11 registers
    for (u32 a = 017777310U; a < 017777332U; a += 2) {
        PUT_TBL(a, &xx011) ;
    }

    // VT11 registers
    for (u32 a = 017772000U; a < 017772040U; a += 2) {
        PUT_TBL(a, &vt11) ;
    }

    PUT_TBL(017777776, &cpu) ;
    PUT_TBL(017777774, &cpu) ;
    PUT_TBL(017777772, &cpu) ;
    PUT_TBL(017777770, &cpu) ;
    PUT_TBL(017777766, &cpu) ;
    PUT_TBL(017777570, &cpu) ;
    PUT_TBL(017777740, &cpu) ;
    PUT_TBL(017777742, &cpu) ;
    PUT_TBL(017777744, &cpu) ;
    PUT_TBL(017777746, &cpu) ;
    PUT_TBL(017777750, &cpu) ;
    PUT_TBL(017777752, &cpu) ;
    PUT_TBL(017777764, &cpu) ;
    PUT_TBL(017777760, &cpu) ;
    PUT_TBL(017777762, &cpu) ;

    PUT_TBL(TC11_ST, &tc11) ;
    PUT_TBL(TC11_CM, &tc11) ;
    PUT_TBL(TC11_WC, &tc11) ;
    PUT_TBL(TC11_BA, &tc11) ;
    PUT_TBL(TC11_DT, &tc11) ;

    PUT_TBL(RK11_CSR, &rk11) ;
    PUT_TBL(017777402, &rk11) ;
    PUT_TBL(017777404, &rk11) ;
    PUT_TBL(017777406, &rk11) ;
    PUT_TBL(017777410, &rk11) ;
    PUT_TBL(017777412, &rk11) ;

    PUT_TBL(DEV_RL_CS, &rl11) ;
    PUT_TBL(DEV_RL_MP, &rl11) ;
    PUT_TBL(DEV_RL_BA, &rl11) ;
    PUT_TBL(DEV_RL_DA, &rl11) ;
    PUT_TBL(DEV_RL_BAE, &rl11) ;

    PUT_TBL(DL11_CSR, &dl11) ;
    PUT_TBL(017776502, &dl11) ;
    PUT_TBL(017776504, &dl11) ;
    PUT_TBL(017776506, &dl11) ;

    PUT_TBL(KL11_XCSR, &cons) ;
    PUT_TBL(KL11_XBUF, &cons) ;
    PUT_TBL(KL11_RCSR, &cons) ;
    PUT_TBL(KL11_RBUF, &cons) ;

    PUT_TBL(LP11_LPS, &lp11) ;
    PUT_TBL(LP11_LPD, &lp11) ;

    PUT_TBL(PC11_PRS, &ptr_ptp) ;
    PUT_TBL(PC11_PRB, &ptr_ptp) ;
    PUT_TBL(PC11_PPS, &ptr_ptp) ;
    PUT_TBL(PC11_PPB, &ptr_ptp) ;

    PUT_TBL(KW11_CSR, &kw11) ;
    PUT_TBL(KW11P_CSR, &kw11) ;
    PUT_TBL(KW11P_CSB, &kw11) ;
    PUT_TBL(KW11P_CTR, &kw11) ;

    PUT_TBL(017777572, &cpu.mmu) ;
    PUT_TBL(017777574, &cpu.mmu) ;
    PUT_TBL(017777576, &cpu.mmu) ;
    PUT_TBL(017772516, &cpu.mmu) ;

    // KT11 SS/KK PAR/PDR
    for (u32 a = 017772200; a < 017772400; a += 2) {
        PUT_TBL(a, &cpu.mmu) ;
    }

    // KT11 UU PAR/PDR
    for (u32 a = 017777600; a < 017777700; a += 2) {
        PUT_TBL(a, &cpu.mmu) ;
    }
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
        cpu.errorRegister = 0100 ;
        trap(INTBUS);
    }

    if (a < MEMSIZE) {
        core[a >> 1] = v;
        return;
    }

    PXX11 px = tbl_xx[(a & 017777) >> 1] ;
    if (px) {
        px->write16(a, v) ;
        return ;
    }

    CLogger::Get()->Write("UNIBUS", LogError, "write16 non-existent address %08o", a) ;
    cpu.errorRegister = 020 ;
    trap(INTBUS);
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
        cpu.errorRegister = 0100 ;
        trap(INTBUS);
    }

    if (a < MEMSIZE) {
        return core[a >> 1];
    }

    PXX11 px = tbl_xx[(a & 017777) >> 1] ;
    if (px) {
        return px->read16(a) ;
    }

    CLogger::Get()->Write("UNIBUS", LogError, "read16 non-existent address %08o", a) ;
    cpu.errorRegister = 020 ;
    trap(INTBUS);
    return 0;
}

void UNIBUS::reset(bool i2c) {
    cons.clearterminal();
    dl11.clearterminal();
    rk11.reset();
    rl11.reset();
    tc11.reset() ;
    kw11.reset() ;
    if (i2c) {
        ptr_ptp.reset() ;
        lp11.reset();
    }
}
