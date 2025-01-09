#include <cons/cons.h>
#include <circle/string.h>
#include <circle/util.h>
#include <circle/logger.h>
#include <circle/serial.h>

Console *Console::pthis = 0 ;

extern CSerialDevice *pSerial ;

Console::Console() :
    shutdownMode(ShutdownNone)
{
    pthis = this ;
}

Console::~Console(void) {
    pthis = 0 ;
}

void Console::init() {
}

bool Console::sendChar(const char c) {
    return pSerial->Write(&c, 1) == 1 ;
}

void Console::sendString(const char *str) {
    while (*str) {
        if (sendChar(*str)) {
            str++ ;
        }
    }
}

TShutdownMode Console::loop() {
    return this->shutdownMode ;
}

Console* Console::get() {
    return pthis ;
}

void Console::printf(const char *__restrict format, ...) {
    va_list args ;
    va_start(args, format) ;
    CString txt ;
    txt.FormatV(format, args) ;
    pthis->sendString(txt) ;
}

void gprintf(const char *__restrict format, ...) {
    va_list args ;
    va_start(args, format) ;
    CLogger::Get()->Write("cons", LogError, format, args) ;
}

void iprintf(const char *__restrict format, ...) {
    va_list args ;
    va_start(args, format) ;
    CLogger::Get()->Write("cons", LogError, format, args) ;
}
