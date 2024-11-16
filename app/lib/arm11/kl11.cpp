#include "kl11.h"

#include "kb11.h"

extern KB11 cpu;

extern volatile bool interrupted ;
extern queue_t keyboard_queue ;
extern queue_t console_queue ;

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
			}
			break;

		case KL11_XBUF:
			xbuf = (v & 0177) | 0400 ;
			xcsr &= ~0200 ;
			break ;

		default:
			gprintf("kl11: write to invalid address %06o\n", a);
			while(!interrupted);
	}
}

// #define KL11_DEBUG

void KL11::xpoll() {
	if (xcsr & 0200) {
		return ;
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
		if (!queue_try_add(&console_queue, &c)) {
			CLogger::Get()->Write("KL11::xpoll", LogError, "console queue is full") ;
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
	}
}

void KL11::rpoll() {
	if (rcsr & 0200) {
		return ;
	}

	u8 c;
	if (queue_try_remove(&keyboard_queue, &c)) {
		rbuf = c & 0377 ;
		rcsr |= 0200 ;
		if (rcsr & 0100) {
			cpu.interrupt(INTTTYIN, 4);
		}
	}
}

