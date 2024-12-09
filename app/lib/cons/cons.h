#ifndef _cons_cons_h
#define _cons_cons_h

#include <circle/types.h>
#include "shutdown.h"
#include <util/queue.h>

void gprintf(const char *__restrict format, ...) ;
void iprintf(const char *__restrict format, ...) ;

class Console {
	public:
		Console();
		~Console(void);

		void init() ;
		TShutdownMode loop() ;
		volatile TShutdownMode shutdownMode ;
		void sendChar(char c) ;
		void sendString(const char *str) ;
		void printf(const char *__restrict format, ...) ;

		static Console* get() ;
	private:
		static Console *pthis ;

	private:
}  ;

#endif

