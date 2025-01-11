#pragma once

#include <circle/types.h>

#define KW11P_CSR 017772540
#define KW11P_CSB 017772542
#define KW11P_CTR 017772544
#define KW11_CSR  017777546

class KW11 {
    public:
        KW11();

        void write16(u32 a, u16 v) ;
        u16 read16(u32 a) ;
        void tick() ;
        void ptick() ;
        void reset() ;
    private:
        u16 csr ;
        u16 pcsr, pcsb, pctr, pcounter ;
} ;
