#include "kt11.h"

extern volatile bool interrupted ;

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

u16 KT11::read16(const u32 a) {
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
            gprintf("mmu::read16 invalid read from %06o", a);
            trap(INTBUS); // intbus
            while(!interrupted) {} ;
            return 0 ;
    }
}

void KT11::write16(const u32 a, const u16 v) {
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
            pages[01][i].par = v & 07777;
            pages[01][i].pdr &= ~0300;
            break;
        case 017772260:
            pages[01][d].par = v & 07777;
            pages[01][d].pdr &= ~0300;
            break;
        case 017772300:
            pages[00][i].pdr = v & 077417;
            break;
        case 017772320:
            pages[00][d].pdr = v & 077417;
            break;
        case 017772340:
            pages[00][i].par = v & 07777;
            pages[00][i].pdr &= ~0300;
            break;
        case 017772360:
            pages[00][d].par = v & 07777;
            pages[00][d].pdr &= ~0300;
            break;
        case 017777600:
            pages[03][i].pdr = v & 077417;
            break;
        case 017777620:
            pages[03][d].pdr = v & 077417;
            break;
        case 017777640:
            pages[03][i].par = v & 07777;
            pages[03][i].pdr &= ~0300;
            break;
        case 017777660:
            pages[03][d].par = v & 07777;
            pages[03][d].pdr &= ~0300;
            break;
        default:
            gprintf("mmu::write16 write to invalid address %06o", a);
            trap(INTBUS); // intbus
    }
}
