#include "arm11.h"
#include "kb11.h"
#include "unibus.h"

#include <cons/cons.h>

extern KB11 cpu;

const char *rs[8] = {"R0", "R1", "R2", "R3", "R4", "R5", "SP", "PC"};

struct D {
    u16 mask;
    u16 ins;
    const char *name;
    u8 flag;
    bool b;
};

enum { DD = 1 << 1, S = 1 << 2, RR = 1 << 3, O = 1 << 4, N = 1 << 5, A = 1 << 6 };

extern KB11 cpu;

constexpr D disamtable[] = {
    {0177777, 0000001, "WAIT", 0, false},
    {0177777, 0000002, "RTI", 0, false},
    {0177777, 0000003, "BPT", 0, false},
    {0177777, 0000004, "IOT", 0, false},
    {0177777, 0000005, "RESET", 0, false},
    {0177777, 0000006, "RTT", 0, false},
    {0177777, 0000007, "MFPT", 0, false},

    {0177700, 0000100, "JMP", DD, false},
    {0177770, 0000200, "RTS", RR, false},
    {0177770, 0000230, "SPL", A, false},
    {0177770, 0000240, "CCC", A, false},
    {0177770, 0000250, "CCC", A, false},
    {0177770, 0000260, "SCC", A, false},
    {0177770, 0000270, "SCC", A, false},
    {0177700, 0000300, "SWAB", DD, false},

    {0177700, 0006400, "MARK", N, false},
    {0177700, 0006500, "MFPI", DD, false},
    {0177700, 0006600, "MTPI", DD, false},
    {0177700, 0106500, "MFPD", DD, false},
    {0177700, 0106600, "MTPD", DD, false},
    {0177700, 0006700, "SXT", DD, false},

    {0177400, 0104000, "EMT", N, false},
    {0177400, 0104400, "TRAP", N, false},
    {0177400, 0100000, "BPL", O, false},
    {0177400, 0100400, "BMI", O, false},
    {0177400, 0101000, "BHI", O, false},
    {0177400, 0101400, "BLOS", O, false},
    {0177400, 0102000, "BVC", O, false},
    {0177400, 0102400, "BVS", O, false},
    {0177400, 0103000, "BCC", O, false},
    {0177400, 0103400, "BCS", O, false},
    {0177400, 0000400, "BR", O, false},
    {0177400, 0001000, "BNE", O, false},
    {0177400, 0001400, "BEQ", O, false},
    {0177400, 0002000, "BGE", O, false},
    {0177400, 0002400, "BLT", O, false},
    {0177400, 0003000, "BGT", O, false},
    {0177400, 0003400, "BLE", O, false},

    {0177000, 0004000, "JSR", RR | DD, false},
    {0177000, 0070000, "MUL", RR | DD, false},
    {0177000, 0071000, "DIV", RR | DD, false},
    {0177000, 0072000, "ASH", RR | DD, false},
    {0177000, 0073000, "ASHC", RR | DD, false},
    {0177000, 0077000, "SOB", RR | O, false},
    {0170000, 0060000, "ADD", S | DD, false},
    {0170000, 0160000, "SUB", S | DD, false},

    {0077700, 0005000, "CLR", DD, true},
    {0077700, 0005100, "COM", DD, true},
    {0077700, 0005200, "INC", DD, true},
    {0077700, 0005300, "DEC", DD, true},
    {0077700, 0005400, "NEG", DD, true},
    {0077700, 0005500, "ADC", DD, true},
    {0077700, 0005600, "SBC", DD, true},
    {0077700, 0005700, "TST", DD, true},
    {0077700, 0006000, "ROR", DD, true},
    {0077700, 0006100, "ROL", DD, true},
    {0077700, 0006200, "ASR", DD, true},
    {0077700, 0006300, "ASL", DD, true},

    {0070000, 0010000, "MOV", S | DD, true},
    {0070000, 0020000, "CMP", S | DD, true},
    {0070000, 0030000, "BIT", S | DD, true},
    {0070000, 0040000, "BIC", S | DD, true},
    {0070000, 0050000, "BIS", S | DD, true},
    {0000000, 0000001, "HALT", 0, false}, // fake instruction so HALT is left in l
    {0, 0, "", 0, false},
};

void disasmaddr(u16 m, u16 a) {
    if (m & 7) {
        switch (m) {
            case 027:
                a += 2;
                Console::get()->printf("#%06o", cpu.readW(a));
                return;
            case 037:
                a += 2;
                Console::get()->printf("@#%06o", cpu.readW(a));
                return;
            case 067:
                a += 2;
                Console::get()->printf("%06o", (a + 2 + (cpu.readW(a))) & 0177777);
                return;
            case 077:
                Console::get()->printf("@%06o", (a + 2 + (cpu.readW(a))) & 0177777);
                return;
        }
    }

    switch (m & 070) {
        case 000:
            Console::get()->printf("%s", rs[m & 7]);
            break;
        case 010:
            Console::get()->printf("(%s)", rs[m & 7]);
            break;
        case 020:
            Console::get()->printf("(%s)+", rs[m & 7]);
            break;
        case 030:
            Console::get()->printf("@(%s)+", rs[m & 7]);
            break;
        case 040:
            Console::get()->printf("-(%s)", rs[m & 7]);
            break;
        case 050:
            Console::get()->printf("@-(%s)", rs[m & 7]);
            break;
        case 060:
            a += 2;
            Console::get()->printf("%06o(%s)", cpu.readW(a), rs[m & 7]);
            break;
        case 070:
            a += 2;
            Console::get()->printf("@%06o(%s)", cpu.readW(a), rs[m & 7]);
            break;
    }
}

void disasm(u16 a) {
    const auto ins = cpu.readW(a);

    D l = {0, 0, "", 0, false};
    for (auto i = 0; disamtable[i].ins; i++) {
        l = disamtable[i];
        if ((ins & l.mask) == l.ins) {
            break;
        }
    }
    if (l.ins == 0) {
        Console::get()->printf("???");
        return;
    }

    Console::get()->printf("%s", l.name);
    if (l.b && (ins & 0100000)) {
        Console::get()->printf("B");
    }

    const auto ss = (ins & 07700) >> 6;
    const auto dd = ins & 077;
    auto o = ins & 0377;

    switch (l.flag) {
        case A:
            Console::get()->printf(" %o", ins & 7);
            break ;
        case S | DD:
            Console::get()->printf(" ");
            disasmaddr(ss, a);
            Console::get()->printf(",");
            [[fallthrough]];
        case DD:
            Console::get()->printf(" ");
            disasmaddr(dd, a);
            break;
        case RR | O:
            Console::get()->printf(" %s,", rs[(ins & 0700) >> 6]);
            o &= 077;
            [[fallthrough]];
        case O:
            if (o & 0x80) {
                Console::get()->printf(" -%03o", (2 * ((0xFF ^ o) + 1)));
            } else {
                Console::get()->printf(" +%03o", (2 * o));
            }
            break;
        case RR | DD:
            Console::get()->printf(" %s, ", rs[(ins & 0700) >> 6]);
            disasmaddr(dd, a);
            [[fallthrough]];
        case RR:
            Console::get()->printf(" %s", rs[ins & 7]);
    }
}
