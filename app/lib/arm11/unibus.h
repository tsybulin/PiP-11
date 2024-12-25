#pragma once
#include "kl11.h"
#include "rk11.h"
#include "kw11.h"
#include "pc11.h"
#include "lp11.h"
#include "rl11.h"
#include "dl11.h"

const u32 MEMSIZE = 0760000 ;

class UNIBUS {

  public:
	  u16 core[MEMSIZE/2];

    KL11 cons;
    RK11 rk11;
    KW11 kw11;
    PC11 ptr_ptp;
    LP11 lp11;
    RL11 rl11;
    DL11 dl11;

    void write16(u32 a, u16 v);
    u16 read16(u32 a);

    void reset(bool i2c = true);

} ;
