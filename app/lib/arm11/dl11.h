#pragma once

#include <circle/types.h>
#include "xx11.h"

#define DL11_CSR 017776500

class DL11 : public XX11 {

    public:
        DL11();

        void clearterminal();
        void xpoll();
        void rpoll();
        virtual u16 read16(const u32 a);
        virtual void write16(const u32 a, const u16 v);

    private:
        u16 rcsr;
        u16 rbuf;
        u16 xcsr;
        u16 xbuf;
} ;
