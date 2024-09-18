#include "kw11.h"
#include "arm11.h"
#include "kb11.h"

extern KB11 cpu;

void kw11alarm(int) { cpu.unibus.kw11.tick(); }
extern volatile bool interrupted ;

KW11::KW11() {
}

void KW11::write16(u32 a, u16 v) {
    switch (a) {
    case KW11_CSR:
        // printf("kw11: csr write %06o\n", v);
        csr = v;
        return;
    default:
        gprintf("kw11: write to invalid address %06o\n", a);
        trap(INTBUS);
    }
}

u16 KW11::read16(u32 a) {
    switch (a) {
    case KW11_CSR:
        // printf("kw11: csr read %06o\n", csr);
        return csr;
    default:
        gprintf("kw11: read from invalid address %06o\n", a);
        trap(INTBUS);
        while(!interrupted) {};
		return 0 ;
    }
}

void KW11::tick() {
    csr |= (1 << 7);
    if (csr & (1 << 6)) {
        cpu.interrupt(INTCLOCK, 6);
    }
}
