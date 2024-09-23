#pragma once
#include "kl11.h"
#include "rk11.h"
#include "kw11.h"
#include "pc11.h"
#include "lp11.h"
#include "rl11.h"
#include "dl11.h"

const u32 IOBASE_18BIT = 0760000;
const u32 MEMSIZE = (128+64+32) * 1024;

class UNIBUS {

  public:
	  u16 core[MEMSIZE/2];

    KL11 cons;
    RK11 rk11;
    KW11 kw11;
    PC11 ptr;
    LP11 lp11;
    RL11 rl11;
    DL11 dl11;

    void write16(u32 a, u16 v);
    u16 read16(u32 a);

    void reset();

} ;