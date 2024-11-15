#include "arm11.h"

#include "kb11.h"
#include <cons/cons.h>
#include <odt/odt.h>
#include <circle/util.h>
#include <circle/setjmp.h>
#include <circle/timer.h>

#include "boot_defs.h"

KB11 cpu;
int kbdelay = 0;
int clkdelay = 0;
u64 systime,nowtime,clkdiv;
FIL bload;
ODT odt ;

extern volatile bool interrupted ;
extern volatile bool halted ;

/* Binary loader.

   Loader format consists of blocks, optionally preceded, separated, and
   followed by zeroes.  Each block consists of:

        001             ---
        xxx              |
        lo_count         |
        hi_count         |
        lo_origin        > count bytes
        hi_origin        |
        data byte        |
        :                |
        data byte       ---
        checksum

   If the byte count is exactly six, the block is the last on the tape, and
   there is no checksum.  If the origin is not 000001, then the origin is
   the PC at which to start the program.
*/

int f_xgetc(FIL *fl)
{
    unsigned char buf[2];
    UINT bctr;

    FRESULT fr=f_read(fl,buf,1,&bctr);
    if (fr==FR_OK)
        return buf[0];
    return -1;
}

void setup(const char *rkfile, const char *rlfile, const bool bootmon) {
	if (cpu.unibus.rk11.crtds[0].obj.lockid) {
		return ;
    }

    for (u8 drv = 0; drv < 2; drv++) {
        char name[20] = "PIP-11/RL11_00.RL02" ;
        name[13] = '0' + drv ;

	    FRESULT fr = f_open(&cpu.unibus.rl11.disks[drv], !drv ? rlfile : name, FA_READ | FA_WRITE);
        if (FR_OK != fr && FR_EXIST != fr) {
            gprintf("f_open(%s) error: (%d)", !drv ? rlfile : name, fr) ;
            while (!interrupted) ;
        }
    }

    for (u8 crtd = 0; crtd < 8; crtd++) {
        char name[20] = "PIP-11/RK11_00.RK05" ;
        name[13] = '0' + crtd ;

        FRESULT fr = f_open(&cpu.unibus.rk11.crtds[crtd], !crtd ? rkfile : name, FA_READ | FA_WRITE);
        if (FR_OK != fr && FR_EXIST != fr) {
            gprintf("f_open(%s) error: (%d)", !crtd ? rkfile : name, fr);
            while (!interrupted) ;
        }
    }

    clkdiv = (u64)1000000 / (u64)60;
    systime = CTimer::GetClockTicks64() ;
	
    cpu.reset(bootmon ? BOOTMON_BASE : BOOTRK_BASE);
    // cpu.reset(bootmon ? ABSLOADER_BASE : BOOTRK_BASE);
    cpu.cpuStatus = CPU_STATUS_ENABLE ;
}

jmp_buf trapbuf;

void trap(u8 vec) { longjmp(trapbuf, vec); }

void loop() {
    auto vec = setjmp(trapbuf);

    if (vec) {
        cpu.trapat(vec) ;

        if (cpu.cpuStatus == CPU_STATUS_STEP) {
            cpu.cpuStatus = CPU_STATUS_HALT ;
        }

        return ;
    }

    while (!interrupted) {
        if (halted) {
            halted = false ;
            cpu.cpuStatus = CPU_STATUS_HALT ;
        }

        if (cpu.cpuStatus == CPU_STATUS_HALT) {
            odt.loop() ;
            continue ;
        }

        // u64 now = CTimer::GetClockTicks64() ;
        // while (CTimer::GetClockTicks64() - now < 5) {
        //     // ;
        // }
        
        u8 ivec = cpu.interrupt_vector() ;

        if (ivec) {
            cpu.trapat(ivec) ;
            cpu.wtstate = false;
            return ; // exit from loop to reset trapbuf
        }

        cpu.updatePriority() ;

        if (!cpu.wtstate) {
            cpu.wasRTT = false ;
            cpu.stackTrap = STACK_TRAP_NONE ;

            cpu.step();

            if (cpu.odtbpt > 0 && cpu.RR[7] == cpu.odtbpt) {
                cpu.cpuStatus = CPU_STATUS_HALT ;
            }
        }
        
        cpu.unibus.rk11.step();
        cpu.unibus.rl11.step();
        cpu.unibus.ptr_ptp.step() ;
        cpu.unibus.lp11.step() ;
        cpu.pirq() ;
        
        // if (clkdelay++ > 5000) {
        //     clkdelay = 0 ;
        cpu.unibus.cons.xpoll() ;
        cpu.unibus.dl11.xpoll() ;
        // }

        if (kbdelay++ == 2000) {
            cpu.unibus.cons.rpoll() ;
            cpu.unibus.dl11.rpoll() ;
            kbdelay = 0;
        }
        
        nowtime = CTimer::GetClockTicks64() ;
        if (nowtime - systime > clkdiv) {
            cpu.unibus.kw11.tick();
            systime = nowtime;
        }

        if (cpu.stackTrap == STACK_TRAP_YELLOW) {
            trap(INTBUS) ;
        } else if (cpu.stackTrap == STACK_TRAP_RED) {
            cpu.RR[6] = 4 ;
            cpu.trapat(INTBUS) ;
        }

        if ((cpu.PSW & PSW_BIT_T) && !cpu.wasRTT) {
            trap(INTDEBUG) ;
        }

        if (cpu.mmu.infotrap) {
            cpu.mmu.infotrap = false ;
            trap(INTFAULT) ;
        }

        if (cpu.cpuStatus == CPU_STATUS_STEP) {
            cpu.cpuStatus = CPU_STATUS_HALT ;
        }
    }
}

TShutdownMode startup(const char *rkfile, const char *rlfile, const bool bootmon) {
    setup(rkfile, rlfile, bootmon);
    
    while (!interrupted) {
        loop();
    }

    return ShutdownReboot ;
}
