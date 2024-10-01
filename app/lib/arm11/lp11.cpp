#include "lp11.h"

#include "kb11.h"

extern KB11 cpu;
extern volatile bool interrupted ;

void LP11::poll() {
    if (!(lps & 0x80)) {
        if (++count > 3000) {
            // fputc(lpb & 0x7f, stdout);
            lps |= 0x80;
            if (lps & (1 << 6)) {
                cpu.interrupt(0200, 4);
            }
        }
    }
}

u16 LP11::read16(u32 a) {
    switch (a) {
    case LP11_CSR:
        return lps;
    case 0777516:
        return 0; // lpb cannot be read
    default:
        gprintf("lp11: read from invalid address %06o\n", a);
        while(!interrupted) {}
        return 0 ;
    }
}

void LP11::write16(u32 a, u16 v) {
    switch (a) {
    case LP11_CSR:
        if (v & (1 << 6)) {
            lps |= 1 << 6;
        } else {
            lps &= ~(1 << 6);
        }
        if (lps & (1 << 6)) {
            cpu.interrupt(0200, 4);
        }
        break;
    case 0777516:
        lpb = v & 0x7f;
        lps &= 0xff7f;
        count = 0;
        break;
    default:
        gprintf("lp11: write to invalid address %06o\n", a);
        while (1) ;
    }
}

void LP11::reset() {
    lps = 0x80;
    lpb = 0;
}
