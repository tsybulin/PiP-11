#pragma once

#include <circle/types.h>

class KL11 {

  public:
    KL11();

    void clearterminal();
    void poll();
    u16 read16(u32 a);
    void write16(u32 a, u16 v);
	void serial_putchar(char c);
	char serial_getchar();


  private:
    u16 rcsr;
    u16 rbuf;
    u16 xcsr;
    u16 xbuf;
    int iflag;

    inline bool rcvrdone() { return rcsr & 0x80; }
    inline bool xmitready() { return xcsr & 0x80; }
};