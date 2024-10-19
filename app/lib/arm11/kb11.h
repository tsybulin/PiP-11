#pragma once

#include <circle/types.h>

#include "kt11.h"
#include "unibus.h"

enum { FLAGN = 8, FLAGZ = 4, FLAGV = 2, FLAGC = 1 };

enum CPUStatus : u8 {
    CPU_STATUS_UNKNOWN,
    CPU_STATUS_ENABLE,
    CPU_STATUS_HALT,
    CPU_STATUS_STEP
} ;

enum OperandType : u8 {
    OPERAND_INSTRUCTION,
    OPERAND_DATA
} ;

class API ;

class KB11 {
    friend API ;
  public:
    void step();
    void reset(u16 start);
    void pirq() ;
    void trapat(u16 vec);

    // interrupt schedules an interrupt.
    void interrupt(u8 vec, u8 pri);
    void printstate();
    void ptstate();

    // mode returns the current cpu mode.
    // 0: kernel, 1: supervisor, 2: illegal, 3: user
    constexpr inline u16 currentmode() { return (PSW >> 14); }

    // previousmode returns the previous cpu mode.
    // 0: kernel, 1: supervisor, 2: illegal, 3: user
    constexpr inline u16 previousmode() { return ((PSW >> 12) & 3); }

    // returns the current CPU interrupt priority.
    constexpr inline u16 priority() { return ((PSW >> 5) & 7); }

    constexpr inline bool denabled() {
        u8 mask = 0 ;
        switch (currentmode()) {
            case 0 :
                mask = 4 ;
                break;
            case 1:
                mask = 2 ;
                break ;
            case 3:
                mask = 1 ;
            default:
                mask = 0 ;
                break;
        }
        return mmu.SR[3] & mask ;
    }

    // pop the top interrupt off the itab.
    void popirq();

    struct intr {
        u8 vec;
        u8 pri;
    };

    int rflag;

    intr itab[32];

    KT11 mmu;
    UNIBUS unibus;
    bool print=false;
    bool wtstate;
    inline u16 read16(const u16 va, bool d = false) {
        const auto a = mmu.decode<false>(va, currentmode(), d);
        switch (a) {
            case 0777776:
                return PSW;
            case 0777774:
                return stacklimit;
            case 0777570:
                return switchregister;
            default:
                return unibus.read16(a);
        }
    }
    void write16(const u16 va, const u16 v, bool d = false);
    u16 PC;               // holds R[7] during instruction execution
    u16 PSW;              // processor status word
    u16 R[8]; // R0-R7

    volatile CPUStatus cpuStatus = CPU_STATUS_UNKNOWN ;
  private:
    u16 oldPSW;
    u16 stacklimit, switchregister, displayregister;
    u16 stackpointer[4]; // Alternate R6 (kernel, super, illegal, user)

    u16 pir_str = 0 ;
    u16 pir_cnt = 0 ;

    inline bool N() { return PSW & FLAGN; }
    inline bool Z() { return PSW & FLAGZ; }
    inline bool V() { return PSW & FLAGV; }
    inline bool C() { return PSW & FLAGC; }
    inline void setZ(const bool b) {
        if (b)
            PSW |= FLAGZ;
    }

    inline u16 fetch16() {
        const auto val = read16(R[7]);
        R[7] += 2;
        return val;
    }

    inline void push(const u16 v) {
        R[6] -= 2;
        write16(R[6], v);
    }

    inline u16 pop() {
        const auto val = read16(R[6]);
        R[6] += 2;
        return val;
    }

    typedef struct {
        u16 operand ;
        OperandType operandType ; 
    } Operand ;
    
    template <auto len> inline Operand DA(const u16 instr) {
        static_assert(len == 1 || len == 2);
        if (!(instr & 070)) {
            rflag++;
            return {(u16)(instr & 7), OPERAND_INSTRUCTION} ;
        }
        return fetchOperand<len>(instr);
    }

    template <auto len> Operand fetchOperand(const u16 instr) {
        const auto mode = (instr >> 3) & 7;
        const auto reg = instr & 7;

        Operand result = {0, OPERAND_DATA} ;

        const bool den = denabled() ;

        switch (mode) {
            case 0: // Mode 0: Registers don't have a virtual address so trap!
                trap(INTBUS);
            case 1: // Mode 1: (R)
                result.operand = R[reg] ;
                return result ;
            case 2: // Mode 2: (R)+ including immediate operand #x
                result.operand = R[reg];
                R[reg] += (reg >= 6) ? 2 : len;
                result.operandType = reg < 7 ? OPERAND_DATA : OPERAND_INSTRUCTION ;
                return result ;
            case 3: // Mode 3: @(R)+
                result.operand = R[reg];
                R[reg] += 2;
                result.operand = read16(result.operand, den);
                result.operandType = reg < 7 ? OPERAND_DATA : OPERAND_INSTRUCTION ;
                return result ;
            case 4: // Mode 4: -(R)
                R[reg] -= (reg >= 6) ? 2 : len;
                result.operand = R[reg];
                return result;
            case 5: // Mode 5: @-(R)
                R[reg] -= 2;
                result.operand = R[reg];
                result.operand = read16(result.operand, den);
                return result;
            case 6: // Mode 6: d(R)
                result.operand = fetch16();
                result.operand = result.operand + R[reg];
                return result;
            default: // 7 Mode 7: @d(R)
                result.operand = fetch16();
                result.operand = result.operand + R[reg];
                result.operand = read16(result.operand, den);
                return result ;
        }
    }

