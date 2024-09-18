#include <cons/cons.h>
#include <circle/string.h>
#include <circle/util.h>
#include "status.h"

Console *Console::pthis = 0 ;

queue_t keyboard_queue ;
queue_t console_queue ;

Console::Console(CActLED *actLED, CDeviceNameService *deviceNameService, CInterruptSystem *interrupt, CTimer *timer) :
    shutdownMode(ShutdownNone),
    usbhci(interrupt, timer, true),
    keyboard(0),
    koi7n1(false)
{
    pthis = this ;

    this->actLED = actLED ;
    this->deviceNameService = deviceNameService ;
    this->interrupt = interrupt ;
    this->timer = timer ;
    this->led_ticks = 0 ;
}

Console::~Console(void) {
    pthis = 0 ;
}

void Console::init(CScreenDevice *screen) {
    this->screen = screen ;
    usbhci.Initialize() ;
    queue_init(&keyboard_queue, 1, 16) ;
    queue_init(&console_queue, 1, 256) ;

    charset_G0 = CS_TEXT;
    charset_G1 = CS_GRAPHICS;
    saved_charset_G0 = CS_TEXT;
    saved_charset_G1 = CS_GRAPHICS;
    charset = &charset_G0;
    koi7n1 = false ;

    bool updated = this->usbhci.UpdatePlugAndPlay() ;
    if (updated && this->keyboard == 0) {
        keyboard = (CUSBKeyboardDevice *) this->deviceNameService->GetDevice("ukbd1", FALSE) ;
        if (keyboard != 0) {
            keyboard->RegisterRemovedHandler(keyboardRemovedHandler) ;
            keyboard->RegisterKeyStatusHandlerRaw(keyStatusHandler) ;
        }
    }
}

void Console::write(const char *buffer, unsigned col, unsigned row, TScreenColor fg, TScreenColor bg) {
    while (*buffer) {
        this->screen->displayChar(*buffer++, col, row, fg, bg) ;
        if (++col > this->screen->GetColumns() - 1) {
            col = 0 ;
            if (++row > this->screen->GetRows() - 1) {
                row = 0 ;
            }
        }
    }
}

void Console::drawLine(const char* title, unsigned row) {
    char line[screen->GetColumns() + 1] ;
    memset(&line, 0, this->screen->GetColumns() + 1) ; 
    memset(&line, title ? 0xCD : 0xC4, this->screen->GetColumns()) ; 
    write(line, 0, row);

    if (!title) {
        return ;
    }

    CString tmp ;
    tmp.Format(" %s ", title) ;
    write(tmp, (this->screen->GetColumns() - tmp.GetLength()) / 2, row, GREEN_COLOR) ;
}

void Console::sendChar(char c) {
    queue_try_add(&keyboard_queue, &c) ;
}

void Console::sendString(const char *str) {
    while (*str) {
        queue_try_add(&keyboard_queue, str++) ;
    }
}

void Console::sendEscapeSequence(u8 key) {
    this->sendString("\033[") ;
    this->sendChar(key) ;
}

TShutdownMode Console::loop() {
    this->keyboardLoop() ;
    if (this->led_ticks > 0 && this->timer->GetTicks() > this->led_ticks) {
        this->led_ticks = 0 ;
        this->actLED->Off() ;
    }
    return this->shutdownMode ;
}

void Console::printf(const char *__restrict format, ...) {
    va_list args ;
    va_start(args, format) ;
    CString tmp ;
    tmp.FormatV(format, args) ;
    putStringVT100(tmp) ;
}

Console* Console::get() {
    return pthis ;
}

void gprintf(const char *__restrict format, ...) {
    va_list args ;
    va_start(args, format) ;
    CString tmp ;
    tmp.FormatV(format, args) ;

    Console::get()->vtFillRegion(30, TEXTMODE_ROWS, TEXTMODE_COLS - 1, TEXTMODE_ROWS, ' ', WHITE_COLOR, BLACK_COLOR) ;
    Console::get()->write(tmp, 30, TEXTMODE_ROWS, RED_COLOR) ;
}

void iprintf(const char *__restrict format, ...) {
    va_list args ;
    va_start(args, format) ;
    CString tmp ;
    tmp.FormatV(format, args) ;

    Console::get()->vtFillRegion(0, TEXTMODE_ROWS, 30, TEXTMODE_ROWS, ' ', WHITE_COLOR, BLACK_COLOR) ;
    Console::get()->write(tmp, 0, TEXTMODE_ROWS, BRIGHT_BLACK_COLOR) ;
}

void Console::showStatus() {
    const u16 *st = status[vt52_mode ? 1 : 0] ;
    for (int y = 0; y < STATUS_HEIGHT; y++) {
        for (int x = 0; x < STATUS_WIDTH; x++) {
			screen->SetPixel(x, y + 456, st[y * STATUS_WIDTH + x]) ;
        }
    }
}

void Console::showRusLat() {
    unsigned xshift = STATUS_WIDTH + 5 ;
    unsigned lrshift = koi7n1 ? LATRUS_HEIGHT * LATRUS_WIDTH : 0 ;
    for (int y = 0; y < LATRUS_HEIGHT; y++) {
        for (int x = 0; x < LATRUS_WIDTH; x++) {
			screen->SetPixel(x + xshift, y + 456, latrus[lrshift + y * LATRUS_WIDTH + x]) ;
        }
    }
}