#include "arm11.h"

#include "kb11.h"
#include <cons/cons.h>
#include <odt/odt.h>
#include <circle/util.h>
#include <circle/setjmp.h>
#include <circle/timer.h>

#ifndef ARM_ALLOW_MULTI_CORE
#define ARM_ALLOW_MULTI_CORE
#endif

#include <circle/multicore.h>

#include "boot_defs.h"

#define DRIVE "USB:"

KB11 cpu;
int kbdelay = 0;
int clkdelay = 0;
u64 systime,nowtime,clkdiv;
FIL bload;
ODT odt ;

extern volatile bool interrupted ;
extern volatile bool halted ;

void setup(const char *rkfile, const char *rlfile, const bool bootmon) {
	if (cpu.unibus.rk11.crtds[0].obj.lockid) {
		return ;
    }

    for (u8 drv = 0; drv < 2; drv++) {
        char name[26] = DRIVE "/PIP-11/RL11_00.RL02" ;
        name[18] = '0' + drv ;

	    FRESULT fr = f_open(&cpu.unibus.rl11.disks[drv], !drv ? rlfile : name, FA_READ | FA_WRITE);
        if (FR_OK != fr && FR_EXIST != fr) {
            gprintf("f_open(%s) error: (%d)", !drv ? rlfile : name, fr) ;
            while (!interrupted) ;
        }
    }

    for (u8 crtd = 0; crtd < 8; crtd++) {
        char name[26] = DRIVE "/PIP-11/RK11_00.RK05" ;
        name[18] = '0' + crtd ;

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

static volatile bool cpuThrottle = false ;

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

        if (cpuThrottle) {
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

static bool hotkeyHandler(const unsigned char modifiers, const unsigned char hid_key, void *context) {
    Console *console = (Console *)context ;

    if (
        modifiers & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL) &&
        modifiers & (KEYBOARD_MODIFIER_LEFTALT | KEYBOARD_MODIFIER_RIGHTALT) &&
        hid_key == HID_KEY_DELETE
    ) {
		console->shutdownMode = ShutdownReboot ;
        CMultiCoreSupport::SendIPI(0, IPI_USER) ;

		return true ;
    }

    if (
        modifiers & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL) &&
        modifiers & (KEYBOARD_MODIFIER_LEFTALT | KEYBOARD_MODIFIER_RIGHTALT) &&
        hid_key == HID_KEY_BACKSPACE
    ) {
        CMultiCoreSupport::SendIPI(0, IPI_USER + 1) ;
        return true ;
    }

    if (!modifiers && hid_key == HID_KEY_F12) {
        console->vt52_mode = !console->vt52_mode ;
        console->showStatus() ;
        return true ;
    }

    if (modifiers & KEYBOARD_MODIFIER_LEFTGUI && hid_key == HID_KEY_SPACE) {
        console->koi7n1 = !console->koi7n1 ;
        console->showRusLat() ;
        return true ;
    }

    if (!modifiers && hid_key == HID_KEY_F11) {
        cpuThrottle = !cpuThrottle ;
        console->showThrottle(cpuThrottle) ;
        return true ;
    }

    return false ;
}

TShutdownMode startup(const char *rkfile, const char *rlfile, const bool bootmon) {
    Console::get()->setHotkeyHandler(hotkeyHandler, Console::get()) ;
    setup(rkfile, rlfile, bootmon);
    Console::get()->showThrottle(cpuThrottle) ;

    while (!interrupted) {
        loop();
    }

    return ShutdownReboot ;
}