    // OOMR
    // constexpr inline bool dmode(const u16 instr) {
    //     const u8 r = instr & 7 ;
    //     const auto mode = (instr >> 3) & 7;
    //     if (mode == 1 || mode == 6 || mode == 7) {
    //         return true ;
    //     }
    //     if ((mode == 2 || mode == 3 || mode == 4 || mode == 5) && r < 7) {
    //         return true ;
    //     }

    //     return false ;
    // }

    template <auto len> constexpr u16 SS(const u16 instr) {
        static_assert(len == 1 || len == 2);

        // If register mode just get register value
        if (!(instr & 07000)) {
            return R[(instr >> 6) & 7] & max<len>();
        }

        const Operand op = fetchOperand<len>(instr >> 6);
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        if constexpr (len == 2) {
            return read16(op.operand, dpage);
        }

        if (op.operand & 1) {
            return read16(op.operand & ~1, dpage) >> 010;
        }

        return read16(op.operand & ~1, dpage) & 0377;
    }

    constexpr inline void branch(const u16 instr) {
        if (instr & 0x80) {
            R[7] += (instr | 0xff00) << 1;
        } else {
            R[7] += (instr & 0xff) << 1;
        }
    }

    constexpr inline void writePSW(const u16 psw) {
        stackpointer[currentmode()] = R[6];
        PSW = psw;
        R[6] = stackpointer[currentmode()];
    }

    // kernelmode pushes the current processor mode and switchs to kernel.
    constexpr inline void kernelmode() {
        writePSW((PSW & 0007777) | (currentmode() << 12));
    }

    template <auto l> constexpr inline u16 read(const u16 a, bool d = false) {
        static_assert(l == 1 || l == 2);

        if (rflag) {
            if constexpr (l == 2) {
                return R[a & 7];
            }
            else {
                return R[a & 7] & 0xFF;
            }
        }
        if constexpr (l == 2) {
            return read16(a, d);
        }
        if (a & 1) {
            return read16(a & ~1, d) >> 8;
        }
        return read16(a, d) & 0xFF;
    }

    template <auto l> constexpr void write(const u16 a, const u16 v, bool d = false) {
        static_assert(l == 1 || l == 2);
        auto vl = v;

        if (rflag) {
            auto r = a & 7;
            if constexpr (l == 2) {
                R[r] = vl;
            } else {
                R[r] &= 0xFF00;
                R[r] |= vl & 0xFF;
            }
            return;
        }

        if constexpr (l == 2) {
            write16(a, vl, d);
            return;
        }

        if (a & 1) {
            write16(a & ~1, (read16(a & ~1, d) & 0xff) | (vl << 8), d);
        }
        else {
            write16(a, (read16(a, d) & 0xFF00) | (vl & 0xFF), d);
        }
    }

    template <auto l> constexpr inline u16 max() {
        static_assert(l == 1 || l == 2);
        if constexpr (l == 2) {
            return 0xffff;
        } else {
            return 0xff;
        }
    }

    template <auto l> constexpr inline u16 msb() {
        static_assert(l == 1 || l == 2);
        if constexpr (l == 2) {
            return 0x8000;
        } else {
            return 0x80;
        }
    }

    // CMP 02SSDD, CMPB 12SSDD
    template <auto l> void CMP(const u16 instr) {
        const auto src = SS<l>(instr);
        Operand op = DA<l>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;

        const auto dst = read<l>(op.operand, dpage);
        const auto sval = (src - dst) & max<l>();
        PSW &= 0xFFF0;
        if (sval == 0) {
            PSW |= FLAGZ;
        }
        if (sval & msb<l>()) {
            PSW |= FLAGN;
        }
        if (((src ^ dst) & msb<l>()) && (!((dst ^ sval) & msb<l>()))) {
            PSW |= FLAGV;
        }
        if (src < dst) {
            PSW |= FLAGC;
        }
    }

    // Set N & Z clearing V (C unchanged)
    template <auto len> inline void setNZ(const u16 v) {
        static_assert(len == 1 || len == 2);
        PSW &= (0xFFF0 | FLAGC);
        if (v & msb<len>()) {
            PSW |= FLAGN;
        }
        if ((v & max<len>()) == 0) {
            PSW |= FLAGZ;
        }
    }

