/*
Modified BSD License

Copyright (c) 2021 Chloe Lunn

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// sam11 software emulation of DEC PDP-11/40 RK11 RK Disk Controller

#pragma once

#include <circle/types.h>
#include <fatfs/ff.h>
#include "xx11.h"

#define RL11_CSR 017774400
enum {
    DEV_RL_BAE = 017774420,  // RL11 Bus Address Extension Register
    DEV_RL_MP  = 017774406,  // RL11 Multipurpose Register
    DEV_RL_DA  = 017774404,  // RL11 Disk Address
    DEV_RL_BA  = 017774402,  // RL11 Bus Address
    DEV_RL_CS  = RL11_CSR,   // RL11 Control Status
} ;

class RL11 : public XX11 {

public:
   FIL disks[4];
   virtual void write16(const u32 a, const u16 v);
   virtual u16 read16(const u32 a);
   void reset();
   void step();
   void rlnotready();
   void rlready();

private:
    u16 drun, dtype;
    u8 drive ;
    u16 RLWC, RLDA, RLMP, RLCS, RLBAE;
    u32 RLBA;
    unsigned int bcnt;
} ;

