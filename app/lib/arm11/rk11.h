#pragma once

#include <circle/types.h>
#include <fatfs/ff.h>
#include "xx11.h"

#define RK11_CSR 017777400

class RK11 : public XX11 {

  public:
	  FIL crtds[8];
    virtual u16 read16(const u32 a);
    virtual void write16(const u32 a, const u16 v);
    void reset();
    void step();
	  FRESULT fr;

  private:
	  u16 rkds, rker, rkcs, rkwc, rkba, rkda;
	  UINT bcnt;
    u32 sector, surface, cylinder,rkba18,rkdelay;
    u8 drive ;

    void rknotready();
    void rkready();
    void readwrite();
    void seek();
};
