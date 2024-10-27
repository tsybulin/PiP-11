#include "odt.h"

#include <arm11/kb11.h>
#include <util/queue.h>
#include <circle/logger.h>

extern KB11 cpu ;
extern queue_t keyboard_queue ;
void disasm(u32 ia);

ODT::ODT() :
    prompt_shown(false),
    bufptr(0),
    cons(0)
{
}

void ODT::loop() {
    if (cpu.cpuStatus != CPU_STATUS_HALT) {
        return ;
    }

    if (!cons) {
        cons = Console::get() ;
    }

    if (!prompt_shown) {
        prompt_shown = true ;
        if (cpu.wtstate) {
            cons->printf("\r\nw %06o\r\n@", cpu.RR[7]) ;
        } else {
            cons->printf("\r\n%06o\r\n@", cpu.RR[7]) ;
        }
    }

    char c ;
    if (!queue_try_remove(&keyboard_queue, &c)) {
        return ;
    }

    switch (c) {
        case 010: // backspace
        case 0177: // delete
        case 015: // enter
        case 'c':
        case 'g':
        case 'e':
        case 'd':
        case '?':
        case 'p':
        case 'r':
        case 's':
        case 'u':
        case ' ':
        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
            parseChar(c) ;
            break;
        
        default:
            break;
    }
}

void ODT::parseChar(const char c) {
    if (c == 015) {
        if (bufptr == 0) {
            prompt_shown = false ;
            return ;
        }

        parseCommand() ;

        return ;
    }

    if ((c == 010 || c == 0177)) {
        if (bufptr > 0) {
            bufptr-- ;
            cons->putCharVT(0177) ;
        }
        return ;
    }

    if (c == '-') {
        if (bufptr < 2) {
            return ;
        }
    
        if (buf[0] != 'e') {
            return ;
        }
    }

    if (c == ' ') {
        if (bufptr < 1) {
            return ;
        }

        if (buf[0] != 'g' && buf[0] != 'e' && buf[0] != 'd' && buf[0] != 's' && buf[0] != 'u') {
            return ;
        }

        if (bufptr > 1 && buf[0] != 'd') {
            return ;
        }
    }

    if (c == '?' && bufptr > 0) {
        return ;
    }

    if (c >= '0' && c <= '7' && bufptr < 2) {
        return ;
    }

    if (c >= 'a' && c <= 'z' && bufptr > 0) {
        return ;
    }

    buf[bufptr] = c ;
    if (bufptr < 78) {
        buf[++bufptr] = 0 ;
    }

    cons->putCharVT(c) ;
}

