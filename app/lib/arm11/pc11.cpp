#include "pc11.h"

#include "arm11.h"
#include "kb11.h"

#include <cons/cons.h>

extern volatile bool interrupted ;
extern KB11 cpu;

u16 PC11::read16(u32 a) {
    switch (a) {
        case PC11_PRS:
            return prs ;
        case PC11_PRB:
            prs = prs & ~0200 ; // clear DONE,
            return prb;
        case PC11_PPS:
            return pps ;
        case PC11_PPB:
            return ppb ;
        default:
            gprintf("pc11::read16 invalid read from %06o\n", a);
            trap(004);
            while(!interrupted) {}
    }
    return 0 ;
}

void PC11::write16(const u32 a, const u16 v) {
    switch (a) {
        case PC11_PRS:
            prs = (prs & 0177676) | (v & 0101) ; //only bits 6,0 is write-able
            if (prs & 01) {
                prs = (prs & ~0200) | 04000 ; // if GO - clear buffer; clear done and set busy
                prb = 0 ;
            }
            break;
        case PC11_PRB:
            break ; //read-only
        case PC11_PPS:
            pps = (pps & 0100200) | (v & 077577) ; // bits 15,7 are read-only
            if ((pps & 0100) && (pps & 0100200)) {
                cpu.interrupt(INTPTP, 5) ;
            }
            break ;
        case PC11_PPB:
            ppb = v ;
            pps = pps & ~0200 ;
            break;
        default:
            gprintf("pc11::write16 invalid write to %06o\n", a);
            trap(004);
            while(!interrupted) {}
    }
}

void PC11::reset() {
    prs = 0 ;
    pps = 0200 ; // clear interrupt, set ready

    f_close(&ptpfile) ;

    FRESULT fr = f_open(&ptpfile, "PTP.TAP", FA_WRITE | FA_CREATE_ALWAYS) ;
    if (fr != FR_OK && fr != FR_EXIST) {
        gprintf("pc11 ptp f_open %d", fr) ;
        pps |= 0100000 ;
    } else {
        f_truncate(&ptpfile) ;
        f_lseek(&ptpfile, 0) ;
    }

    f_close(&ptrfile) ;

    fr = f_open(&ptrfile, "PTR.TAP", FA_READ | FA_OPEN_ALWAYS) ;
    if (fr != FR_OK && fr != FR_EXIST) {
        gprintf("pc11 ptr f_open %d", fr) ;
        prs |= 0100000 ;
    } else {
        f_lseek(&ptrfile, 0) ;
    }
}

void PC11::step() {
    // ptr
    if (prs & 01) {
        UINT br = 0 ;
        FRESULT fr = f_read(&ptrfile, &prb, 1, &br) ;
        if (fr != FR_OK || br != 1) {
            prs |= 0100000 ;
            f_lseek(&ptrfile, 0) ;
        }
        
        prs = (prs & ~04001) | 0200 ; // clear enable, busy. set done

        if (prs & 0100) {
            cpu.interrupt(INTPTR, 4) ;
        }
    }

    // ptp
    if ((pps & 0200) == 0) {
        UINT bw = 0 ;
        FRESULT fr = f_write(&ptpfile, &ppb, 1, &bw) ;
        if (fr != FR_OK || bw != 1) {
            pps |= 0100000 ;
            gprintf("pc11 ptp f_write res=%d, bytes=%d", fr, bw) ;
        } else {
            f_sync(&ptpfile) ;
        }

        pps |= 0200 ; // set ready
        if (pps & 0100) {
            cpu.interrupt(INTPTP, 4) ;
        }
    }
}