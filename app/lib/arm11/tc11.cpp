#include "tc11.h"

#include <circle/logger.h>
#include "arm11.h"
#include "kb11.h"

extern KB11 cpu;

enum TC11COMMAND : u8 {
    TCC_SAT,   // stop all tape motion; all units
    TCC_RNUM,  // search block
    TCC_RDATA, // read data
    TCC_RALL,  // read all info
    TCC_SST,   // stop motion on selected unit
    TCC_WRTM,  // write time marks
    TCC_WDATA, // write data
    TCC_WALL   // write all unfo
} ;

#define DRUN 6
static int drun = 0 ;

#define TC11_BSZ 512

TC11::TC11() {
    for (u8 u = 0; u < TC11_UNITS; u++) {
        units[u].block = 0 ;
    }
}

u16 TC11::read16(u32 a) {
    switch (a) {
        case TC11_ST:
            return tcst ;
        case TC11_CM:
            return tccm & 0137776 ; // bit 0 is write only
        case TC11_WC:
            return tcwc ;
        case TC11_BA:
            return tcba & 0177776 ; // bit 0 is unused
        case TC11_DT:
            return tcdt ;
        
        default:
            CLogger::Get()->Write("TC11", LogError, "read16 non-existent address %08o", a) ;
            trap(INTBUS);
    }

    return 0 ;
}

void TC11::write16(u32 a, u16 v) {
    switch (a) {
        case TC11_CM:
            tccm =  (tccm & 0200) | (v & 0177577) ; // bit 7 is read only
            
            // unit selection
            unit = (tccm >> 8) & 7 ;
            if (unit == 0) {
                tcst &= 0173777 ;
            } else {
                tcst |= 04000 ;
                tccm |= 0100000 ;
                drun = 0 ;
            }

            // clear READY, DO on DO set
            if (tccm & 1) {
                tccm &= 0177576 ;
                drun = DRUN ;
            }

            // clear status errors if no command error
            if ((tccm & 0100000) == 0) {
                tcst &= 014377 ;
            }
            // CLogger::Get()->Write("TC11", LogError, "write16 cmd %d, dir %d, unit %d", (tccm >> 1) & 7, (tccm >> 11) & 1, unit) ;
            break;
        case TC11_WC:
            tcwc = v ;
            break ;
        case TC11_BA:
            tcba = v & 0177776 ; // bit 0 is unused
            break;
        
        default:
            CLogger::Get()->Write("TC11", LogError, "write16 non-existent address %08o : %06o", a, v) ;
            trap(INTBUS);
    }
}

void TC11::reset() {
    tcst &= 014134 ; // effect of INIT signal: clear bits 15, 14, 13, 10 through 7, 5, 1 and O
    tccm = (tccm & 0140201) | 0200 ;  // clear bits 13 through 8, 6 through 1; set bit 7
    tcwc = tcba = tcdt = 0 ;
    unit = 0 ;

    for (u8 u = 0; u < TC11_UNITS; u++) {
        units[u].block = 0 ;
    }
}

