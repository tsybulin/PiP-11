#include "dl11.h"

#include "kb11.h"
#include <circle/serial.h>

extern KB11 cpu;

extern volatile bool interrupted ;
extern CSerialDevice *pSerial ;

DL11::DL11() {
}

void DL11::clearterminal() {
	rcsr = 0;
	xcsr = 0x80;
	rbuf = 0;
	xbuf = 0;
}

static char dl11_char = '\0' ;

static int _kbhit() {
	char c ;
	int n = pSerial->Read(&c, 1) ;
	if (n <= 0) {
		return false ;
	}

	dl11_char = c ;

	return true ;
}

void DL11::serial_putchar(char c) {
	pSerial->Write(&c, 1) ;
}

char DL11::serial_getchar() {
	return dl11_char ;
}

void DL11::poll() {
	if (!rcvrdone()) {
		// unit not busy
		if (_kbhit()) {
			char ch = serial_getchar();
			if (true) {
				rbuf = ch & 0x7f;
				rcsr |= 0x80;
				if (rcsr & 0x40) {
					cpu.interrupt(INTDLR, 4);
				}
			}
		}
	}


	if (xbuf) {
		xcsr |= 0x80;
		xbuf = 0;
		if (xcsr & 0x40) {
			cpu.interrupt(INTDLT, 4);
		}
	}
	else {
		if (iflag == 1) {
			cpu.interrupt(INTDLT, 4);
			iflag = 2;
		}
	}
}

u16 DL11::read16(u32 a) {
	switch (a & 7) {
	case 00:
		return rcsr;
	case 02:
		rcsr &= ~0x80;
		return rbuf;
	case 04:
		return xcsr;
	case 06:
		return xbuf;
	default:
		gprintf("Dl11: read from invalid address %06o\n", a);
		trap(INTBUS);
		while(!interrupted) {};
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
			if ((xcsr & 0300) == 0300 && iflag == 0)
				iflag = 1;
			if (iflag == 2) {
				iflag = 0;
			}
		}
			break;
		case 06:
		xbuf = v & 0x7f;
		serial_putchar(v & 0x7f);
		xbuf |= 0200; // Allow for nulls !!!!
		xcsr &= ~0x80;
		iflag = 0;
		break;
	default:
		gprintf("Dl11: write to invalid address %06o\n", a);
		trap(INTBUS);
	}
}

