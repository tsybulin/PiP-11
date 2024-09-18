#include "pc11.h"
#include "arm11.h"

#include <cons/cons.h>
extern volatile bool interrupted ;

u16 PC11::read16(u32 a) {
    switch (a & 6) {
        case 0: // ptr11.prs  777550
            break;
        case 2: // ptr11.pdb  777552
            prs = prs & ~0x80; // Clear DONE,
            return prb;
    }
    gprintf("mmu::read16 invalid read from %06o\n", a);
    trap(004);
    while(!interrupted) {}
    return 0 ;
}