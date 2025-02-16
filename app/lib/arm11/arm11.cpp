#include "arm11.h"

#include "kb11.h"
#include <cons/cons.h>
#include <odt/odt.h>
#include <circle/util.h>
#include <circle/setjmp.h>
#include <circle/timer.h>
#include <circle/sched/scheduler.h>
#include <circle/logger.h>

#ifndef ARM_ALLOW_MULTI_CORE
#define ARM_ALLOW_MULTI_CORE
#endif

#include <circle/multicore.h>

#include "boot_defs.h"

#define BASEPATH "SD:/PIP-11/"
#define NN 17

KB11 cpu;
int kbdelay = 0;
int clkdelay = 0;
u64 systime,nowtime,clkdiv, ptime;
ODT odt ;

extern volatile bool interrupted ;
extern volatile bool halted ;
extern volatile bool kb11hrottle ;

void setup(const char *rkfile, const char *rlfile, const bool bBootmon) {
	if (cpu.unibus.rk11.crtds[0].obj.lockid) {
		return ;
    }

    for (u8 drv = 0; drv < 4; drv++) {
        char name[26] = BASEPATH "RL11_00.RL02" ;
        name[NN] = '0' + drv ;

	    FRESULT fr = f_open(&cpu.unibus.rl11.disks[drv], !drv ? rlfile : name, FA_READ | FA_WRITE);
        if (FR_OK != fr && FR_EXIST != fr) {
            gprintf("f_open(%s) error: (%d)", !drv ? rlfile : name, fr) ;
            while (!interrupted) ;
        }
    }

    for (u8 crtd = 0; crtd < 8; crtd++) {
        char name[26] = BASEPATH "RK11_00.RK05" ;
        name[NN] = '0' + crtd ;

        FRESULT fr = f_open(&cpu.unibus.rk11.crtds[crtd], !crtd ? rkfile : name, FA_READ | FA_WRITE);
        if (FR_OK != fr && FR_EXIST != fr) {
            gprintf("f_open(%s) error: (%d)", !crtd ? rkfile : name, fr);
            while (!interrupted) ;
        }
    }

    for (u8 u = 0; u < TC11_UNITS; u++) {
        char name[26] = BASEPATH "TC11_00.TAPE" ;
        name[NN] = '0' + u ;
        FRESULT fr = f_open(&cpu.unibus.tc11.units[u].file, name, FA_READ | FA_WRITE | FA_OPEN_ALWAYS) ;
        if (FR_OK != fr && FR_EXIST != fr) {
            gprintf("f_open(%s) error: (%d)", name, fr) ;
            while (!interrupted) ;
        }
    }

    clkdiv = (u64)1000000 / (u64)60;
    systime = CTimer::GetClockTicks64() ;
    ptime = systime ;
	
    DIR dir ;
    FILINFO fno ;
    FIL of ;
    u16 w ;
    UINT br ;
    char fname[100] ;

    FRESULT fr = f_findfirst(&dir, &fno, BASEPATH, "*.OVL") ;
    while (fr == FR_OK && fno.fname[0]) {
        strcpy(fname, BASEPATH) ;
        strcat(fname, fno.fname) ;

        FRESULT fr1 = f_open(&of, fname, FA_READ | FA_OPEN_EXISTING) ;
        if (fr1 == FR_OK || fr1 == FR_EXIST) {
            fr1 = f_read(&of, &w, 2, &br) ;
            u16 a = w ;
            CLogger::Get()->Write("ARM11", LogError, "load %06o : %s", a, fno.fname) ;
            fr1 = f_read(&of, &w, 2, &br) ;
            while (fr1 == FR_OK && br == 2) {
                cpu.unibus.write16(a, w) ;
                fr1 = f_read(&of, &w, 2, &br) ;
                a += 2 ;
            }
        } else {
            CLogger::Get()->Write("ARM11", LogError, "f_open err %s %d", fname, fr1) ;
        }
        f_close(&of) ;
        fr = f_findnext(&dir, &fno) ;
    }
    
    cpu.reset(bBootmon ? BOOTMON_BASE : BOOTRK_BASE);
    cpu.cpuStatus = CPU_STATUS_ENABLE ;
}

jmp_buf trapbuf;

void trap(u8 vec) { longjmp(trapbuf, vec); }

inline bool QINTERRUPT() {
    u8 ivec = cpu.interrupt_vector() ;
    if (ivec) {
        cpu.trapat(ivec) ;
        if (cpu.cpuStatus == CPU_STATUS_STEP) {
            cpu.cpuStatus = CPU_STATUS_HALT ;
        }
        return true ;
    }

    return false ;
}

inline void hw_step() {
    if (cpu.cpuStatus != CPU_STATUS_HALT) {
        cpu.unibus.rk11.step() ;
        cpu.unibus.rl11.step() ;
        cpu.unibus.tc11.step() ;
        cpu.unibus.ptr_ptp.step() ;
        cpu.unibus.lp11.step() ;
        cpu.pirq() ;
        
        cpu.unibus.cons.xpoll() ;
        cpu.unibus.dl11.xpoll() ;

        if (kbdelay++ > 60) {
            cpu.unibus.cons.rpoll() ;
            cpu.unibus.dl11.rpoll() ;
            kbdelay = 0;
        }
        
        nowtime = CTimer::GetClockTicks64() ;
        if (nowtime - systime > clkdiv) {
            cpu.unibus.kw11.tick();
            systime = nowtime;
        }

        if (nowtime - ptime > 10ul) { // 100 kHz
            ptime = nowtime ;
            cpu.unibus.kw11.ptick() ;
        }

        cpu.unibus.toy.step() ;
    }
}

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

        hw_step() ;
        
        if (QINTERRUPT()) {
            return ; // exit from loop to reset trapbuf
        }

        if (!cpu.wtstate) {
            cpu.wasRTT = false ;
            cpu.wasSPL = false ;
            cpu.stackTrap = STACK_TRAP_NONE ;

            cpu.step();

            if (cpu.odtbpt > 0 && cpu.RR[7] == cpu.odtbpt) {
                cpu.cpuStatus = CPU_STATUS_HALT ;
            }
        }

        if (!cpu.wasSPL) {
            cpu.updatePriority() ;
        }

        if (cpu.stackTrap == STACK_TRAP_YELLOW) {
            cpu.errorRegister = 010 ;
            cpu.trapat(INTBUS) ;
        } else if (cpu.stackTrap == STACK_TRAP_RED) {
            cpu.errorRegister = 4 ;
            cpu.RR[6] = 4 ;
            trap(INTBUS) ;
        }

        if (QINTERRUPT()) {
            return ; // exit from loop to reset trapbuf
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
    cpu.unibus.init() ;
    
    setup(rkfile, rlfile, bootmon);

    while (!interrupted) {
        loop();
    }

    return Console::get()->shutdownMode ;
}
