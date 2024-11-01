#include "kt11.h"

extern volatile bool interrupted ;

u16 KT11::read16(const u32 a) {
    const u8 i = ((a & 017) >> 1);
    const u8 d = i + 8 ;

    switch (a & ~017) {
        case 0772200:
            return pages[01][i].pdr;
        case 0772220:
            return pages[01][d].pdr;
        case 0772240:
            return pages[01][i].par;
        case 0772260:
            return pages[01][d].par;
        case 0772300:
            return pages[00][i].pdr;
        case 0772320:
            return pages[00][d].pdr;
        case 0772340:
            return pages[00][i].par;
        case 0772360:
            return pages[00][d].par;
        case 0777600:
            return pages[03][i].pdr;
        case 0777620:
            return pages[03][d].pdr;
        case 0777640:
            return pages[03][i].par;
        case 0777660:
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
        case 0772200:
            pages[01][i].pdr = v & 077417;
            break;
        case 0772220:
            pages[01][d].pdr = v & 077417;
            break;
        case 0772240:
            pages[01][i].par = v & 07777;
            pages[01][i].pdr &= ~0100;
            break;
        case 0772260:
            pages[01][d].par = v & 07777;
            pages[01][d].pdr &= ~0100;
            break;
        case 0772300:
            pages[00][i].pdr = v & 077417;
            break;
        case 0772320:
            pages[00][d].pdr = v & 077417;
            break;
        case 0772340:
            pages[00][i].par = v & 07777;
            pages[00][i].pdr &= ~0100;
            break;
        case 0772360:
            pages[00][d].par = v & 07777;
            pages[00][d].pdr &= ~0100;
            break;
        case 0777600:
            pages[03][i].pdr = v & 077417;
            break;
        case 0777620:
            pages[03][d].pdr = v & 077417;
            break;
        case 0777640:
            pages[03][i].par = v & 07777;
            pages[03][i].pdr &= ~0100;
            break;
        case 0777660:
            pages[03][d].par = v & 07777;
            pages[03][d].pdr &= ~0100;
            break;
        default:
            gprintf("mmu::write16 write to invalid address %06o", a);
            trap(INTBUS); // intbus
    }
}
