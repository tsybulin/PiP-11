//
// kernel.cpp
//
#include "kernel.h"

#include <circle/util.h>
#include <circle/sched/scheduler.h>
#include "ini.h"
#include "logo.h"
#include "firmware.h"

#define DRIVE "USB:"
extern volatile bool interrupted ;

typedef struct configuration {
    CString name;
    CString rk;
    CString rl;
} configuration_t ;

static configuration_t configurations[5] = {
    {
        "RK0: DEFAULT",
        DRIVE "/PIP-11/RK11_00.RK05",
        DRIVE "/PIP-11/RL11_00.RL02"
   }
} ;

extern queue_t keyboard_queue ;
CSerialDevice *pSerial ;
CI2CMaster *pI2cMaster ;

static int config_handler(void* user, const char* section, const char* name, const char* value) {
    int c = -1 ;

    if (*section && strlen(section) > 0) {
        for (int i = 0; i < 5; i++) {
            if (configurations[i].name.Compare(section) == 0) {
                c = i ;
                break;
            }
        }

        if (c < 0) {
            for (int i = 0; i < 5; i++) {
                if (configurations[i].name.GetLength() > 0) {
                    continue;
                }

                c = i ;
				
                configurations[i].name.Format("%s", section);
                break;
            }

            if (c < 0) {
                gprintf("too many configurations") ;
                while(!interrupted) ;
                return 0 ;
            }
        }

        if (strcmp(name, "RK") == 0) {
            configurations[c].rk.Format("%s", value);
        } else if (strcmp(name, "RL") == 0) {
            configurations[c].rl.Format("%s", value);
        }

        return 1;
    } else {
        gprintf("empty config section") ;
        while(!interrupted) ;
        return 0 ;
    }
}

CKernel::CKernel (void)
:	screen(options.GetWidth(), options.GetHeight()),
	timer(&interrupt),
	serial(&interrupt),
	logger(options.GetLogLevel()),
	cpuThrottle(CPUSpeedMaximum),
    usbhci(&interrupt, &timer, true),
	i2cMaster(1),

	console(&actLED, &deviceNameService, &interrupt, &timer),
	multiCore(CMemorySystem::Get(), &console, &cpuThrottle),
	screenBrightness(100)
{
	pSerial = &serial ;
	pI2cMaster = &i2cMaster ;

	unsigned sbr = 0 ;
	CKernelOptions* kops = CKernelOptions::Get() ;
	if (kops) {
		sbr = kops->GetBacklight() ;
		if (sbr > 0) {
			screenBrightness = sbr ;
		}
	}
}

CKernel::~CKernel (void) {
}

boolean CKernel::Initialize (void) {
	boolean bOK = TRUE ;

	if (bOK) {
		bOK = screen.Initialize() ;
	}

	if (bOK) {
		bOK = serial.Initialize(115200) ;
		serial.SetOptions(0) ;
	}

	if (bOK) {
		cpuThrottle.SetSpeed(CPUSpeedMaximum, false) ;
	}

	if (bOK) {
		CDevice *pTarget = deviceNameService.GetDevice(options.GetLogDevice(), FALSE) ;
		if (pTarget == 0) {
			pTarget = &screen;
		}

		bOK = logger.Initialize(pTarget) ;
	}

	if (bOK) {
		bOK = interrupt.Initialize();
	}

	if (bOK) {
		bOK = timer.Initialize();
	}

	if (bOK) {
		bOK = usbhci.Initialize ();

		if (bOK) {
			logger.Write("kernel", LogNotice, "waiting for keyboard") ;

			while (!console.hasKeyboard()) {
			    bool updated = usbhci.UpdatePlugAndPlay() ;
				if (updated) {
					CUSBKeyboardDevice *keyboard = (CUSBKeyboardDevice *) deviceNameService.GetDevice("ukbd1", FALSE) ;
					console.attachKeyboard(keyboard) ;
				}
			}

			logger.Write("kernel", LogNotice, "keyboard attached") ;
		}
	}

	if (bOK) {
		bOK = i2cMaster.Initialize() ;
	}

	if (bOK) {
		this->console.init(&this->screen) ;
	}

	return bOK ;
}

static volatile bool firmwareMode = false ;

bool CKernel::hotkeyHandler(const unsigned char modifiers, const unsigned char hid_key, void *context) {
	CKernel *pthis = (CKernel *)context ;

    if (
        modifiers & (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_RIGHTCTRL) &&
        modifiers & (KEYBOARD_MODIFIER_LEFTALT | KEYBOARD_MODIFIER_RIGHTALT) &&
        hid_key == HID_KEY_DELETE
    ) {
		pthis->console.shutdownMode = ShutdownReboot ;
        CMultiCoreSupport::SendIPI(0, IPI_USER) ;

		return true ;
    }

	if (!modifiers && hid_key == HID_KEY_F9) {
		if (pthis->screenBrightness > 50) {
			pthis->screenBrightness -= 10 ;
			pthis->screen.GetFrameBuffer()->SetBacklightBrightness(pthis->screenBrightness) ;
			iprintf("Brightness %d", pthis->screenBrightness) ;
		}
		return true ;
	}

	if (!modifiers && hid_key == HID_KEY_F10) {
		if (pthis->screenBrightness < 180) {
			pthis->screenBrightness += 10 ;
			pthis->screen.GetFrameBuffer()->SetBacklightBrightness(pthis->screenBrightness) ;
			iprintf("Brightness %d", pthis->screenBrightness) ;
		}
		return true ;
	}

	if (!modifiers && hid_key == HID_KEY_F12) {
		firmwareMode = true ;
		return true ;
	}

	return false ;
}

