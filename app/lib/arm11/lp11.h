#pragma once

#include <circle/types.h>

#define LP11_CSR 0777514

class LP11 {

  public:
    void poll();
    void reset();
    u16 read16(u32 a);
    void write16(u32 a, u16 v);

  private:
    u16 lps;
    u16 lpb;
    u16 count;
};