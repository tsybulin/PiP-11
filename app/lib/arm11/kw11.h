#pragma once

#include <circle/types.h>

#define KW11_CSR 0777546

class KW11 {
  public:
    void write16(u32 a, u16 v);
    u16 read16(u32 a);

    KW11();
    void tick();

  private:
    u16 csr;
};