TShutdownMode CKernel::Run (void) {
	console.setHotkeyHandler(hotkeyHandler, this) ;
	console.vtCls() ;
	console.beep() ;
	CString txt ;
	txt.Format("PiP-11/45: " __DATE__ " " __TIME__ " %dx%d (%dx%d) (%dx%d)", screen.GetWidth(), screen.GetHeight(), screen.GetColumns(), screen.GetRows(), screen.getCharWidth(), screen.getCharHeight()) ;
	this->console.write(txt, 0, 6, CONS_TEXT_COLOR) ;

	if (FR_OK != f_mount(&fileSystem, DRIVE, 1)) {
		gprintf("USB error!") ;
		while (1) {
			TShutdownMode mode = this->console.loop() ;

			if (mode != ShutdownNone) {
				return mode ;
			}
		}
	}

	logger.Write("kernel", LogNotice, DRIVE " mounted") ;

    if (FRESULT res = ini_parse(DRIVE "/PIP-11/CONFIG.INI", config_handler, nullptr); res != FR_OK) {
        gprintf("Can't load 'PIP-11/CONFIG.INI' err %d", res);
		f_unmount(DRIVE) ;

        while (1) {
			TShutdownMode mode = this->console.loop() ;

			if (mode != ShutdownNone) {
				return mode ;
			}
		};
    }

	logger.Write("kernel", LogNotice, "CONFIG.INI parsed") ;

	int ci = 0 ;
	bool selected = false ;
	bool paused = false ;

	console.write("> SHOW CONFIGURATION", 0, 8, CONS_TEXT_COLOR) ;

	for (int y = 0; y < MODEL_HEIGHT; y++) {
		for (int x = 0; x < MODEL_WIDTH; x ++) {
			screen.SetPixel(x, y + 16, model[y * MODEL_WIDTH + x]) ;
		}
	}

	for (int y = 0; y < LOGO_HEIGHT; y++) {
		for (int x = 0; x < LOGO_WIDTH; x ++) {
			screen.SetPixel(options.GetWidth() - LOGO_WIDTH + x, y + 16, logo[y * LOGO_WIDTH + x]) ;
		}
	}
	
	unsigned int row = 10 ;

    for (int i = 0; i < 5; i++) {
		if (configurations[i].name.GetLength() == 0) {
			continue ;
		}

        CString tmp ;
		tmp.Format("%d. ", i) ;
		tmp.Append(configurations[i].name) ;
		console.write(tmp, 0, row++, i == 0 ? YELLOW_COLOR : CONS_TEXT_COLOR) ;
    }

	console.write("BOOT>", 0, ++row, CONS_TEXT_COLOR) ;

	unsigned int timeout = timer.GetTicks() + 600 ;

	while (!selected && (paused || timer.GetTicks() < timeout)) {
		TShutdownMode mode = this->console.loop() ;

		if (mode != ShutdownNone) {
			return mode ;
		}

		if (firmwareMode) {
			ci = 0 ;
			selected = false ;
			paused = true ;

			console.vtCls() ;
			console.write("FIRMWARE> ", 0, 0, CONS_TEXT_COLOR) ;

			CScheduler scheduler ;

			const u8 ipaddress[] = {172, 16, 103, 101} ;
			const u8 netmask[]   = {255, 255, 255, 0} ;
			const u8 gateway[]   = {172, 16, 103, 254} ;
			const u8 dns[]       = {172, 16, 103, 254} ;
			CNetSubSystem net(ipaddress, netmask, gateway, dns, "pip11") ;

			if (!net.Initialize()) {
				gprintf("Net initilize error") ;
				while(!interrupted) {} ;
				break;
			}

			console.write("> SHOW IP", 0, 2, CONS_TEXT_COLOR) ;

			CString ips ;
			net.GetConfig()->GetIPAddress()->Format(&ips);
			console.write(ips, 0, 4, CONS_TEXT_COLOR) ;

			new Firmware(&net, &fileSystem, DRIVE "/") ;
			while (!interrupted) {
				scheduler.Yield() ;
			}
		}

		if (!queue_is_empty(&keyboard_queue)) {
			unsigned char c ;
			if (!queue_try_remove(&keyboard_queue, &c)) {
				continue ;
			}

			if (!paused) {
				timeout = timer.GetTicks() + 600 ;
			}

			switch (c) {
				case '0':
				case '\n':
				case '\r':
					ci = 0 ;
					selected = true ;
					break;
				
				case '1':
					ci = 1 ;
					selected = true ;
					break;

				case '2':
					ci = 2 ;
					selected = true ;
					break;

				case '3':
					ci = 3 ;
					selected = true ;
					break;

				case '4':
					ci = 4 ;
					selected = true ;
					break;

				default:
					ci = 0 ;
					selected = false ;
					paused = true ;
					break;
			}
		}

		if (!paused) {
			txt.Format("BOOT %d s.>", (timeout - timer.GetTicks()) / 100) ;
			this->console.write(txt, 0, row, CONS_TEXT_COLOR) ;
		} else {
			this->console.write("BOOT>        ", 0, row, CONS_TEXT_COLOR) ;
		}
	}
	
	const char *rk   = configurations[ci].rk ;
	const char *rl   = configurations[ci].rl ;

	screen.ClearScreen() ;
	console.showStatus() ;
	console.showRusLat() ;

	multiCore.Initialize((char *)rk, (char *)rl, ci > 0) ;
	multiCore.Run(0) ;

	f_unmount(DRIVE) ;
	
	return console.shutdownMode ;
}
