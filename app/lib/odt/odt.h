#pragma once

#include <circle/types.h>
#include <cons/cons.h>

class ODT {
    public:
        ODT();
        void loop() ;
    private:
        void parseChar(const char c) ;
        void parseCommand() ;
        int parseArg(u32 *arg1, u32 *arg2, bool allow_space = false) ;

        bool prompt_shown ;
        char buf[80] ;
        u8 bufptr ;
        Console *cons ;
} ;
