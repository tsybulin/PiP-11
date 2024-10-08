#include "kt11.h"

extern volatile bool interrupted ;

u16 KT11::read16(const u32 a) {
    // printf("kt11:read16: %06o\n", a);
    const auto i = ((a & 017) >> 1);
    switch (a & ~017) {
    case 0772200:
        return pages[01][i].pdr;
    case 0772240:
        return pages[01][i].par;
    case 0772300:
        return pages[00][i].pdr;
    case 0772340:
        return pages[00][i].par;
    case 0777600:
        return pages[03][i].pdr;
    case 0777640:
        return pages[03][i].par;
    default:
        gprintf("mmu::read16 invalid read from %06o", a);
        trap(INTBUS); // intbus
        while(!interrupted) {} ;
		return 0 ;
    }
}

void KT11::write16(const u32 a, const u16 v) {
    //  printf("kt11:write16: %06o %06o\n", a, v);
    const auto i = ((a & 017) >> 1);
    switch (a & ~017) {
    case 0772200:
        pages[01][i].pdr = v & 077416;
        break;
    case 0772240:
        pages[01][i].par = v & 07777;
        pages[01][i].pdr &= ~0100;
        break;
    case 0772300:
        pages[00][i].pdr = v & 077416;
        break;
    case 0772340:
        pages[00][i].par = v & 07777;
        pages[00][i].pdr &= ~0100;
        break;
    case 0777600:
        pages[03][i].pdr = v & 077416;
        break;
    case 0777640:
        pages[03][i].par = v & 07777;
        pages[03][i].pdr &= ~0100;
        break;
    default:
        gprintf("mmu::write16 write to invalid address %06o", a);
        trap(INTBUS); // intbus
    }
}
