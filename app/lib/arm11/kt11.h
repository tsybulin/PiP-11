#pragma once
#include "arm11.h"

#include <cons/cons.h>

class KT11 {
    public:
        KT11() ;

        void reset() ;
        
        u16 SR[4] = {0, 0, 0, 0}; // MM status registers

        bool infotrap = false ;
        bool lastWasData = false ;

        inline u32 decode16(const u16 a) {
            return a > 0157777 ? ((u32)a) + (u32)017600000 : a ;
        }

        template <bool wr> inline u32 decode18(const u16 a, const u16 mode, bool d = false, bool src = false) {
            const auto i = (a >> 13) + (d ? 8 : 0); // page index
            const auto block = (a >> 6) & 0177;
            const auto disp = a & 077;

            if (pages[mode][i].nr()) {
                SR[0] = 0100001 | (d ? 020 : 0) ;
                SR[0] |= (a >> 12) & ~1;
                SR[0] |= (mode << 5) ;

                if (pages[mode][i].ed() ? (block < pages[mode][i].len()) : (block > pages[mode][i].len())) {
                    SR[0] |= (1 << 14);
                }
                trap(INTFAULT) ;
            }

            if (wr && !pages[mode][i].write()) {
                SR[0] = (1 << 13) | 1 | (d ? 020 : 0) ;
                SR[0] |= (a >> 12) & ~1;
                SR[0] |= (mode << 5) ;
                trap(INTFAULT) ;
            }

            if (!pages[mode][i].read()) {
                SR[0] = (1 << 15) | 1 | (d ? 020 : 0) ;
                SR[0] |= (a >> 12) & ~1;
                SR[0] |= (mode << 5) ;
                trap(INTFAULT) ;
            }


            if (pages[mode][i].ed() ? (block < pages[mode][i].len())
                                    : (block > pages[mode][i].len())) {
                SR[0] = (1 << 14) | 1 | (d ? 020 : 0);
                SR[0] |= (a >> 12) & ~1;
                SR[0] |= (mode << 5) ;
                trap(INTFAULT) ;
            }

            if (wr) {
                pages[mode][i].pdr |= 1 << 6;
            }

            u32 aa = (((pages[mode][i].addr() + block) << 6) + disp) & 0777777;

            if (SR[0] & 01000 && (
                (pages[mode][i].pdr & 7) == 1 ||
                (pages[mode][i].pdr & 7) == 4 ||
                (wr && (pages[mode][i].pdr & 7) == 5)
                ) && !is_internal(aa)
            ) {
                if (wr) {
                    SR[0] = (1 << 13) | 1 | (d ? 020 : 0) ;
                } else {
                    SR[0] = (1 << 15) | 1 | (d ? 020 : 0) ;
                }
                SR[0] |= (a >> 12) & ~1;
                SR[0] |= (mode << 5) ;

                pages[mode][i].pdr |= 0200 ;
                infotrap = true ;
            }

            // temp. fix for I/O page relocation
            if (aa > 0760000) {
                aa += 017000000 ;
            }

            return aa ;
        }

