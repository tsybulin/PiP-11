#pragma once
#include "arm11.h"

#include <cons/cons.h>

class KT11 {

  public:
    u16 SR[4] = {0, 0, 0, 0}; // MM status registers

    u16 T ;

    template <bool wr> inline u32 decode(const u16 a, const u16 mode, bool d = false, bool src = false) {
        if ((SR[0] & 0401) == 0) {
            return a > 0157777 ? ((u32)a) + 0600000 : a;
        }
        
        if (((SR[0] & 0401) == 0400) && src) {
            return a > 0157777 ? ((u32)a) + 0600000 : a;
        }

        const auto i = (a >> 13) + (d ? 8 : 0); // page index

        if (pages[mode][i].nr()) {
            SR[0] = (1 << 15) | 1 | (d ? 020 : 0) ;
            SR[0] |= (a >> 12) & ~1;
            if (mode) {
                SR[0] |= (1 << 5) | (1 << 6);
            }
            trap(INTFAULT) ;
        }

        if (wr && !pages[mode][i].write()) {
            SR[0] = (1 << 13) | 1 | (d ? 020 : 0) ;
            SR[0] |= (a >> 12) & ~1;
            if (mode) {
                SR[0] |= (1 << 5) | (1 << 6);
            }
            trap(INTFAULT) ;
        }

        if (!pages[mode][i].read()) {
            SR[0] = (1 << 15) | 1 | (d ? 020 : 0) ;
            SR[0] |= (a >> 12) & ~1;
            if (mode) {
                SR[0] |= (1 << 5) | (1 << 6);
            }
            trap(INTFAULT) ;
        }

        const auto block = (a >> 6) & 0177;
        const auto disp = a & 077;

        if (pages[mode][i].ed() ? (block < pages[mode][i].len())
                                : (block > pages[mode][i].len())) {
            SR[0] = (1 << 14) | 1 | (d ? 020 : 0);
            SR[0] |= (a >> 12) & ~1;
            if (mode) {
                SR[0] |= (1 << 5) | (1 << 6);
            }
            trap(INTFAULT) ;
        }

        if (wr) {
            pages[mode][i].pdr |= 1 << 6;
        }

        u32 aa = (((pages[mode][i].addr() + block) << 6) + disp) & 0777777;

        // if (T == 4 && a == 0160000) {
        //     CLogger::Get()->Write("KT11", LogError, "decode a:%06o, m:%02o, d:%d, i:%d, A:%06o", a, mode, d, i, aa) ;
        // }

        return aa ;
    }

    u16 read16(const u32 a);
    void write16(const u32 a, const u16 v);

    struct page {
        u16 par,  // page address register (base address relocation register)
            pdr ; // page descriptor register

        inline u32 addr() { return par & 07777; }
        inline u8 len() { return (pdr >> 8) & 0x7f; }
        inline bool read() { return (pdr & 2) == 2; }
        inline bool write() { return (pdr & 6) == 6; };
        inline bool ed() { return pdr & 8; }
        inline bool nr() {
            return ((pdr & 7) == 0) | ((pdr & 7) == 7) ;
        } ;
    };

    page pages[4][16] ;
};