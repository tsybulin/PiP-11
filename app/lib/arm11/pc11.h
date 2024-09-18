#pragma once

#include <circle/types.h>

class PC11 {

    public:
    u16 prs, prb, pps, ppb;

    u16 read16(u32 a);
};