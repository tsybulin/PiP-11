#include "arm11.h"

#include "kb11.h"
#include <cons/cons.h>
#include <odt/odt.h>
#include <circle/util.h>
#include <circle/setjmp.h>
#include <circle/timer.h>
#include <circle/sched/scheduler.h>

#ifndef ARM_ALLOW_MULTI_CORE
#define ARM_ALLOW_MULTI_CORE
#endif

#include <circle/multicore.h>

#include "boot_defs.h"

#define DRIVE "SD:"
#define NN 17

KB11 cpu;
int kbdelay = 0;
int clkdelay = 0;
u64 systime,nowtime,clkdiv;
FIL bload;
ODT odt ;

extern volatile bool interrupted ;
extern volatile bool halted ;
extern volatile bool kb11hrottle ;

void setup(const char *rkfile, const char *rlfile, const bool bootmon) {
	if (cpu.unibus.rk11.crtds[0].obj.lockid) {
		return ;
    }

    for (u8 drv = 0; drv < 2; drv++) {
        char name[26] = DRIVE "/PIP-11/RL11_00.RL02" ;
        name[NN] = '0' + drv ;

	    FRESULT fr = f_open(&cpu.unibus.rl11.disks[drv], !drv ? rlfile : name, FA_READ | FA_WRITE);
        if (FR_OK != fr && FR_EXIST != fr) {
            gprintf("f_open(%s) error: (%d)", !drv ? rlfile : name, fr) ;
            while (!interrupted) ;
        }
    }

    for (u8 crtd = 0; crtd < 8; crtd++) {
        char name[26] = DRIVE "/PIP-11/RK11_00.RK05" ;
        name[NN] = '0' + crtd ;

        FRESULT fr = f_open(&cpu.unibus.rk11.crtds[crtd], !crtd ? rkfile : name, FA_READ | FA_WRITE);
        if (FR_OK != fr && FR_EXIST != fr) {
            gprintf("f_open(%s) error: (%d)", !crtd ? rkfile : name, fr);
            while (!interrupted) ;
        }
    }

    for (u8 u = 0; u < TC11_UNITS; u++) {
        char name[26] = DRIVE "/PIP-11/TC11_00.TAPE" ;
        name[NN] = '0' + u ;
        FRESULT fr = f_open(&cpu.unibus.tc11.units[u].file, name, FA_READ | FA_WRITE | FA_OPEN_ALWAYS) ;
        if (FR_OK != fr && FR_EXIST != fr) {
            gprintf("f_open(%s) error: (%d)", name, fr) ;
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

// static volatile bool cpuThrottle = false ;

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

        if (kb11hrottle) {
            u64 now = CTimer::GetClockTicks64() ;
            while (CTimer::GetClockTicks64() - now < 6) {
                // ;
            }
        }
        
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
        
        cpu.unibus.rk11.step() ;
        cpu.unibus.rl11.step() ;
        cpu.unibus.tc11.step() ;
        cpu.unibus.ptr_ptp.step() ;
        cpu.unibus.lp11.step() ;
        cpu.pirq() ;
        
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

    return Console::get()->shutdownMode ;
}
