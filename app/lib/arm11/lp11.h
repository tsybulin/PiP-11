#pragma once

#include <circle/types.h>

#define LP11_LPS 017777514
#define LP11_LPD 017777516

class LP11 {

  public:
    void step();
    void reset();
    u16 read16(u32 a);
    void write16(u32 a, u16 v);

    private:
        bool lpcheck ;
};