void TC11::step() {
    if (!drun) {
        return ;
    }

    if (--drun > 0) {
        return ;
    }

    TC11COMMAND cmd = (TC11COMMAND)((tccm >> 1) & 7) ;
    switch (cmd) {
        case TCC_SAT: {
                // CLogger::Get()->Write("TC11", LogError, "step cmd SAT") ;
                tcst &= 0177 ; // clear status errors
                tccm = (tccm & 077776) | 0200 ; // clear DO, set READY
                drun = 0 ;
                if (tccm & 0100) {
                    cpu.interrupt(INTTC, 6) ;
                }
            }
            break;

        case TCC_SST: {
                // CLogger::Get()->Write("TC11", LogError, "step cmd SST %d", unit) ;
                tcst &= 0177 ; // clear status errors
                tccm = (tccm & 077776) | 0200 ; // clear DO, set READY
                drun = 0 ;
                if (tccm & 0100) {
                    cpu.interrupt(INTTC, 6) ;
                }
            }
            break;

        case TCC_RNUM: {
                u8 dir = (tccm >> 11) & 1 ;
                if (dir) {
                    units[unit].block-- ;
                    if (units[unit].block < 0) {
                        // units[unit].block = 0 ;
                        tcst &= ~0200 ; // !@speed
                        tcst |= 0100000 ; // ENDZ
                        tccm |= 0100000 ; // ERROR
                    } else {
                        tcst &= 077777 ; // !ENDZ
                        tcst |= 0200 ; // @speed
                    }
                } else {
                    units[unit].block++ ;
                    if (units[unit].block > 577) {
                        units[unit].block = 577 ;
                        tcst &= ~0200 ; // !@speed
                        tcst |= 0100000 ; // ENDZ
                        tccm |= 0100000 ; // ERROR
                    } else {
                        tcst &= 077777 ; // !ENDZ
                        tcst |= 0200 ; // @speed
                    }
                }

                tcdt = units[unit].block ;
                tccm = (tccm & ~1) | 0200 ; // clear DO, set READY
                drun = 0 ;
                // CLogger::Get()->Write("TC11", LogError, "step cmd RNUM %d %s, block %d", unit, dir ? "rev" : "fwd", units[unit].block) ;
                if (tccm & 0100) {
                    cpu.interrupt(INTTC, 6) ;
                }
            }
            break;
        
        case TCC_WRTM:
            CLogger::Get()->Write("TC11", LogError, "step cmd WRTM %d", unit) ;
            units[unit].block = 0 ;
            tcst &= 0177 ;
            tccm = (tccm & 077776) | 0200 ;
            drun = 0 ;
            if (tccm & 0100) {
                cpu.interrupt(INTTC, 6) ;
            }
            break;

        case TCC_RDATA: {
                // CLogger::Get()->Write("TC11", LogError, "step cmd RDATA %d, block %06o", unit, units[unit].block) ;
                if (units[unit].block < 0 || units[unit].block > 577) {
                        tcst &= ~0200 ; // !@speed
                        tcst |= 0100000 ; // ENDZ
                        tccm |= 0100200 ; // ERROR, READY
                } else {
                    FRESULT fr = f_lseek(&units[unit].file, units[unit].block * TC11_BSZ) ;
                    if (fr != FR_OK) {
                        tcst |= 02000 ;
                        tccm |= 0100200 ; // ERROR, READY
                        CLogger::Get()->Write("TC11", LogError, "step WDATA seek err %d", fr) ;
                    } else {
                        FRESULT fr = FR_OK ;
                        UINT br ;
                        while (tcwc != 0 && fr == FR_OK) {
                            u32 aa = ((u32) tcba) | ((u32)((tccm >> 4) & 3) << 16) ;
                            u16 word = 0 ;
                            br = 0 ;
                            fr = f_read(&units[unit].file, &word, 2, &br) ;
                            if (fr != FR_OK) {
                                tcst |= 02000 ;
                                tccm |= 0100000 ;
                                CLogger::Get()->Write("TC11", LogError, "step RDATA f_read err %d", fr) ;
                            } else {
                                cpu.unibus.ub_write16(aa, word) ;
                            }
                            // CLogger::Get()->Write("TC11", LogError, "step cmd WDATA %d, %06o words from %08o to block %d %d", unit, -tcwc, aa, units[unit].block, units[unit].dib) ;
                            tcwc++ ;
                            aa += 2 ;
                            tcba = aa & 0177777 ;
                            tccm = (tccm & ~060) | ((aa >> 12) & 060) ;
                        }
                        tcst &= 077777 ; // !ENDZ
                        tcst |= 0200 ; // @speed
                        tccm = (tccm & 077776) | 0200 ; // !ERROR, READY
                    }
                }

                drun = 0 ;
                if (tccm & 0100) {
                    cpu.interrupt(INTTC, 6) ;
                }
            }
            break;

        case TCC_WDATA: {
                // CLogger::Get()->Write("TC11", LogError, "step cmd WDATA %d, block %d", unit, units[unit].block) ;
                if (units[unit].block < 0 || units[unit].block > 577) {
                        tcst &= ~0200 ; // !@speed
                        tcst |= 0100000 ; // ENDZ
                        tccm |= 0100200 ; // ERROR, READY
                } else {
                    FRESULT fr = f_lseek(&units[unit].file, units[unit].block * TC11_BSZ) ;
                    if (fr != FR_OK) {
                        tcst |= 02000 ;
                        tccm |= 0100200 ; // ERROR, READY
                        CLogger::Get()->Write("TC11", LogError, "step WDATA seek err %d", fr) ;
                    } else {
                        FRESULT fr = FR_OK ;
                        UINT bw ;
                        while (tcwc != 0 && fr == FR_OK) {
                            u32 aa = ((u32) tcba) | ((u32)((tccm >> 4) & 3) << 16) ;
                            u16 word = cpu.unibus.ub_read16(aa) ;
                            bw = 0 ;
                            fr = f_write(&units[unit].file, &word, 2, &bw) ;
                            if (fr != FR_OK) {
                                tcst |= 02000 ;
                                tccm |= 0100000 ;
                                CLogger::Get()->Write("TC11", LogError, "step WDATA f_write err %d", fr) ;
                            }
                            // CLogger::Get()->Write("TC11", LogError, "step cmd WDATA %d, %06o words from %08o to block %d %d", unit, -tcwc, aa, units[unit].block, units[unit].dib) ;
                            tcwc++ ;
                            aa += 2 ;
                            tcba = aa & 0177777 ;
                            tccm = (tccm & ~060) | ((aa >> 12) & 060) ;
                        }
                        f_sync(&units[unit].file) ;
                        tcst &= 077777 ; // !ENDZ
                        tcst |= 0200 ; // @speed
                        tccm = (tccm & 077776) | 0200 ; // !ERROR, READY
                    }
                }

                drun = 0 ;
                if (tccm & 0100) {
                    cpu.interrupt(INTTC, 6) ;
                }
            }
            break;

        default:
            CLogger::Get()->Write("TC11", LogError, "step unknown command %1o", cmd) ;
            break;
    }
}
