#include "pc11.h"

#include "arm11.h"
#include "kb11.h"

#include <circle/i2cmaster.h>
#include <cons/cons.h>

#define I2C_SLAVE 050
const u8 PC11_I2C_PRS = 050 ;
const u8 PC11_I2C_PRB = 052 ;
const u8 PC11_I2C_PPS = 054 ;
const u8 PC11_I2C_PPB = 056 ;
const u8 PC11_I2C_RST = 060 ;

extern volatile bool interrupted ;
extern KB11 cpu;
extern CI2CMaster *pI2cMaster ;

int ptp_delay = 0 ;
int ptr_delay = 0 ;

static u16 pc11_i2c_read(const u8 addr) {
    u8 result[3] = {0, 0, 0} ;
    int r = pI2cMaster->WriteReadRepeatedStart(I2C_SLAVE, &addr, 1, result, 3) ;
    if (r != 3 || !result[2]) {
        gprintf("pc11_i2c_read: a=%03o, r=%d, ret=%03o", addr, r, result[2]) ;
        return 0 ;
    }

    return result[0] | (result[1] << 8) ;
}

static void pc11_i2c_write(const u8 addr, const u16 v) {
    u8 request[3] = {(u8)(addr | 0100), (u8)(v & 0377), (u8)((v >> 8) & 0377)} ;
    u8 result = 0 ;
    int r = pI2cMaster->WriteReadRepeatedStart(I2C_SLAVE, request, 3, &result, 1) ;
    if (r != 1 || !result) {
        gprintf("pc11_i2c_write: a=%03o, v=%06o, r=%d, ret=%03o", addr, r, result) ;
    }
}

u16 PC11::read16(const u32 a) {
    switch (a) {
        case PC11_PRS:
            return pc11_i2c_read(PC11_I2C_PRS) ;
        case PC11_PRB:
            return pc11_i2c_read(PC11_I2C_PRB) ;
        case PC11_PPS:
            return pc11_i2c_read(PC11_I2C_PPS) ;
        case PC11_PPB:
            return pc11_i2c_read(PC11_I2C_PPB) ; ;
        default:
            break ;
    }
    return 0 ;
}

void PC11::write16(const u32 a, const u16 v) {
    switch (a) {
        case PC11_PRS:
            pc11_i2c_write(PC11_I2C_PRS, v) ;
            if (v & 01) {
                ptr_delay = 0 ;
                ptrcheck = true ;
            }
            break;
        case PC11_PRB:
            break ; //read-only
        case PC11_PPS: {
                pc11_i2c_write(PC11_I2C_PPS, v) ;
                u16 pps = pc11_i2c_read(PC11_I2C_PPS) ;
                if ((pps & 0100) && (pps & 0100200)) {
                    cpu.interrupt(INTPTP, 5) ;
                } else {
                    cpu.clearIRQ(INTPTP) ;
                }
            }
            break ;
        case PC11_PPB:
            pc11_i2c_write(PC11_I2C_PPB, v) ;
            ptp_delay = 0 ;
            ptpcheck = true ;
            break;
        default:
            gprintf("pc11::write16 invalid write to %06o\n", a);
            trap(004);
            while(!interrupted) {}
    }
}

void PC11::reset() {
    ptrcheck = false ;
    ptpcheck = false ;
    pI2cMaster->Write(I2C_SLAVE, &PC11_I2C_RST, 1) ;
}

void PC11::step() {
    ptr_step() ;
    ptp_step() ;
}

void PC11::ptr_step() {
    if (!ptrcheck) {
        return ;
    }

    if (ptr_delay++ < 200) {
        return ;
    }

    ptr_delay = 0 ;

    u16 prs = pc11_i2c_read(PC11_I2C_PRS) ;
    if (prs & 0200) {
        ptrcheck = false ;
        if (prs & 0100) {
            cpu.interrupt(INTPTR, 4) ;
        } else {
            cpu.clearIRQ(INTPTR) ;
        }
    }
}

void PC11::ptp_step() {
    if (!ptpcheck) {
        return ;
    }

    if (ptp_delay++ < 75000) {
        return ;
    }

    ptp_delay = 0 ;

    u16 pps = pc11_i2c_read(PC11_I2C_PPS) ;
    if (pps & 0200) {
        ptpcheck = false ;

        if (pps & 0100) {
            cpu.interrupt(INTPTP, 4) ;
        } else {
            cpu.clearIRQ(INTPTP) ;
        }
    }
}
