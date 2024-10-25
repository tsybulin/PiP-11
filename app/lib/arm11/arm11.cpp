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
	if (cpu.unibus.rk11.rk05.obj.lockid) {
		return ;
    }

	FRESULT fr = f_open(&cpu.unibus.rl11.rl02,rlfile, FA_READ | FA_WRITE);
	if (FR_OK != fr && FR_EXIST != fr) {
		gprintf("f_open(%s) error: (%d)", rlfile, fr);
		while (!interrupted) ;
	}

    fr = f_open(&cpu.unibus.rk11.rk05, rkfile, FA_READ | FA_WRITE);
	if (FR_OK != fr && FR_EXIST != fr) {
		gprintf("f_open(%s) error: (%d)", rkfile, fr);
		while (!interrupted) ;
	}
    
    clkdiv = (u64)1000000 / (u64)60;
    systime = CTimer::GetClockTicks64() ;
	
    cpu.reset(bootmon ? BOOTMON_BASE : BOOTRK_BASE);
    cpu.cpuStatus = CPU_STATUS_ENABLE ;
}

jmp_buf trapbuf;

void trap(u16 vec) { longjmp(trapbuf, vec); }

void loop() {
    auto vec = setjmp(trapbuf);

    if (vec) {
        cpu.trapat(vec) ;
        return ;
    }

    while (!interrupted) {
        if (halted) {
            halted = false ;
            cpu.cpuStatus = CPU_STATUS_HALT ;
        }

        if (cpu.cpuStatus == CPU_STATUS_HALT) {
            odt.loop() ;
            continue; ;
        }

        if ((cpu.itab[0].vec > 0) && (cpu.itab[0].pri > cpu.priority())) {
            cpu.trapat(cpu.itab[0].vec);
            cpu.popirq();
            return; // exit from loop to reset trapbuf
        }
        
        if (!cpu.wtstate) {
            cpu.step();
        }
        
        cpu.unibus.rk11.step();
        cpu.unibus.rl11.step();
        cpu.unibus.ptr_ptp.step() ;
        cpu.unibus.lp11.step() ;
        cpu.pirq() ;
        
        if (kbdelay++ == 2000) {
            cpu.unibus.cons.poll();
            cpu.unibus.dl11.poll();
            kbdelay = 0;
        }
        
        nowtime = CTimer::GetClockTicks64() ;
        if (nowtime - systime > clkdiv) {
            cpu.unibus.kw11.tick();
            systime = nowtime;
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
