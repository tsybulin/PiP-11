#include "kw11.h"

#include <circle/logger.h>
#include "arm11.h"
#include "kb11.h"

extern KB11 cpu;

KW11::KW11() {
}

void KW11::write16(u32 a, u16 v) {
    switch (a) {
        case KW11_CSR:
            csr = v;
            return;

        case KW11P_CSR:
            pcsr = (pcsr & 0200) | (v & 0177) ; // bits 15 cleared=, 7 read-only, 8..14 unused
            return ;
        case KW11P_CSB:
            pcsb = v ;
            pctr = pcsb ;
            return ;
        case KW11P_CTR:
            CLogger::Get()->Write("KW11", LogError, "write16 PCTR %08o : %06o", a, v) ;
            return ;

        default:
            CLogger::Get()->Write("KW11", LogError, "write16 non-existent address %08o : %06o", a, v) ;
            trap(INTBUS);
    }
}

u16 KW11::read16(u32 a) {
    switch (a) {
        case KW11_CSR:
            return csr;

        case KW11P_CSR: {
                u16 v = pcsr & ~040 ; // bit 5 write-only
                pcsr &= 077777 ; // 15 cleared
                pcounter = 0 ;
                return v ; 
            }
        case KW11P_CTR:
            return pctr ;
        case KW11P_CSB:
            return pcsb ;

        default:
            CLogger::Get()->Write("KW11", LogError, "read16  non-existent address %08o", a) ;
            trap(INTBUS);
            return 0 ;
    }
}

void KW11::tick() {
    csr |= (1 << 7);
    if (csr & (1 << 6)) {
        cpu.interrupt(INTCLOCK, 6);
    }
}

void KW11::ptick() {
    // GO?
    if ((pcsr & 1) == 0) {
        return ;
    }

    // ERR?
    if (pcsr & 0100000) {
        return ;
    }

    // RATE
    u8 rate = (pcsr >> 1) & 3 ;
    switch (rate) {
        case 0: // 100 kHz
            break ;
        case 1: // 10 kHz
            if (++pcounter < 10) {
                return ;
            }
            pcounter = 0 ;
            break ;
        case 2: // line, 60 Hz
            if (++pcounter < 1666) {
                return ;
            }
            pcounter = 0 ;
            break ;
        case 3: // external
            break ;
    }

    // UP/DOWN
    switch (pcsr & 020) {
        case 0: // down
            if (rate != 3) {
                pctr-- ;
            } else if (pcsr & 040) {
                pctr-- ;
                pcsr &= ~040 ;
            }
            break ;
        case 020: // up
            if (rate != 3) {
                pctr++ ;
            } else if (pcsr & 040) {
                pctr++ ;
                pcsr &= ~040 ;
            }
            break;
    }

    // OVER/UNDER-FLOW?
    if (((pcsr & 020) && pctr != 0177777) || (!(pcsr & 020) && pctr > 0)) {
        return ;
    }

    pcsr |= 0200 ; // DONE

    if (pcsr & 0100) {
        cpu.interrupt(INTPCLK, 7);
    }

    // REPEAT?
    if (pcsr & 010) {
        pctr = pcsb ;
    } else {
        pcsr &= 1 ;
    }
}

void KW11::reset() {
    csr = 0 ;
    pcsr = 0 ;
    pcounter = 0 ;
}
