#include <cons/cons.h>
#include <circle/string.h>
#include <circle/util.h>
#include <circle/logger.h>

Console *Console::pthis = 0 ;

queue_t netrecv_queue ;
queue_t netsend_queue ;

Console::Console() :
    shutdownMode(ShutdownNone)
{
    pthis = this ;
}

Console::~Console(void) {
    pthis = 0 ;
}

void Console::init() {
    queue_init(&netsend_queue, 1, 80) ;
    queue_init(&netrecv_queue, 1, 256) ;
}

void Console::sendChar(char c) {
    queue_try_add(&netsend_queue, &c) ;
}

void Console::sendString(const char *str) {
    while (*str) {
        queue_try_add(&netsend_queue, str++) ;
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
    CLogger::Get()->Write("cons", LogError, format, args) ;
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
