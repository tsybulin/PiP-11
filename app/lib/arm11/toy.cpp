#include "toy.h"

#include "kb11.h"
#include <circle/logger.h>
#include <circle/i2cmaster.h>

extern KB11 cpu ;
extern CI2CMaster *pI2cMaster ;

#define I2C_SLAVE 0150

static u8 buf[8] = {
	0, // regaddr
	0, // seconds
	0, // minutes
	0, // hours
	0, // weekdays
	0, // days
	0, // months
	0  // years
} ;

enum ATTRS {
	REGADDR = 0,
	SECONDS,
	MINUTES,
	HOURS,
	WEEKDAYS,
	DAYS,
	MONTHS,
	YEARS
} ;

bool ds3231_read_time(u8 *buf) {   
    int r = pI2cMaster->WriteReadRepeatedStart(I2C_SLAVE, buf, 1, &buf[SECONDS], 7) ;
    if (r != 7) {
        CLogger::Get()->Write("TOY", LogError, "ds3231_read_time err r = %d", r) ;
    }

    return r == 7 ;
}

bool TOY::ds3231_set_time() {
	char obf[2] ;

    u32 total = ((((u32) thr) << 16) | tlr) / 60U ;
    u32 hours = total / 3600U ;
    u32 minutes = (total % 3600U) / 60U ;
    u32 seconds = total - hours * 3600U - minutes * 60U ;

    obf[0] = 0x00 ;
	obf[1] = ((seconds % 10) & 017) | (((seconds / 10) & 7) << 4) ;
    int r = pI2cMaster->Write(I2C_SLAVE, &obf, 2) ;
    if (r != 2) {
        CLogger::Get()->Write("TOY", LogError, "ds3231_set_time seconds err r = %d", r) ;
        return false ;
    }

    obf[0] = 0x01 ;
	obf[1] = ((minutes % 10) & 017) | (((minutes / 10) & 7) << 4) ;
    r = pI2cMaster->Write(I2C_SLAVE, &obf, 2) ;
    if (r != 2) {
        CLogger::Get()->Write("TOY", LogError, "ds3231_set_time minutes err r = %d", r) ;
        return false ;
    }

    obf[0] = 0x02 ;
	obf[1] =  0100 | ((hours % 10) & 017) | (((hours / 10) & 3) << 4) ;
    r = pI2cMaster->Write(I2C_SLAVE, &obf, 2) ;
    if (r != 2) {
        CLogger::Get()->Write("TOY", LogError, "ds3231_set_time hours err r = %d", r) ;
        return false ;
    }

    obf[0] = 0x03 ;
	obf[1] = 1 ; // doesn't matter
    r = pI2cMaster->Write(I2C_SLAVE, &obf, 2) ;
    if (r != 2) {
        CLogger::Get()->Write("TOY", LogError, "ds3231_set_time weekdays err r = %d", r) ;
        return false ;
    }

    // DATE : AAMMMMDD DDDYYYYY

    u16 days = (dar >> 5) & 037 ;

    obf[0] = 0x04 ;
	obf[1] = ((days % 10) & 017) | (((days / 10) & 3) << 4) ;
    r = pI2cMaster->Write(I2C_SLAVE, &obf, 2) ;
    if (r != 2) {
        CLogger::Get()->Write("TOY", LogError, "ds3231_set_time days err r = %d", r) ;
        return false ;
    }

    u16 months = (dar >> 10) & 017 ;

    obf[0] = 0x05 ;
	obf[1] = ((months % 10) & 017) | (((months / 10) & 1) << 4) ;
    r = pI2cMaster->Write(I2C_SLAVE, &obf, 2) ;
    if (r != 2) {
        CLogger::Get()->Write("TOY", LogError, "ds3231_set_time months err r = %d", r) ;
        return false ;
    }

    u16 years = (dar & 037) + 1972 + ((dar >> 14) * 32) - 2000 ;

    obf[0] = 0x06 ;
	obf[1] = ((years % 10)  & 017) | (((years / 10) & 017) << 4) ;
    r = pI2cMaster->Write(I2C_SLAVE, &obf, 2) ;
    if (r != 2) {
        CLogger::Get()->Write("TOY", LogError, "ds3231_set_time years err r = %d", r) ;
    }

    return r == 2 ;
}

TOY::TOY() {
    csr = 0 ;
    dar = 0 ;
    tlr = 0 ;
    thr = 0 ;
}

u16 TOY::read16(const u32 a) {
    switch (a) {
        case TOY_CSR:
            return csr & 0100200 ;
            break ;
        case TOY_DAR:
            return dar ;
            break ;
        case TOY_TLR:
            return tlr ;
            break ;
        case TOY_THR:
            return thr ;
            break ;
        default:
            CLogger::Get()->Write("TOY", LogError, "read16 from invalid address %08o", a) ;
            cpu.errorRegister = 020 ;
            trap(INTBUS);
            return 0 ;
    }
}

void TOY::write16(const u32 a, const u16 v) {
    switch (a) {
        case TOY_CSR:
            csr = (csr & ~3) | (v & 3) ;
            if (csr & 3) {
                csr &= ~0100200 ;
            }
            break ;
        case TOY_DAR:
            dar = v ;
            break ;
        case TOY_TLR:
            tlr = v ;
            break ;
        case TOY_THR:
            thr = v ;
            break ;
        default:
            CLogger::Get()->Write("TOY", LogError, "write16 to invalid address %08o", a) ;
            cpu.errorRegister = 020 ;
            trap(INTBUS); // intbus
    }
}

void TOY::step() {
    if ((csr & 3) == 0) {
        return ;
    }

    // read
    if (csr & 1) {
        if (ds3231_read_time(buf)) {
            // DATE : AAMMMMDD DDDYYYYY
            int years = 2000 + (buf[YEARS] & 017) + ((buf[YEARS] >> 4) & 017) * 10 ;
            u8 age = 0 ;
            while (years > 2003) {
                years -= 32 ;
                age++ ;
            }

            years -= 1972 ;

            int days = (buf[DAYS] & 017) +  (((buf[DAYS] >> 4) & 3)) * 10 ;
            int months = (buf[MONTHS] & 017) + ((buf[MONTHS] >> 4) & 1) * 10 ;
        
            dar = ((age & 3) << 14) | (years & 037)  | ((days & 037) << 5) | ((months & 017) << 10) ;


            u32 seconds = (buf[SECONDS] & 017) + ((buf[SECONDS] >> 4) & 7) * 10 ;
            u32 minutes = (buf[MINUTES] & 017) + ((buf[MINUTES] >> 4) & 7) * 10 ;
            u32 hours = (buf[HOURS] & 017) + ((buf[HOURS] >> 4) & 3) * 10 ;
            u32 ticks = (seconds + minutes * 60 + hours * 3600) * 60 ;
            tlr = ticks & 0177777 ;
            thr = (ticks >> 16) & 0177777 ;
        } else {
            csr |= 0100000 ;
        }

        csr = (csr & ~1) | 0200 ;
        return ;
    }

    // write
    if (csr & 2) {
        if (!ds3231_set_time()) {
            csr |= 0100000 ;
        }
        csr = (csr & ~2) | 0200 ;
    }

}

void TOY::reset() {
    // u8 bbf[8] = {
    //     0x00, // regaddr
    //     00, // seconds
    //     026, // minutes
    //     023, // hours
    //     04, // weekdays
    //     025, // days
    //     02, // months
    //     045  // years
    // } ;
	// ds3231_set_time(bbf) ; 
}
