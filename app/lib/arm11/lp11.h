#pragma once

#include <circle/types.h>
#include "xx11.h"

#define LP11_LPS 017777514
#define LP11_LPD 017777516

class LP11 : public XX11 {

  public:
    void step();
    void reset();
    virtual u16 read16(const u32 a);
    virtual void write16(const u32 a, const u16 v);

    private:
        bool lpcheck ;
};