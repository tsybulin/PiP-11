#pragma once

#include <circle/types.h>
#include "xx11.h"

#define TOY_CSR 017777526
#define TOY_DAR 017777530
#define TOY_TLR 017777532
#define TOY_THR 017777534

class TOY : public XX11 {
    public:
        TOY() ;
        virtual u16 read16(const u32 a) ;
        virtual void write16(const u32 a, const u16 v) ;
        void step() ;
        void reset() ;
    private:
        bool ds3231_set_time() ;
        u16 csr, dar, tlr, thr ;
} ;
