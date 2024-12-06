//
// kernel.cpp
//
#include "kernel.h"

#include <circle/util.h>
#include "ini.h"
#include "logo.h"
#include "firmware.h"
#include "api.h"

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

CSerialDevice *pSerial ;
CI2CMaster *pI2cMaster ;

static const u8 ipaddress[] = {172, 16, 103, 101} ;
static const u8 netmask[]   = {255, 255, 255, 0} ;
static const u8 gateway[]   = {172, 16, 103, 254} ;
static const u8 dns[]       = {172, 16, 103, 254} ;

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
	logger(options.GetLogLevel(), &timer),
	cpuThrottle(CPUSpeedMaximum),
	net(ipaddress, netmask, gateway, dns, "pip11"),
    usbhci(&interrupt, &timer, true),
	i2cMaster(1),

	console(),
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
	}

	if (bOK) {
		bOK = net.Initialize() ;

		if (bOK) {
			CString ips ;
			net.GetConfig()->GetIPAddress()->Format(&ips);
			logger.Write("CKernel::Initialize", LogError, ips) ;
		} else {
			logger.Write("CKernel::Initialize", LogError, "net init") ;
		}
	}

	if (bOK) {
		bOK = i2cMaster.Initialize() ;
	}

	if (bOK) {
		this->console.init() ;
	}

	return bOK ;
}

static volatile bool firmwareMode = false ;

TShutdownMode CKernel::Run (void) {
	CString txt ;
	txt.Format("\033[H\033[JPiP-11/45: " __DATE__ " " __TIME__ " %dx%d (%dx%d) (%dx%d)\r\n\r\n", screen.GetWidth(), screen.GetHeight(), screen.GetColumns(), screen.GetRows(), screen.getCharWidth(), screen.getCharHeight()) ;
	this->console.sendString(txt) ;

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

	console.sendString("> SHOW CONFIGURATION\r\n\r\n") ;

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
	
    for (int i = 0; i < 5; i++) {
		if (configurations[i].name.GetLength() == 0) {
			continue ;
		}

        CString tmp ;
		tmp.Format("    %d. ", i) ;
		tmp.Append(configurations[i].name) ;
		tmp.Append("\r\n") ;
		console.sendString(tmp) ;
    }

	console.sendString("    9. FIRMWARE UPDATE\r\n") ;

	console.sendString("\033[1G\033[15dBOOT>") ;

	unsigned int timeout = timer.GetTicks() + 600 ;
	int pt = 666 ;
	int t = 0 ;

	while (!selected && (paused || timer.GetTicks() < timeout)) {
		TShutdownMode mode = this->console.loop() ;

		if (mode != ShutdownNone) {
			return mode ;
		}

		if (firmwareMode) {
			ci = 0 ;
			selected = false ;
			paused = true ;

			console.sendString("\033[H\033[JFIRMWARE>") ;

			new Firmware(&net, &fileSystem, DRIVE "/") ;
			unsigned char c ;
			while (!interrupted) {
				scheduler.Yield() ;
				int n = serial.Read(&c, 1) ;
				if (n > 0) {
					console.shutdownMode = ShutdownReboot ;
					interrupted = true ;
				}
			}
		}

		unsigned char c ;
		int n = serial.Read(&c, 1) ;
		if (n > 0) {
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

				case '9':
					firmwareMode = true ;
					break ;

				default:
					ci = 0 ;
					selected = false ;
					paused = true ;
					break;
			}
		}

		t = (timeout - timer.GetTicks()) / 100 ;
		if (pt != t) {
			pt = t ;
			this->console.sendString("\033[1G\033[15d\033[K") ;
			if (!paused) {
				txt.Format("BOOT %d s.>", t) ;
				this->console.sendString(txt) ;
			} else {
				this->console.sendString("BOOT>") ;
			}
		}

		scheduler.Yield() ;
	}
	
	const char *rk   = configurations[ci].rk ;
	const char *rl   = configurations[ci].rl ;

	screen.ClearScreen() ;
	this->console.sendString("\033[H\033[J") ;

	multiCore.Initialize((char *)rk, (char *)rl, ci > 0) ;
	// API * api = new API(&net) ;
    // api->init() ;
	multiCore.Run(0) ;

	f_unmount(DRIVE) ;
	
	return console.shutdownMode ;
}
