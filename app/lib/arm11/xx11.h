#pragma once

#include <circle/types.h>

class XX11 {
    public:
        virtual u16 read16(const u32 a) ;
        virtual void write16(const u32 a, const u16 v) ;
} ;

typedef XX11* PXX11 ;

class XX011 : public XX11 {
    public:
        virtual u16 read16(const u32 a) {
            return 0 ;
        }

        virtual void write16(const u32 a, const u16 v) {
            //
        }
} ;