    // Set N, Z & V (C unchanged)
    template <auto len> inline void setNZV(const u16 v) {
        setNZ<len>(v);
        if (v == msb<len>()) {
            PSW |= FLAGV;
        }
    }

    // Set N, Z & C clearing V
    template <auto len> inline void setNZC(const u16 v) {
        static_assert(len == 1 || len == 2);
        PSW &= 0xFFF0;
        if (v & msb<len>()) {
            PSW |= FLAGN;
        }
        if (v == 0) {
            PSW |= FLAGZ;
        }
        PSW |= FLAGC;
    }

    template <auto l> void BIC(const u16 instr) {
        const auto src = SS<l>(instr);
        Operand op = DA<l>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        const auto dst = read<l>(op.operand, dpage);
        auto uval = (max<l>() ^ src) & dst;
        write<l>(op.operand, uval, dpage);
        PSW &= 0xFFF1;
        setZ(uval == 0);
        if (uval & msb<l>()) {
            PSW |= FLAGN;
        }
    }

    template <auto l> void BIS(const u16 instr) {
        const auto src = SS<l>(instr);
        Operand op = DA<l>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        const auto dst = read<l>(op.operand, dpage);
        auto uval = src | dst;
        PSW &= 0xFFF1;
        setZ(uval == 0);
        if (uval & msb<l>()) {
            PSW |= FLAGN;
        }
        write<l>(op.operand, uval, dpage);
    }

    // CLR 0050DD, CLRB 1050DD
    template <auto l> void CLR(const u16 instr) {
        PSW &= 0xFFF0;
        PSW |= FLAGZ;
        Operand op = DA<l>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        write<l>(op.operand, 0, dpage);
    }

    // COM 0051DD, COMB 1051DD
    template <auto l> void COM(const u16 instr) {
        Operand op = DA<l>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        const auto dst = ~read<l>(op.operand, dpage);
        write<l>(op.operand, dst, dpage);
        PSW &= 0xFFF0;
        if ((dst & msb<l>())) {
            PSW |= FLAGN;
        }
        if ((dst & max<l>()) == 0) {
            PSW |= FLAGZ;
        }
        PSW |= FLAGC;
    }

    // DEC 0053DD, DECB 1053DD
    template <auto l> void _DEC(const u16 instr) {
        Operand op = DA<l>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        const auto oval = read<l>(op.operand, dpage) & max<l>();
        const auto uval = (read<l>(op.operand, dpage) - 1) & max<l>();
        write<l>(op.operand, uval, dpage);
        setNZ<l>(uval);
        if (oval == msb<l>()) {
            PSW |= FLAGV;
        }
    }

    // NEG 0054DD, NEGB 1054DD
    template <auto l> void NEG(const u16 instr) {
        Operand op = DA<l>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        const auto dst = (-read<l>(op.operand, dpage)) & max<l>();
        write<l>(op.operand, dst, dpage);
        PSW &= 0xFFF0;
        if (dst & msb<l>()) {
            PSW |= FLAGN;
        }
        if ((dst & max<l>()) == 0) {
            PSW |= FLAGZ;
        } else {
            PSW |= FLAGC;
        }
        if (dst == msb<l>()) {
            PSW |= FLAGV;
        }
    }

    template <auto l> void _ADC(const u16 instr) {
        Operand op = DA<l>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        auto uval = read<l>(op.operand, dpage);
        if (PSW & FLAGC) {
            write<l>(op.operand, (uval + 1) & max<l>(), dpage);
            PSW &= 0xFFF0;
            if ((uval + 1) & msb<l>()) {
                PSW |= FLAGN;
            }
            setZ(uval == max<l>());
            if (l == 1)
                uval = (uval << 8) | 0xff;
            if (uval == 0077777) {
                PSW |= FLAGV;
            }
            if (uval == 0177777) {
                PSW |= FLAGC;
            }
        }
        else {
            PSW &= 0xFFF0;
            if (uval & msb<l>()) {
                PSW |= FLAGN;
            }
            setZ(uval == 0);
        }
    }


    template <auto l> void SBC(const u16 instr) {
        Operand op = DA<l>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        auto sval = read<l>(op.operand, dpage);
        auto qval = sval;
        PSW &= ~(FLAGN | FLAGV | FLAGZ);
        if (PSW & FLAGC) {
            if (sval)
                PSW ^= FLAGC;
            sval = (sval - 1) & max<l>();
            write<l>(op.operand, sval, dpage);
        }
        setZ(sval == 0);
        if (qval == msb<l>())
            PSW |= FLAGV;
        if (sval & msb<l>())
            PSW |= FLAGN;

    }

