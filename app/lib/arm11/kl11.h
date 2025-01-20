#pragma once

#include <circle/types.h>
#include "xx11.h"

#define KL11_XCSR 017777564
#define KL11_XBUF 017777566
#define KL11_RCSR 017777560
#define KL11_RBUF 017777562

class KL11 : public XX11 {

  public:
    KL11();

    void clearterminal();
    void xpoll() ;
    void rpoll() ;
    u16 read16(const u32 a);
    void write16(const u32 a, const u16 v);
	
  private:
    u16 rcsr;
    u16 rbuf;
    u16 xcsr;
    u16 xbuf;
} ;
