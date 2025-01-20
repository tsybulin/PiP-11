#include "kt11.h"

#include "kb11.h"

extern KB11 cpu ;

KT11::KT11() {
    for (int i = 0; i < 64; i++) {
        UBMR[i] = 0 ;
    }
}

void KT11::reset() {
    for (int i = 0; i < 64; i++) {
        UBMR[i] = 0 ;
    }
    SR[0]=0;
    SR[3]=0;
}

bool KT11::is_internal(const u32 a) {
    switch (a) {
        case 017777572:
        case 017777574:
        case 017777576:
        case 017772516:
            return true ;
    }

    switch (a & ~017) {
        case 017772200:
        case 017772220:
        case 017772240:
        case 017772260:
        case 017772300:
        case 017772320:
        case 017772340:
        case 017772360:
        case 017777600:
        case 017777620:
        case 017777640:
        case 017777660:
            return true ;
    }

    return false ;
}

bool KT11::is_debug() {
    return cpu.cpuStatus != CPU_STATUS_ENABLE ;
}

u16 KT11::read16(const u32 a) {
    if (a >= 017770200U && a <= 017770376U) {
        u8 n = (a & 0176) >> 1 ;
        // CLogger::Get()->Write("KT11", LogError, "read16 from %08o UBMR[%d]", a, n) ;
        return UBMR[n] ;
    }

    switch (a) {
        case 017777572:
            return SR[0];
        case 017777574:
            return SR[1];
        case 017777576:
            return SR[2];
        case 017772516:
            return SR[3] & 067 ;
    }

    const u8 i = ((a & 017) >> 1);
    const u8 d = i + 8 ;

    switch (a & ~017) {
        case 017772200:
            return pages[01][i].pdr;
        case 017772220:
            return pages[01][d].pdr;
        case 017772240:
            return pages[01][i].par;
        case 017772260:
            return pages[01][d].par;
        case 017772300:
            return pages[00][i].pdr;
        case 017772320:
            return pages[00][d].pdr;
        case 017772340:
            return pages[00][i].par;
        case 017772360:
            return pages[00][d].par;
        case 017777600:
            return pages[03][i].pdr;
        case 017777620:
            return pages[03][d].pdr;
        case 017777640:
            return pages[03][i].par;
        case 017777660:
            return pages[03][d].par;
        default:
            CLogger::Get()->Write("KT11", LogError, "read16 from invalid address %08o", a) ;
			cpu.errorRegister = 020 ;
			trap(INTBUS);
            return 0 ;
    }
}

void KT11::write16(const u32 a, const u16 v) {
    if (a >= 017770200U && a <= 017770376U) {
        u8 n = (a & 0176) >> 1 ;
        if (n & 1) {
            UBMR[n] = v & 077 ;
        } else {
            UBMR[n] = v & 0177776 ;
        }
        // CLogger::Get()->Write("KT11", LogError, "write16 to %08o UBMR[%d] <- %06o", a, n, v) ;
        return  ;
    }

    switch (a) {
        case 017777572:
            SR[0] = v ;
            return ;
        case 017777574:
            // SR[1] = v; // read-only
            return ;
        case 017777576:
            // cpu.mmu.SR[2] = v; // SR2 is read only
            return;
        case 017772516:
            cpu.mmu.SR[3] = v & 067 ;
            return ;
    }

    const u8 i = ((a & 017) >> 1);
    const u8 d = i + 8 ;
    switch (a & ~017) {
        case 017772200:
            pages[01][i].pdr = v & 077417;
            break;
        case 017772220:
            pages[01][d].pdr = v & 077417;
            break;
        case 017772240:
            pages[01][i].par = v;
            pages[01][i].pdr &= ~0300;
            break;
        case 017772260:
            pages[01][d].par = v ;
            pages[01][d].pdr &= ~0300;
            break;
        case 017772300:
            pages[00][i].pdr = v & 077417;
            break;
        case 017772320:
            pages[00][d].pdr = v & 077417;
            break;
        case 017772340:
            pages[00][i].par = v;
            pages[00][i].pdr &= ~0300;
            break;
        case 017772360:
            pages[00][d].par = v;
            pages[00][d].pdr &= ~0300;
            break;
        case 017777600:
            pages[03][i].pdr = v & 077417;
            break;
        case 017777620:
            pages[03][d].pdr = v & 077417;
            break;
        case 017777640:
            pages[03][i].par = v;
            pages[03][i].pdr &= ~0300;
            break;
        case 017777660:
            pages[03][d].par = v;
            pages[03][d].pdr &= ~0300;
            break;
        default:
            CLogger::Get()->Write("KT11", LogError, "write16 to invalid address %08o", a) ;
			cpu.errorRegister = 020 ;
            trap(INTBUS); // intbus
    }
}
