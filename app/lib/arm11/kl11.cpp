#include "kl11.h"

#include "kb11.h"
#include <circle/logger.h>
#include <circle/serial.h>

#ifndef ARM_ALLOW_MULTI_CORE
#define ARM_ALLOW_MULTI_CORE
#endif

#include <circle/multicore.h>

extern KB11 cpu;

extern volatile bool interrupted ;
extern CSerialDevice *pSerial ;

static char kl11_char = '\0' ;

static int _kbhit() {
	char c ;
	int n = pSerial->Read(&c, 1) ;
	if (n <= 0) {
		return false ;
	}

	kl11_char = c ;

	return true ;
}

KL11::KL11() {
}

void KL11::clearterminal() {
	rcsr = 0;
	xcsr = 0200 ; // transmitter ready
	rbuf = 0;
	xbuf = 0;
}

u16 KL11::read16(u32 a) {
	switch (a) {
		case KL11_RCSR:
			return rcsr;
		case KL11_RBUF:
			rcsr &= ~0200 ;
			return rbuf;
		case KL11_XCSR:
			return xcsr;
		case KL11_XBUF:
			return xbuf & 0377 ;
		default:
			cpu.errorRegister = 020 ;
			trap(INTBUS);
			return 0 ;
	}
}

void KL11::write16(u32 a, u16 v) {
	switch (a) {
		case KL11_RCSR:
			rcsr = ((rcsr & 0200) ^ (v & ~0200));
			break;
		case KL11_RBUF:
			rcsr &= ~0200;
			break;

		case KL11_XCSR:
			xcsr = ((xcsr & 0200) ^ (v & ~0200));
			if ((xcsr & 0200) && (xcsr & 0100)) {
				cpu.interrupt(INTTTYOUT, 4);
			} else {
				cpu.clearIRQ(INTTTYOUT) ;
			}
			break;

		case KL11_XBUF:
			xbuf = (v & 0177) | 0400 ;
			xcsr &= ~0200 ;
			break ;

		default:
			gprintf("kl11: write to invalid address %06o\n", a);
			cpu.errorRegister = 020 ;
			trap(INTBUS);
	}
}

void KL11::rpoll() {
	if (rcsr & 0200) {
		return ;
	}

	if (_kbhit()) {
		rbuf = kl11_char & 0377 ;
		if (rbuf & 0200) {
			switch (rbuf) {
				case 0201:
					CMultiCoreSupport::SendIPI(0, IPI_USER) ;
					return ;
				case 0202:
					CMultiCoreSupport::SendIPI(0, IPI_USER + 1) ;
					return ;
				case 0203:
					CMultiCoreSupport::SendIPI(0, IPI_USER + 2) ;
					return ;
				default:
				    CLogger::Get()->Write("KL11", LogError, "unknown control character %03o", rbuf) ;
					return ;
				}
		}
		rcsr |= 0200;
		if (rcsr & 0100) {
			cpu.interrupt(INTTTYIN, 4);
		} else {
			cpu.clearIRQ(INTTTYIN) ;
		}
	}

}

static u64 lx = 0 ; 

// #define KL11_DEBUG

void KL11::xpoll() {
	if (xcsr & 0200) {
		return ;
	}

	// rate output limit to about 28800 bit/s
	u64 t = CTimer::GetClockTicks64() ;

	if (lx == 0) {
		lx = t ;
	} else {
		if (t - lx < 315lu) {
			if (!(xcsr & 0100)) {
				cpu.clearIRQ(INTTTYOUT) ;
			}
			return ;
		}

		lx = t ;
	}


#ifdef KL11_DEBUG
	static FIL ft ;

	if (ft.obj.fs == nullptr) {
		FRESULT fr = f_open(&ft, "TTYOUT.TXT", FA_WRITE | FA_CREATE_ALWAYS) ;
		if (fr != FR_OK && fr != FR_EXIST) {
			gprintf("kl11 f_open %d", fr) ;
		} else {
			f_truncate(&ft) ;
			f_lseek(&ft, 0) ;
		}
	}
#endif

	if (xbuf) {
		u8 c = xbuf & 0377 ;
		if (pSerial->Write(&c, 1) != 1) {
			// CLogger::Get()->Write("KL11", LogError, "xpoll send error") ;
			return ;
		}

#ifdef KL11_DEBUG
		UINT bw;
		f_write(&ft, &c, 1, &bw) ;
		f_sync(&ft) ;
#endif

		xbuf = 0 ;
	}

	xcsr |= 0200 ;
	
	if (xcsr & 0100) {
		cpu.interrupt(INTTTYOUT, 4);
	} else {
		cpu.clearIRQ(INTTTYOUT) ;
	}
}