    template <auto l> void ROR(const u16 instr) {
        Operand op = DA<l>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        const auto dst = read<l>(op.operand, dpage);
        auto result = dst >> 1;
        if (PSW & FLAGC) {
            result |= msb<l>();
        }
        write<l>(op.operand, result, dpage);
        PSW &= 0xFFF0;
        if (dst & 1) {
            // shift lsb into carry
            PSW |= FLAGC;
        }
        if (result & msb<l>()) {
            PSW |= FLAGN;
        }
        if ((result & max<l>()) == 0) {
            PSW |= FLAGZ;
        }
        if (!(PSW & FLAGC) ^ !(PSW & FLAGN)) {
            PSW |= FLAGV;
        }
    }

    template <auto l> void ROL(const u16 instr) {
        Operand op = DA<l>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        u32 sval = read<l>(op.operand, dpage) << 1;
        if (PSW & FLAGC) {
            sval |= 1;
        }
        PSW &= 0xFFF0;
        if (sval & (max<l>() + 1)) {
            PSW |= FLAGC;
        }
        if (sval & msb<l>()) {
            PSW |= FLAGN;
        }
        setZ(!(sval & max<l>()));
        if ((sval ^ (sval >> 1)) & msb<l>()) {
            PSW |= FLAGV;
        }
        sval &= max<l>();
        write<l>(op.operand, sval, dpage);
    }

    template <auto l> void ASR(const u16 instr) {
        Operand op = DA<l>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        auto uval = read<l>(op.operand, dpage);
        PSW &= 0xFFF0;
        if (uval & 1) {
            PSW |= FLAGC;
        }
        uval = (uval & msb<l>()) | (uval >> 1);
        if (uval & msb<l>()) {
            PSW |= FLAGN;
        }
        if (!(PSW & FLAGC) ^ !(PSW & FLAGN)) {
            PSW |= FLAGV;
        }
        setZ(uval == 0);
        write<l>(op.operand, uval, dpage);
    }

    template <auto l> void ASL(const u16 instr) {
        Operand op = DA<l>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        // TODO(dfc) doesn't need to be an sval
        u32 sval = read<l>(op.operand, dpage);
        PSW &= 0xFFF0;
        if (sval & msb<l>()) {
            PSW |= FLAGC;
        }
        if (sval & (msb<l>() >> 1)) {
            PSW |= FLAGN;
        }
        if ((sval ^ (sval << 1)) & msb<l>()) {
            PSW |= FLAGV;
        }
        sval = (sval << 1) & max<l>();
        setZ(sval == 0);
        write<l>(op.operand, sval, dpage);
    }

    // INC 0052DD, INCB 1052DD
    template <auto l> void INC(const u16 instr) {
        Operand op = DA<l>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        const auto dst = read<l>(op.operand, dpage) + 1;
        write<l>(op.operand, dst, dpage);
        setNZV<l>(dst);
    }

    // BIT 03SSDD, BITB 13SSDD
    template <auto l> void _BIT(const u16 instr) {
        const auto src = SS<l>(instr);
        Operand op = DA<l>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        const auto dst = read<l>(op.operand, dpage);
        const auto result = src & dst;
        setNZ<l>(result);
    }

    // TST 0057DD, TSTB 1057DD
    template <auto l> void TST(const u16 instr) {
        Operand op = DA<l>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        const auto dst = read<l>(op.operand, dpage);
        PSW &= 0xFFF0;
        if ((dst & max<l>()) == 0) {
            PSW |= FLAGZ;
        }
        if (dst & msb<l>()) {
            PSW |= FLAGN;
        }
    }

    // MOV 01SSDD, MOVB 11SSDD
    template <auto len> void MOV(const u16 instr) {
        const auto src = SS<len>(instr);
        if (!(instr & 0x38) && (len == 1)) {
            // Special case: movb sign extends register to word size
            R[instr & 7] = src & 0x80 ? 0xff00 | src : src;
            setNZ<len>(src);
            return;
        }
        setNZ<len>(src);

        // const bool dpage = denabled() && dmode(instr) ;
        Operand op = DA<len>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        write<len>(op.operand, src, dpage);
    }

    void ADD(const u16 instr);
    void SUB(const u16 instr);
    void JSR(const u16 instr);
    void MUL(const u16 instr);
    void DIV(const u16 instr);
    void ASH(const u16 instr);
    void ASHC(const u16 instr);
    void XOR(const u16 instr);
    void SOB(const u16 instr);
    void FIS(const u16 instr);
    void JMP(const u16 instr);
    void MARK(const u16 instr);
    void MFPI(const u16 instr);
    void MFPT();
    void MTPS(const u16 instr);
    void MFPS(const u16 instr);
    void MTPI(const u16 instr);
    void RTS(const u16 instr);
    void SWAB(u16);
    void SXT(u16);
    void RTT();
    void RESET();
    void WAIT();
};