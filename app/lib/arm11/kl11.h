#pragma once

#include <circle/types.h>

#define KL11_XCSR 0777564
#define KL11_XBUF 0777566
#define KL11_RCSR 0777560
#define KL11_RBUF 0777562

class KL11 {

  public:
    KL11();

    void clearterminal();
    void xpoll() ;
    void rpoll() ;
    u16 read16(u32 a);
    void write16(u32 a, u16 v);
	
  private:
    u16 rcsr;
    u16 rbuf;
    u16 xcsr;
    u16 xbuf;
} ;
