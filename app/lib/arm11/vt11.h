#pragma once

#include <circle/types.h>

#define VT11_DPC    017772000
#define VT11_MPR    017772002
#define VT11_GIXPR  017772004
#define VT11_CCYPR  017772006
#define VT11_RR     017772010
#define VT11_SPR    017772012
#define VT11_XOR    017772014
#define VT11_YOR    017772016
#define VT11_ANR    017772020
#define VT11_SCCR   017772022
#define VT11_NR     017772024
#define VT11_SDR    017772026
#define VT11_CSTR   017772030
#define VT11_SAMR   017772032
#define VT11_ZPR    017772034
#define VT11_ZOR    017772036

class VT11 {
    public:
        VT11() ;
        u16 read16(u32 a) ;
        void write16(u32 a, u16 v) ;
    private:
        u16 dpc, mpr, gixpr, ccypr, rr, spr, xosr, yosr, anr, samr ;
} ;
