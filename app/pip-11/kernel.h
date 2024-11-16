//
// kernel.h
//
#ifndef _kernel_h
#define _kernel_h

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/types.h>
#include <circle/cputhrottle.h>
#include <cons/cons.h>
#include <sdcard/emmc.h>
#include <fatfs/ff.h>
#include <circle/i2cmaster.h>

#include "mcore.h"

class CKernel {
	public:
		CKernel(void) ;
		~CKernel(void) ;

		boolean Initialize(void) ;

		TShutdownMode Run(void);


		static bool hotkeyHandler(const unsigned char modifiers, const unsigned char hid_key, void *context) ;
	private:
		// do not change this order
		CActLED				actLED;
		CKernelOptions		options;
		CDeviceNameService	deviceNameService;
		CScreenDevice		screen;
		CExceptionHandler	exceptionHandler;
		CInterruptSystem	interrupt;
		CTimer				timer;
		CSerialDevice		serial;
		CLogger				logger;
		CCPUThrottle		cpuThrottle;
		CEMMCDevice		    emmc;
		FATFS				fileSystem ;
		CI2CMaster    		i2cMaster ;

		Console				console ;
		MultiCore			multiCore ;
		unsigned            screenBrightness ;

} ;

#endif
