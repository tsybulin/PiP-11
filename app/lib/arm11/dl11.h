#pragma once
#include <circle/types.h>

#define DL11_CSR 0776500

class DL11 {

    public:
        DL11();

        void clearterminal();
        void xpoll();
        void rpoll();
        u16 read16(u32 a);
        void write16(u32 a, u16 v);


    private:
        u16 rcsr;
        u16 rbuf;
        u16 xcsr;
        u16 xbuf;
} ;