void ODT::parseCommand() {
    prompt_shown = false ;

    if (bufptr == 1 && buf[0] == '?') {
        cons->printf("\r\n") ;
        cons->printf("?         : this help\r\n") ;
        cons->printf("c         : continue\r\n") ;
        cons->printf("d oa ov   : deposit octal value ov to octall address oa\r\n") ;
        cons->printf("e ob[-oe] : examine octall address ob or range ob ..< oe\r\n") ;
        cons->printf("g oa      : go to octall address oa and run\r\n") ;
        cons->printf("p         : print state\r\n") ;
        cons->printf("r         : reset\r\n") ;
        cons->printf("s [oa]    : step from current PC or optional octal address oa\r\n") ;
        cons->printf("u oa      : disasm instruction at octall address oa\r\n") ;
        bufptr = 0 ;
        return ;
    }

    if (bufptr == 1 && buf[0] == 'c') {
        cons->printf("\r\n") ;
        cpu.cpuStatus = CPU_STATUS_ENABLE ;
        bufptr = 0 ;
        return ;
    }

    if (bufptr > 0 && buf[0] == 's') {
        if (bufptr > 2) {
            bufptr = 2 ;
        }

        u32 arg1, arg2 ;
        int r = parseArg(&arg1, &arg2) ;
        if (r == 1) {
            arg1 = (arg1 >> 1) << 1 ;
            cpu.RR[7] = arg1 ;
        }
        
        if (r == 2) {
            cons->printf("incorrect args. ? for help\r\n") ;
            bufptr = 0 ;
            return ;
        }

        cpu.cpuStatus = CPU_STATUS_STEP ;
        bufptr = 0 ;
        return ;
    }

    if (bufptr == 1 && buf[0] == 'p') {
        cons->printf("\r\n") ;
        cpu.printstate() ;
        bufptr = 0 ;
        return ;
    }

    if (bufptr == 1 && buf[0] == 'r') {
        cpu.RESET() ;
        cpu.wtstate = false ;
        bufptr = 0 ;
        return ;
    }

    if (bufptr > 0 && buf[0] == 'g') {
        cons->printf("\r\n") ;
        u32 arg1, arg2 ;

        if (bufptr > 2) {
            bufptr = 2 ;
        }

        int r = parseArg(&arg1, &arg2) ;
        if (r == 1) {
            arg1 = (arg1 >> 1) << 1 ;
            cpu.RR[7] = arg1 ;
            cpu.wtstate = false ;
            cpu.cpuStatus = CPU_STATUS_ENABLE ;
        } else {
            cons->printf("incorrect args. ? for help\r\n") ;
        }

        bufptr = 0 ;
        return ;
    }

    if (bufptr > 0 && buf[0] == 'e') {
        cons->printf("\r\n") ;
        u32 arg1, arg2 ;

        if (bufptr > 2) {
            bufptr = 2 ;
        }

        int r = parseArg(&arg1, &arg2) ;

        if (r == 0) {
            cons->printf("incorrect args. ? for help\r\n") ;
            bufptr = 0 ;
            return ;
        }

        arg1 = (arg1 >> 1) << 1 ;

        if (r == 1) {
            arg2 = arg1 + 2;
        }

        arg2 = (arg2 >> 1) << 1 ;

        if (arg1 > arg2) {
            cons->printf("incorrect args. ? for help\r\n") ;
            bufptr = 0 ;
            return ;
        }

        int cnt = 0 ;
        for (u32 i = arg1; i < arg2; i += 2) {
            if (cnt == 0) {
                cons->printf("%06o : ", i) ;
            }

            u16 val = cpu.read16(i) ;
            cons->printf("%06o ", val) ;
            
            if (++cnt > 7) {
                cons->printf("\r\n") ;
                cnt = 0 ;
            }
        }
        
        cons->printf("\r\n") ;

        bufptr = 0 ;
        return ;
    }

    if (bufptr > 0 && buf[0] == 'd') {
        cons->printf("\r\n") ;
        u32 arg1, arg2 ;

        if (bufptr > 2) {
            bufptr = 2 ;
        }

        int r = parseArg(&arg1, &arg2, true) ;
        if (r != 2) {
            cons->printf("incorrect args. ? for help\r\n") ;
            bufptr = 0 ;
            return ;
        }

        arg1 = (arg1 >> 1) << 1 ;

        cpu.write16(arg1, arg2) ;

        bufptr = 0 ;
        return ;
    }

    if (bufptr > 0 && buf[0] == 'u') {
        cons->printf("\r\n") ;
        u32 arg1, arg2 ;

        if (bufptr > 2) {
            bufptr = 2 ;
        }

        int r = parseArg(&arg1, &arg2) ;
        if (r == 1) {
            arg1 = (arg1 >> 1) << 1 ;
            disasm(arg1) ;
        } else {
            cons->printf("incorrect args. ? for help\r\n") ;
        }

        bufptr = 0 ;
        return ;
    }

    cons->printf("\r\nunknown command. ? for help\r\n") ;
    bufptr = 0 ;
}

int ODT::parseArg(u32 *arg1, u32 *arg2, bool allow_space) {
    if (bufptr < 2) {
        return 0 ;
    }

    *arg1 = 0 ;
    int res = 0 ;
    
    while (bufptr < 78 && buf[bufptr]) {
        char c = buf[bufptr++] ;
        if (c >= '0' && c <= '7') {
            *arg1 = (*arg1 << 3) + (c - 060) ;
            res = 1 ;
            continue ;
        }

        if ((!allow_space && c == '-') || (allow_space && c == ' ')) {
            res = 2 ;
            break;
        }
    }

    if (res == 0 || res == 1) {
        return res ;
    }

    *arg2 = 0 ;

    while (bufptr < 78 && buf[bufptr]) {
        char c = buf[bufptr++] ;
        if (c >= '0' && c <= '7') {
            *arg2 = (*arg2 << 3) + (c - 060) ;
            res = 2 ;
            continue ;
        }

        res = 0 ;
    }

    
    return res ;
}