        template <bool wr> inline u32 decode22(const u16 a, const u16 mode, bool d = false, bool src = false) {
            const u8 apf = (a >> 13) + (d ? 8 : 0); // active page field, page index
            const auto bn = (a >> 6) & 0177;
            const auto dib = a & 077;

            if (pages[mode][apf].nr()) {
                SR[0] = 0100001 | (d ? 020 : 0) ;
                SR[0] |= (a >> 12) & ~1;
                SR[0] |= (mode << 5) ;

                if (pages[mode][apf].ed() ? (bn < pages[mode][apf].len()) : (bn > pages[mode][apf].len())) {
                    SR[0] |= (1 << 14);
                }
                trap(INTFAULT) ;
            }

            if (wr && !pages[mode][apf].write()) {
                SR[0] = (1 << 13) | 1 | (d ? 020 : 0) ;
                SR[0] |= (a >> 12) & ~1;
                SR[0] |= (mode << 5) ;
                trap(INTFAULT) ;
            }

            if (!pages[mode][apf].read()) {
                SR[0] = (1 << 15) | 1 | (d ? 020 : 0) ;
                SR[0] |= (a >> 12) & ~1;
                SR[0] |= (mode << 5) ;
                trap(INTFAULT) ;
            }


            if (pages[mode][apf].ed() ? (bn < pages[mode][apf].len())
                                    : (bn > pages[mode][apf].len())) {
                SR[0] = (1 << 14) | 1 | (d ? 020 : 0);
                SR[0] |= (a >> 12) & ~1;
                SR[0] |= (mode << 5) ;
                trap(INTFAULT) ;
            }

            if (wr) {
                pages[mode][apf].pdr |= 1 << 6;
            }

            u32 aa = (((pages[mode][apf].addr22() + bn) << 6) + dib) & 017777777;

            if (SR[0] & 01000 && (
                (pages[mode][apf].pdr & 7) == 1 ||
                (pages[mode][apf].pdr & 7) == 4 ||
                (wr && (pages[mode][apf].pdr & 7) == 5)
                ) && !is_internal(aa)
            ) {
                if (wr) {
                    SR[0] = (1 << 13) | 1 | (d ? 020 : 0) ;
                } else {
                    SR[0] = (1 << 15) | 1 | (d ? 020 : 0) ;
                }
                SR[0] |= (a >> 12) & ~1;
                SR[0] |= (mode << 5) ;

                pages[mode][apf].pdr |= 0200 ;
                infotrap = true ;
            }

            // temp. fix for I/O page relocation
            // if (aa > 0760000) {
            //     aa += 017000000 ;
            // }

            return aa ;
        }

        template <bool wr> inline u32 decode(const u16 a, const u16 mode, bool d = false, bool src = false) {
            lastWasData = d ;

            // MMU OFF
            if ((SR[0] & 0401) == 0) {
                lastWasData = false ;
                return decode16(a) ;
            }
            
            // MMU Maintenance, dst-only
            if (((SR[0] & 0401) == 0400) && src) {
                return decode16(a) ;
            }

            // MMU ON, 18bit mode
            if ((SR[3] & 020) == 0) {
                return decode18<wr>(a, mode, d, src) ;
            }

            // assertion_failed("22bit relocation", __FILE__, __LINE__) ;
            return decode22<wr>(a, mode, d, src) ;
        }

        inline u32 ub_decode(const u32 a) {
            // MMU off or UNIBUS 
            if ((SR[0] & 1) == 0 || (SR[3] & 040) == 0) {
                return a ;
            }

            u8 nr = (a & 0760000) >> 13 ;

            u32 aa = (a & 017776) + ((u32)UBMR[nr] | ((u32)UBMR[nr+1] & 077) << 16) ;

            return aa ;
        }

        u16 read16(const u32 a);
        void write16(const u32 a, const u16 v);

        struct page {
            u16 par,  // page address register (base address relocation register)
                pdr ; // page descriptor register

            inline u32 addr() { return par & 07777; }
            inline u32 addr22() { return par ; }
            inline u8 len() { return (pdr >> 8) & 0x7f; }
            inline bool read() {
                return ((pdr & 7) == 1) | ((pdr & 7) == 2) | ((pdr & 7) == 4) | ((pdr & 7) == 5) | ((pdr & 7) == 6) ;
            }
            inline bool write() {
                return ((pdr & 7) == 4) | ((pdr & 7) == 5) | ((pdr & 7) == 6) ;
            }
            inline bool ed() { return pdr & 8; }
            inline bool nr() {
                return ((pdr & 7) == 0) | ((pdr & 7) == 3) | ((pdr & 7) == 7) ;
            }
        };

        page pages[4][16] ;

    private:
        bool is_internal(const u32 a) ;
        u16 UBMR[64] ;
}; 