//
// kernel.cpp
//
#include "kernel.h"

#include <circle/util.h>
#include "ini.h"
#include "logo.h"

#define DRIVE "SD:"
extern volatile bool interrupted ;

typedef struct configuration {
    CString name;
    CString rk;
    CString rl;
	CString boot;
} configuration_t ;

static configuration_t configurations[5] = {
    {
        "DEFAULT",
        "PIP-11/BOOT.RK05",
        "PIP-11/WORK.RL02",
		"RK"
    }
} ;

extern queue_t keyboard_queue ;
TShutdownMode startup(const char *rkfile, const char *rlfile, int bootdev) ;

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
        } else if (strcmp(name, "BOOT") == 0) {
			configurations[c].boot.Format("%s", value) ;
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
	logger(options.GetLogLevel (), &timer),
	cpuThrottle(CPUSpeedMaximum),
	emmc(&interrupt, &timer, &actLED),

	console(&actLED, &deviceNameService, &interrupt, &timer),
	multiCore(CMemorySystem::Get(), &console, &cpuThrottle)
{
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
		this->console.init(&this->screen) ;
	}

	if (bOK) {
		multiCore.Initialize() ;
	}

	return bOK ;
}

TShutdownMode CKernel::Run (void) {
	console.vtCls() ;
	CString txt ;
	txt.Format("PiP-11: %dx%d (%dx%d) (%dx%d)", screen.GetWidth(), screen.GetHeight(), screen.GetColumns(), screen.GetRows(), screen.getCharWidth(), screen.getCharHeight()) ;
	this->console.write(txt, 0, 24, GREEN_COLOR) ;

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

	console.drawLine(" BOOT ", 5) ;

	for (int y = 0; y < LOGO_HEIGHT; y++) {
		for (int x = 0; x < LOGO_WIDTH; x ++) {
			screen.SetPixel(x, y + 16, logo[y * LOGO_WIDTH + x]) ;
		}
	}
	
    for (int i = 0; i < 5; i++) {
		if (configurations[i].name.GetLength() == 0) {
			continue ;
		}

        CString tmp ;
		tmp.Format("%d. ", i) ;
		tmp.Append(configurations[i].boot) ;
		tmp.Append("0: ") ;
		tmp.Append(configurations[i].name) ;
		console.write(tmp, 20, 7 + i, i == 0 ? BRIGHT_WHITE_COLOR : WHITE_COLOR) ;
    }
    
	console.drawLine(nullptr, 13) ;

	unsigned int timeout = timer.GetTicks() + 600 ;

	while (!selected && (paused || timer.GetTicks() < timeout)) {
		TShutdownMode mode = this->console.loop() ;

		if (mode != ShutdownNone) {
			return mode ;
		}

		if (!queue_is_empty(&keyboard_queue)) {
			char c ;
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
					ci = 0 ;
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
			txt.Format("Boot in %d s.", (timeout - timer.GetTicks()) / 100) ;
			this->console.write(txt, 34, 14, BRIGHT_WHITE_COLOR) ;
		} else {
			this->console.write("AutoBoot paused      ", 34, 14, BRIGHT_WHITE_COLOR) ;
		}
	}
	
	const char *rk   = configurations[ci].rk ;
	const char *rl   = configurations[ci].rl ;
	const char *boot = configurations[ci].boot ;

	screen.ClearScreen() ;
	console.showStatus() ;
	console.showRusLat() ;

	TShutdownMode mode = startup(rk, rl, strncmp(boot, "RL", 2) == 0 ? 1 : 0) ;
	
	return mode ;
}
