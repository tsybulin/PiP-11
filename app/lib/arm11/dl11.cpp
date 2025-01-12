#include "dl11.h"

#include "kb11.h"
#include <circle/serial.h>

extern KB11 cpu;

extern volatile bool interrupted ;
// extern CSerialDevice *pSerial ;

DL11::DL11() {
}

void DL11::clearterminal() {
	rcsr = 0;
	xcsr = 0200;
	rbuf = 0;
	xbuf = 0;
}

static char dl11_char = '\0' ;

static int _kbhit() {
	return false ;
	// char c ;
	// int n = pSerial->Read(&c, 1) ;
	// if (n <= 0) {
	// 	return false ;
	// }

	// dl11_char = c ;

	// return true ;
}

u16 DL11::read16(u32 a) {
	switch (a & 7) {
		case 00:
			return rcsr;
		case 02:
			rcsr &= ~0200;
			return rbuf;
		case 04:
			return xcsr;
		case 06:
			return xbuf;
		default:
			gprintf("Dl11: read from invalid address %06o\n", a);
			cpu.errorRegister |= 020 ;
			trap(INTBUS);
			return 0 ;
	}
}

void DL11::write16(u32 a, u16 v) {
	switch (a & 7) {
		case 00:
			rcsr = ((rcsr & 0200) ^ (v & ~0200));
			break;
		case 02:
			//rcsr &= ~0x80;
			break;
		case 04: {
			xcsr = ((xcsr & 0200) ^ (v & ~0200));
			if ((xcsr & 0200) && (xcsr & 0100)) {
				cpu.interrupt(INTDLT, 4);
			} else {
				cpu.clearIRQ(INTDLT) ;
			}
		}
			break;
		case 06:
			xbuf = v & 0177;
			xbuf |= 0200; // Allow for nulls !!!!
			xcsr &= ~0200;
			break;
		default:
			gprintf("Dl11: write to invalid address %06o\n", a);
			cpu.errorRegister |= 020 ;
			trap(INTBUS);
	}
}

void DL11::rpoll() {
	if (rcsr & 0200) {
		return ;
	}

	if (_kbhit()) {
		rbuf = dl11_char & 0177 ;
		rcsr |= 0200;
		if (rcsr & 0100) {
			cpu.interrupt(INTDLR, 4);
		} else {
			cpu.clearIRQ(INTDLR) ;
		}
	}
}

void DL11::xpoll() {
	if (xcsr & 0200) {
		return ;
	}

	if (xbuf) {
		// char c = xbuf & 0177 ;
		// pSerial->Write(&c, 1) ;
	}
	
	xcsr |= 0200;
	xbuf = 0;
	if (xcsr & 0100) {
		cpu.interrupt(INTDLT, 4);
	} else {
		cpu.clearIRQ(INTDLT) ;
	}
}

