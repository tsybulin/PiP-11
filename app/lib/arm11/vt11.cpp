#include "vt11.h"

#include <circle/logger.h>
#include "arm11.h"

VT11::VT11() :
    dpc(0),
    mpr(0),
    gixpr(0),
    ccypr(0),
    rr(0),
    spr(0)
{
}

u16 VT11::read16(u32 a) {
    switch (a) {
        case VT11_DPC:
            return dpc & ~1 ;
        case VT11_MPR:
            return mpr ;
        case VT11_GIXPR:
            return gixpr ;
        case VT11_CCYPR:
            return ccypr ;
        case VT11_RR:
            return rr ;
        case VT11_SPR:
            return spr ;
        case VT11_SAMR:
            return samr ;
        
        default:
            CLogger::Get()->Write("VT11", LogError, "read16 non-existent address %08o", a) ;
            trap(INTBUS);
    }

    return 0 ;
}

void VT11::write16(u32 a, u16 v) {
    switch (a) {
        case VT11_DPC:
            dpc = v ;
            break;
        case VT11_MPR:
            mpr = v ;
            break ;
        case VT11_GIXPR:
        case VT11_CCYPR:
            CLogger::Get()->Write("VT11", LogError, "write16 GIXPR/CCYPR read-only address %08o : %06o", a, v) ;
            break;
        case VT11_RR:
            rr = v & 07777 ; // 12bit
            break;
        case VT11_SPR:
            rr = (rr & ~200) | (v & 0200) ; // bit 7 only
            break;
        case VT11_SAMR:
            samr = v ;
            break;
        
        default:
            CLogger::Get()->Write("VT11", LogError, "write16 non-existent address %08o : %06o", a, v) ;
            trap(INTBUS);
    }
}
