#pragma once

#include <circle/types.h>
#include "xx11.h"

#define PC11_PRS 017777550
#define PC11_PRB 017777552
#define PC11_PPS 017777554
#define PC11_PPB 017777556

class PC11 : public XX11 {
    public:
        virtual u16 read16(const u32 a);
        virtual void write16(const u32 a, const u16 v) ;
        void reset() ;
        void step() ;

    private:
        void ptr_step() ;
        void ptp_step() ;
        bool ptrcheck, ptpcheck ;
} ;