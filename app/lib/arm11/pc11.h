#pragma once

#include <circle/types.h>

#define PC11_PRS 017777550
#define PC11_PRB 017777552
#define PC11_PPS 017777554
#define PC11_PPB 017777556

class PC11 {
    public:
        u16 read16(u32 a);
        void write16(const u32 a, const u16 v) ;
        void reset() ;
        void step() ;

    private:
        bool ptrcheck, ptpcheck ;
} ;