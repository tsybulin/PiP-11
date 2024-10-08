#include "arm11.h"

#include "kb11.h"
#include <cons/cons.h>

#include <circle/util.h>
#include <circle/setjmp.h>
#include <circle/timer.h>

KB11 cpu;
int kbdelay = 0;
int clkdelay = 0;
u64 systime,nowtime,clkdiv;
FIL bload;

extern volatile bool interrupted ;

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

int sim_load(FIL *bld)
{
    int c[6], d;
    u32 i, cnt, csum;
    u32 org;


    do {                                                    /* block loop */
        csum = 0;                                           /* init checksum */
        for (i = 0; i < 6; ) {                              /* 6 char header */
            if ((c[i] = f_xgetc(bld))==-1)
                return 1;
            if ((i != 0) || (c[i] == 1))                    /* 1st must be 1 */
                csum = csum + c[i++];                       /* add into csum */
        }
        cnt = (c[3] << 8) | c[2];                           /* count */
        org = (c[5] << 8) | c[4];                           /* origin */
        if (cnt < 6)                                        /* invalid? */
            return 1;
        if (cnt == 6) {                                     /* end block? */
            return 0;
        }
        for (i = 6; i < cnt; i++) {                         /* exclude hdr */
            if ((d = f_xgetc(bld))==-1)                /* data char */
                return 1;
            csum = csum + d;                                /* add into csum */
            if (org & 1)
                d = (d << 8) | cpu.unibus.core[org >> 1];
            cpu.unibus.core[org>>1] = d;
            org = (org + 1) & 0177777;                      /* inc origin */
        }
        if ((d = f_xgetc(bld))==-1)                    /* get csum */
            return 1;
        csum = csum + d;                                    /* add in */
    } while ((csum & 0377) == 0);                       /* result mbz */
    return 1;
}


void setup(const char *rkfile, const char *rlfile) {
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
    
    clkdiv = (uint64_t)1000000 / (uint64_t)60;
    systime = CTimer::GetClockTicks64() ;
	
    cpu.reset(0140000);
}

jmp_buf trapbuf;

void trap(u16 vec) { longjmp(trapbuf, vec); }

// static u32 cycles = 0 ;

void loop() {
    auto vec = setjmp(trapbuf);

    if (vec) {
        cpu.trapat(vec) ;
        return ;
    }

    while (!interrupted) {
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
    }
}

TShutdownMode startup(const char *rkfile, const char *rlfile) {
    setup(rkfile,rlfile);
    
    while (!interrupted) {
        loop();
    }

    return ShutdownReboot ;
}
