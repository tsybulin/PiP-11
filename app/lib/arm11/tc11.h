#pragma once

#include <circle/types.h>
#include <fatfs/ff.h>
#include "xx11.h"

#define TC11_ST 017777340
#define TC11_CM 017777342
#define TC11_WC 017777344
#define TC11_BA 017777346
#define TC11_DT 017777350

#define TC11_UNITS 1

struct TC11Unit {
    int block ;
    FIL file ;
} ;

class TC11 : public XX11 {
    public:
        TC11() ;

        virtual u16 read16(u32 a) ;
        virtual void write16(u32 a, u16 v) ;
        void reset() ;
        void step();

        TC11Unit units[TC11_UNITS] ;
    private:
        u16 tcst, tccm, tcba, tcdt ;
        s16 tcwc ;
        u8 unit ;
} ;
