//
// kernel.cpp
//
#include "kernel.h"

#include <circle/util.h>
#include <circle/sched/scheduler.h>
#include "ini.h"
#include "logo.h"
#include "firmware.h"

#define DRIVE "SD:"
extern volatile bool interrupted ;
extern volatile bool core3active ;

typedef struct configuration {
    CString name;
    CString rk;
    CString rl;
} configuration_t ;

static configuration_t configurations[5] = {
    {
        "RK0: DEFAULT",
        "PIP-11/BOOT.RK05",
        "PIP-11/WORK.RL02"
   }
} ;

extern queue_t keyboard_queue ;
TShutdownMode startup(const char *rkfile, const char *rlfile, const bool bootmon) ;
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
	emmc(&interrupt, &timer, &actLED),
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
		bOK = emmc.Initialize ();
	}

	if (bOK) {
		bOK = i2cMaster.Initialize() ;
	}

	if (bOK) {
		this->console.init(&this->screen) ;
	}

	if (bOK) {
		multiCore.Initialize() ;
	}

	return bOK ;
}

TShutdownMode CKernel::Run (void) {
	console.vtCls() ;
	console.beep() ;
	CString txt ;
	txt.Format("PiP-11/45: " __DATE__ " " __TIME__ " %dx%d (%dx%d) (%dx%d)", screen.GetWidth(), screen.GetHeight(), screen.GetColumns(), screen.GetRows(), screen.getCharWidth(), screen.getCharHeight()) ;
	this->console.write(txt, 0, 6, CONS_TEXT_COLOR) ;

	if (FR_OK != f_mount(&fileSystem, DRIVE, 1)) {
		gprintf("SD Card not inserted or SD Card error!") ;
		while (1) {
			TShutdownMode mode = this->console.loop() ;

			if (mode != ShutdownNone) {
				return mode ;
			}
		}
	}

    if (FRESULT res = ini_parse("PIP-11/CONFIG.INI", config_handler, nullptr); res != FR_OK) {
        gprintf("Can't load 'PIP-11/CONFIG.INI' err %d", res);
		f_unmount(DRIVE) ;

        while (1) {
			TShutdownMode mode = this->console.loop() ;

			if (mode != ShutdownNone) {
				return mode ;
			}
		};
    }

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
		console.write(tmp, 0, row++, i == 0 ? GREEN_COLOR : CONS_TEXT_COLOR) ;
    }

    
	console.write("BOOT>", 0, ++row, CONS_TEXT_COLOR) ;

	unsigned int timeout = timer.GetTicks() + 600 ;

	while (!selected && (paused || timer.GetTicks() < timeout)) {
		TShutdownMode mode = this->console.loop() ;

		if (mode != ShutdownNone) {
			return mode ;
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

				case KEY_F10:
					if (screenBrightness < 180) {
						screenBrightness += 10 ;
						screen.GetFrameBuffer()->SetBacklightBrightness(screenBrightness) ;
						iprintf("Brightness %d", screenBrightness) ;
					}
					break;

				case KEY_F9:
					if (screenBrightness > 50) {
						screenBrightness -= 10 ;
						screen.GetFrameBuffer()->SetBacklightBrightness(screenBrightness) ;
						iprintf("Brightness %d", screenBrightness) ;
					}
					break;

				case KEY_F11: {
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

						new Firmware(&net, &fileSystem) ;
						while (!interrupted) {
							scheduler.Yield() ;
						}
					}
					break ;

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

	core3active = true ;

	TShutdownMode mode = startup(rk, rl, ci > 0) ;

	f_unmount(DRIVE) ;
	
	return mode ;
}
