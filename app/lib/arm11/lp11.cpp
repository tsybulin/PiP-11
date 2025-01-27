#include "lp11.h"

#include "kb11.h"
#include <circle/i2cmaster.h>
#include <cons/cons.h>

#define I2C_SLAVE 050
const u8 LP11_I2C_LPS = 014 ;
const u8 LP11_I2C_LPB = 016 ;

extern KB11 cpu;
extern volatile bool interrupted ;
extern CI2CMaster *pI2cMaster ;
int lp11_delay = 0 ;

static u16 lp11_i2c_read(const u8 addr) {
    u8 result[3] = {0, 0, 0} ;
    int r = pI2cMaster->WriteReadRepeatedStart(I2C_SLAVE, &addr, 1, result, 3) ;
    if (r != 3 || !result[2]) {
        gprintf("lp11_i2c_read: a=%03o, r=%d, ret=%03o", addr, r, result[2]) ;
        return 0 ;
    }

    return result[0] | (result[1] << 8) ;
}

static void lp11_i2c_write(const u8 addr, const u16 v) {
    u8 request[3] = {(u8)(addr | 0100), (u8)(v & 0377), (u8)((v >> 8) & 0377)} ;
    u8 result = 0 ;
    int r = pI2cMaster->WriteReadRepeatedStart(I2C_SLAVE, request, 3, &result, 1) ;
    if (r != 1 || !result) {
        gprintf("lp11_i2c_write: a=%03o, v=%06o, r=%d, ret=%03o", addr, r, result) ;
    }
}

u16 LP11::read16(const u32 a) {
    switch (a) {
    case LP11_LPS:
        return lp11_i2c_read(LP11_I2C_LPS) ;
    case LP11_LPD:
        return lp11_i2c_read(LP11_I2C_LPB) ;
    default:
        gprintf("lp11: read from invalid address %06o\n", a);
        while(!interrupted) {}
        return 0 ;
    }
}

void LP11::write16(const u32 a, const u16 v) {
    switch (a) {
        case LP11_LPS: {
                lp11_i2c_write(LP11_I2C_LPS, v) ;
                u16 lps = lp11_i2c_read(LP11_I2C_LPS) ;
                if ((lps & 0100) && (lps & 0100200)) {
                    cpu.interrupt(INTLP, 4);
                } else {
                    cpu.clearIRQ(INTLP) ;
                }
            }
            break;
        case LP11_LPD:
            lp11_i2c_write(LP11_I2C_LPB, v) ;
            lp11_delay = 0 ;
            lpcheck = true ;
            break;
        default:
            gprintf("lp11: write to invalid address %06o", a);
            trap(004);
            while(!interrupted) {}
    }
}

void LP11::reset() {
    lpcheck = false ;
}

void LP11::step() {
    if (!lpcheck) {
        return ;
    }

    if (lp11_delay++ < 200) {
        return ;
    }

    lp11_delay = 0 ;

    u16 lps = lp11_i2c_read(LP11_I2C_LPS) ;
    if (lps & 0200) {
        lpcheck = false ;

        if (lps & 0100) {
            cpu.interrupt(INTLP, 4) ;
        }
    }
}
