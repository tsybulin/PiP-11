#include "kl11.h"

#include "kb11.h"

extern "C" {
#include <cons/cons.h>
}


extern KB11 cpu;

extern volatile bool interrupted ;
extern queue_t keyboard_queue ;
extern queue_t console_queue ;

KL11::KL11()
{
}

void KL11::clearterminal()
{
	rcsr = 0;
	xcsr = 0x80;
	rbuf = 0;
	xbuf = 0;
}

static int _kbhit() {
	return !queue_is_empty(&keyboard_queue) ;
}

void KL11::serial_putchar(char c) {
	// Console::get()->putCharVT100(c) ;
	queue_try_add(&console_queue, &c) ;
}

char KL11::serial_getchar() {
	char c ;
	if (queue_try_remove(&keyboard_queue, &c)) {
		return c ;
	}
	
	return '\0' ;
}

void KL11::poll() {
	if (!rcvrdone()) {
		// unit not busy
		if (_kbhit()) {
			char ch;
			if ((ch = KL11::serial_getchar())) {
				rbuf = ch & 0x7f;
				rcsr |= 0x80;
				if (rcsr & 0x40) {
					cpu.interrupt(INTTTYIN, 4);
				}
			}
		}
	}

	if (xbuf)
	{
		xcsr |= 0x80;
		xbuf = 0;
		if (xcsr & 0x40)
		{
			cpu.interrupt(INTTTYOUT, 4);
		}
	} 
	else {
		if (iflag == 1) {
			cpu.interrupt(INTTTYOUT, 4);
			iflag = 2;
		}
	}
}

u16 KL11::read16(u32 a)
{

	switch (a)
	{
	case 0777560:
		return rcsr;
	case 0777562:
		rcsr &= ~0x80;
		return rbuf;
	case 0777564:
		return xcsr;
	case 0777566:
		return xbuf;
	default:
		// printf("kl11: read from invalid address %06o\n", a);
		trap(INTBUS);
		return 0 ;
	}
}

void KL11::write16(u32 a, u16 v)
{
	switch (a)
	{
	case 0777560:
		rcsr = ((rcsr & 0200) ^ (v & ~0200));
		break;
	case 0777562:
		rcsr &= ~0x80;
		break;
	case 0777564:
		xcsr = ((xcsr & 0200) ^ (v & ~0200));
		if ((xcsr & 0300) == 0300 && iflag == 0)
			iflag = 1;
		if (iflag == 2)
			iflag = 0;
		break;
	case 0777566:
		xbuf = v & 0x7f;
		serial_putchar(xbuf) ;
		xbuf |= 0200; // Allow for nulls !!!!
		xcsr &= ~0x80;
		iflag = 0;
		break;
	default:
		gprintf("kl11: write to invalid address %06o\n", a);
		while(!interrupted);
	}
}
