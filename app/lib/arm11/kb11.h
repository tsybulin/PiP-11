#pragma once

#include <circle/types.h>

#include "kt11.h"
#include "unibus.h"

#define PSW_BIT_C             001
#define PSW_BIT_V             002
#define PSW_BIT_Z             004
#define PSW_BIT_N             010
#define PSW_MASK_COND     0177760
#define PSW_BIT_T             020
#define PSW_BIT_PRIORITY     0340
#define PSW_BIT_UNUSED      03400
#define PSW_BIT_REG_SET     04000
#define PSW_BIT_PRIV_MODE  030000
#define PSW_BIT_MODE      0140000

#define STACK_LIMIT_YELLOW 0400
#define STACK_LIMIT_RED    0340

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

enum StackTrap : u8 {
    STACK_TRAP_NONE,
    STACK_TRAP_YELLOW,
    STACK_TRAP_RED
} ;

class API ;
class ODT ;

class KB11 {
    friend API ;
    friend ODT ;
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
                break ;
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
    bool wasRTT = false ;
    StackTrap stackTrap = STACK_TRAP_NONE ;

    inline u16 read16(const u16 va, bool d = false, bool src = true) {
        const auto a = mmu.decode<false>(va, currentmode(), d, src);
        switch (a) {
            case 0777776:
                return PSW;
            case 0777774:
                return stacklimit;
            case 0777772:
                return pirqr ;
            case 0777770:
                return microbrreg ;
            case 0777570:
                return switchregister;
            default:
                return unibus.read16(a);
        }
    }

    void write16(const u16 va, const u16 v, bool d = false, bool src = false);
    
    inline u8 REG(const u8 reg) {
        return reg > 5 ? reg : PSW & PSW_BIT_REG_SET ? reg + 8 : reg ;
    }
    
    inline u8 REGNAME(const u8 reg) {
        return reg > 5 ? reg : PSW & PSW_BIT_REG_SET ? reg + 10 : reg ;
    }

    u16 PC;               // holds R[7] during instruction execution
    u16 PSW;              // processor status word
    u16 RR[14]; // R0-R7, R10-15

    volatile CPUStatus cpuStatus = CPU_STATUS_UNKNOWN ;
    u16 odtbpt = 0 ;
  private:
    u16 oldPSW;
    u16 stacklimit, switchregister, displayregister, microbrreg ;
    u16 stackpointer[4]; // Alternate R6 (kernel, super, illegal, user)

    u16 pirqr = 0 ;
    
    inline bool N() { return PSW & PSW_BIT_N; }
    inline bool Z() { return PSW & PSW_BIT_Z; }
    inline bool V() { return PSW & PSW_BIT_V; }
    inline bool C() { return PSW & PSW_BIT_C; }
    inline void setZ(const bool b) {
        if (b)
            PSW |= PSW_BIT_Z;
    }

    inline void setPSWbit(const u8 bit, const bool b) {
        PSW = (PSW & ~bit) | (b ? bit : 0) ;
    }
    
    inline u16 fetch16() {
        const auto val = read16(RR[7]);
        RR[7] += 2;
        return val;
    }

    inline void push(const u16 v) {
        RR[6] -= 2;
        write16(RR[6], v, mmu.SR[3] & 4);
    }

    inline u16 pop() {
        const auto val = read16(RR[6], mmu.SR[3] & 4);
        RR[6] += 2;
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

        return fetchOperand<len>(instr, true);
    }

    inline void checkStackLimit(const u16 addr) {
        if (addr < (stacklimit + STACK_LIMIT_RED)) {
            stackTrap = STACK_TRAP_RED ;
        } else if (addr < (stacklimit + STACK_LIMIT_YELLOW)) {
            stackTrap = STACK_TRAP_YELLOW ;
        }
    }

    inline void mmuStat(u8 regno, u8 amount, bool src) {
        if (regno > 5 || amount == 1 || (mmu.SR[0] & 0401) == 0) {
            return ;
        }

        if (src) {
            mmu.SR[1] = 0 ;
        }
        
        u8 v = regno | (amount << 3) ;
        if (!src) {
            v = v << 8 ;
        }

        mmu.SR[1] |= v ;
    }

    template <auto len> Operand fetchOperand(const u16 instr, bool check_stack = false, bool src = true) {
        const auto mode = (instr >> 3) & 7;
        const auto regno = instr & 7;

        Operand result = {0, OPERAND_DATA} ;

        const bool den = denabled() ;

        switch (mode) {
            case 0: // Mode 0: Registers don't have a virtual address so trap!
                trap(INTINVAL);
            case 1: // Mode 1: (R)
                result.operand = RR[REG(regno)] ;
                if (check_stack && regno == 6) {
                    checkStackLimit(RR[6]) ;
                }
                result.operandType = OPERAND_DATA ;
                return result ;
            case 2: // Mode 2: (R)+ including immediate operand #x
                result.operand = RR[REG(regno)];
                if (check_stack && regno == 6) {
                    checkStackLimit(RR[6]) ;
                }
                RR[REG(regno)] += (regno >= 6) ? 2 : len;
                mmuStat(regno, len, src) ;
                result.operandType = regno < 7 ? OPERAND_DATA : OPERAND_INSTRUCTION ;
                return result ;
            case 3: // Mode 3: @(R)+
                result.operand = RR[REG(regno)];
                if (check_stack && regno == 6) {
                    checkStackLimit(RR[6]) ;
                }
                RR[REG(regno)] += 2;
                mmuStat(regno, 2, src) ;
                result.operand = read16(result.operand, den && regno < 7, src);
                result.operandType = OPERAND_DATA ;
                return result ;
            case 4: // Mode 4: -(R)
                RR[REG(regno)] -= (regno >= 6) ? 2 : len;
                if (check_stack && regno == 6) {
                    checkStackLimit(RR[6]) ;
                }
                mmuStat(regno, -len, src) ;
                result.operand = RR[REG(regno)];
                result.operandType = OPERAND_DATA ;
                return result;
            case 5: // Mode 5: @-(R)
                RR[REG(regno)] -= 2;
                if (check_stack && regno == 6) {
                    checkStackLimit(RR[6]) ;
                }
                mmuStat(regno, -2, src) ;
                result.operand = RR[REG(regno)];
                result.operand = read16(result.operand, den, src);
                result.operandType = OPERAND_DATA ;
                return result;
            case 6: // Mode 6: d(R)
                result.operand = fetch16();
                result.operand = result.operand + RR[REG(regno)];
                if (check_stack && regno == 6) {
                    checkStackLimit(result.operand) ;
                }
                result.operandType = OPERAND_DATA ;
                return result;
            default: // 7 Mode 7: @d(R)
                result.operand = fetch16();
                result.operand = result.operand + RR[REG(regno)];
                if (check_stack && regno == 6) {
                    checkStackLimit(result.operand) ;
                }
                result.operand = read16(result.operand, den, src);
                result.operandType = OPERAND_DATA ;
                return result ;
        }
    }

    template <auto len> constexpr u16 SS(const u16 instr) {
        static_assert(len == 1 || len == 2);

        // If register mode just get register value
        if (!(instr & 07000)) {
            return RR[REG((instr >> 6) & 7)] & max<len>();
        }

        const Operand op = fetchOperand<len>(instr >> 6, false);
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
        if (instr & 0200) {
            RR[7] += (instr | 0177400) << 1;
        } else {
            RR[7] += (instr & 0377) << 1;
        }
    }

    /**
     * Programs operating at outer levels (Supervisor and User)
     *   are inhibited from changing bits 5-7 (the Processor's Priority).
     * They are also restricted in their treatment of
     *    bits 15, 14 (Current Mode), bits 13,
     *    12 (Previous Mode), and bit 11 Register Set);
     *    these bits may only be set, they are only cleared by an interrupt or trap.
     * 
     *    bits 15,14 protection clashed with XXDP CKBHB0 test
     */
    constexpr inline void writePSW(const u16 psw, const bool istrap = false) {
        const u16 mode = currentmode() ;
        stackpointer[mode] = RR[6] ;

        u16 newpsw = psw & ~PSW_BIT_UNUSED ;

        if (!istrap) {
            newpsw &= ~PSW_BIT_T ;
        }

        if (mode && !istrap) {
            // newpsw = (newpsw & ~PSW_BIT_PRIORITY) | (PSW & PSW_BIT_PRIORITY) ;
            // newpsw |= ((PSW & PSW_BIT_MODE) | (psw & PSW_BIT_MODE)) ;
            // newpsw |= ((PSW & PSW_BIT_PRIV_MODE) | (psw & PSW_BIT_PRIV_MODE)) ;
            newpsw |= ((PSW & PSW_BIT_REG_SET) | (psw & PSW_BIT_REG_SET)) ;
        }

        PSW = newpsw ;

        RR[6] = stackpointer[currentmode()];
    }

    // kernelmode pushes the current processor mode and switchs to kernel.
    constexpr inline void kernelmode() {
        writePSW((PSW & 0007777) | (currentmode() << 12));
    }

    template <auto l> constexpr inline u16 read(const u16 a, bool d = false, bool src = true) {
        static_assert(l == 1 || l == 2);

        if (rflag) {
            if constexpr (l == 2) {
                return RR[REG(a & 7)];
            }
            else {
                return RR[REG(a & 7)] & 0xFF;
            }
        }
        if constexpr (l == 2) {
            return read16(a, d, src);
        }
        if (a & 1) {
            return read16(a & ~1, d, src) >> 8;
        }
        return read16(a, d, src) & 0xFF;
    }

    template <auto l> constexpr void write(const u16 a, const u16 v, bool d = false) {
        static_assert(l == 1 || l == 2);
        auto vl = v;

        if (rflag) {
            auto r = REG(a & 7);
            if constexpr (l == 2) {
                RR[r] = vl;
            } else {
                RR[r] &= 0xFF00;
                RR[r] |= vl & 0xFF;
            }
            return;
        }

        if constexpr (l == 2) {
            write16(a, vl, d);
            return;
        }

        if (a & 1) {
            write16(a & ~1, (read16(a & ~1, d, false) & 0xff) | (vl << 8), d);
        }
        else {
            write16(a, (read16(a, d, false) & 0xFF00) | (vl & 0xFF), d);
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

        const auto dst = read<l>(op.operand, dpage, false);
        const auto sval = (src - dst) & max<l>();
        PSW &= PSW_MASK_COND;
        if (sval == 0) {
            PSW |= PSW_BIT_Z;
        }
        if (sval & msb<l>()) {
            PSW |= PSW_BIT_N;
        }
        if (((src ^ dst) & msb<l>()) && (!((dst ^ sval) & msb<l>()))) {
            PSW |= PSW_BIT_V;
        }
        if (src < dst) {
            PSW |= PSW_BIT_C;
        }
    }

    // Set N & Z clearing V (C unchanged)
    template <auto len> inline void setNZ(const u16 v) {
        static_assert(len == 1 || len == 2);
        PSW &= (PSW_MASK_COND | PSW_BIT_C);
        if (v & msb<len>()) {
            PSW |= PSW_BIT_N;
        }
        if ((v & max<len>()) == 0) {
            PSW |= PSW_BIT_Z;
        }
    }

    // Set N, Z & V (C unchanged)
    template <auto len> inline void setNZV(const u16 v) {
        setNZ<len>(v);
        if (v == msb<len>()) {
            PSW |= PSW_BIT_V;
        }
    }

    // Set N, Z & C clearing V
    template <auto len> inline void setNZC(const u16 v) {
        static_assert(len == 1 || len == 2);
        PSW &= PSW_MASK_COND ;
        if (v & msb<len>()) {
            PSW |= PSW_BIT_N;
        }
        if (v == 0) {
            PSW |= PSW_BIT_Z;
        }
        PSW |= PSW_BIT_C;
    }

    template <auto l> void BIC(const u16 instr) {
        const auto src = SS<l>(instr);
        Operand op = DA<l>(instr) ;
        if (stackTrap == STACK_TRAP_RED) {
            return ;
        }
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        const auto dst = read<l>(op.operand, dpage, false);
        auto uval = (max<l>() ^ src) & dst;
        PSW &= (PSW_MASK_COND | PSW_BIT_C);
        setZ(uval == 0);
        if (uval & msb<l>()) {
            PSW |= PSW_BIT_N;
        }
        write<l>(op.operand, uval, dpage);
    }

    template <auto l> void BIS(const u16 instr) {
        const auto src = SS<l>(instr);
        Operand op = DA<l>(instr) ;
        if (stackTrap == STACK_TRAP_RED) {
            return ;
        }
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        const auto dst = read<l>(op.operand, dpage, false);
        auto uval = src | dst;
        PSW &= (PSW_MASK_COND | PSW_BIT_C);
        setZ(uval == 0);
        if (uval & msb<l>()) {
            PSW |= PSW_BIT_N;
        }
        write<l>(op.operand, uval, dpage);
    }

    // CLR 0050DD, CLRB 1050DD
    template <auto l> void CLR(const u16 instr) {
        PSW &= PSW_MASK_COND;
        PSW |= PSW_BIT_Z;
        Operand op = DA<l>(instr) ;
        if (stackTrap == STACK_TRAP_RED) {
            return ;
        }
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        write<l>(op.operand, 0, dpage);
    }

    // COM 0051DD, COMB 1051DD
    template <auto l> void COM(const u16 instr) {
        Operand op = DA<l>(instr) ;
        if (stackTrap == STACK_TRAP_RED) {
            return ;
        }
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        const auto dst = ~read<l>(op.operand, dpage, false);
        PSW &= PSW_MASK_COND;
        if ((dst & msb<l>())) {
            PSW |= PSW_BIT_N;
        }
        if ((dst & max<l>()) == 0) {
            PSW |= PSW_BIT_Z;
        }
        PSW |= PSW_BIT_C;
        write<l>(op.operand, dst, dpage);
    }

    // DEC 0053DD, DECB 1053DD
    template <auto l> void _DEC(const u16 instr) {
        Operand op = DA<l>(instr) ;
        if (stackTrap == STACK_TRAP_RED) {
            return ;
        }
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        const auto oval = read<l>(op.operand, dpage, false) & max<l>();
        const auto uval = (read<l>(op.operand, dpage, false) - 1) & max<l>();
        setNZ<l>(uval);
        if (oval == msb<l>()) {
            PSW |= PSW_BIT_V;
        }
        write<l>(op.operand, uval, dpage);
    }

    // NEG 0054DD, NEGB 1054DD
    template <auto l> void NEG(const u16 instr) {
        Operand op = DA<l>(instr) ;
        if (stackTrap == STACK_TRAP_RED) {
            return ;
        }
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        const auto dst = (-read<l>(op.operand, dpage, false)) & max<l>();
        PSW &= PSW_MASK_COND;
        if (dst & msb<l>()) {
            PSW |= PSW_BIT_N;
        }
        if ((dst & max<l>()) == 0) {
            PSW |= PSW_BIT_Z;
        } else {
            PSW |= PSW_BIT_C;
        }
        if (dst == msb<l>()) {
            PSW |= PSW_BIT_V;
        }
        write<l>(op.operand, dst, dpage);
    }

    template <auto l> void _ADC(const u16 instr) {
        Operand op = DA<l>(instr) ;
        if (stackTrap == STACK_TRAP_RED) {
            return ;
        }
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        auto uval = read<l>(op.operand, dpage, false);
        if (PSW & PSW_BIT_C) {
            PSW &= PSW_MASK_COND;
            if ((uval + 1) & msb<l>()) {
                PSW |= PSW_BIT_N;
            }
            setZ(uval == max<l>());
            if (l == 1)
                uval = (uval << 8) | 0xff;
            if (uval == 0077777) {
                PSW |= PSW_BIT_V;
            }
            if (uval == 0177777) {
                PSW |= PSW_BIT_C;
            }
            write<l>(op.operand, (uval + 1) & max<l>(), dpage);
        }
        else {
            PSW &= PSW_MASK_COND;
            if (uval & msb<l>()) {
                PSW |= PSW_BIT_N;
            }
            setZ(uval == 0);
        }
    }


    template <auto l> void SBC(const u16 instr) {
        Operand op = DA<l>(instr) ;
        if (stackTrap == STACK_TRAP_RED) {
            return ;
        }
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        auto sval = read<l>(op.operand, dpage, false);
        auto qval = sval;
        PSW &= ~(PSW_BIT_N | PSW_BIT_V | PSW_BIT_Z);
        if (PSW & PSW_BIT_C) {
            if (sval)
                PSW ^= PSW_BIT_C;
            sval = (sval - 1) & max<l>();
            write<l>(op.operand, sval, dpage);
        }
        setZ(sval == 0);
        if (qval == msb<l>())
            PSW |= PSW_BIT_V;
        if (sval & msb<l>())
            PSW |= PSW_BIT_N;

    }

    template <auto l> void ROR(const u16 instr) {
        Operand op = DA<l>(instr) ;
        if (stackTrap == STACK_TRAP_RED) {
            return ;
        }
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        const auto dst = read<l>(op.operand, dpage, false);
        auto result = dst >> 1;
        if (PSW & PSW_BIT_C) {
            result |= msb<l>();
        }
        PSW &= PSW_MASK_COND;
        if (dst & 1) {
            // shift lsb into carry
            PSW |= PSW_BIT_C;
        }
        if (result & msb<l>()) {
            PSW |= PSW_BIT_N;
        }
        if ((result & max<l>()) == 0) {
            PSW |= PSW_BIT_Z;
        }
        if (!(PSW & PSW_BIT_C) ^ !(PSW & PSW_BIT_N)) {
            PSW |= PSW_BIT_V;
        }
        write<l>(op.operand, result, dpage);
    }

    template <auto l> void ROL(const u16 instr) {
        Operand op = DA<l>(instr) ;
        if (stackTrap == STACK_TRAP_RED) {
            return ;
        }
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        u32 sval = read<l>(op.operand, dpage, false) << 1;
        if (PSW & PSW_BIT_C) {
            sval |= 1;
        }
        PSW &= PSW_MASK_COND;
        if (sval & (max<l>() + 1)) {
            PSW |= PSW_BIT_C;
        }
        if (sval & msb<l>()) {
            PSW |= PSW_BIT_N;
        }
        setZ(!(sval & max<l>()));
        if ((sval ^ (sval >> 1)) & msb<l>()) {
            PSW |= PSW_BIT_V;
        }
        sval &= max<l>();
        write<l>(op.operand, sval, dpage);
    }

    template <auto l> void ASR(const u16 instr) {
        Operand op = DA<l>(instr) ;
        if (stackTrap == STACK_TRAP_RED) {
            return ;
        }
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        auto uval = read<l>(op.operand, dpage, false);
        PSW &= PSW_MASK_COND;
        if (uval & 1) {
            PSW |= PSW_BIT_C;
        }
        uval = (uval & msb<l>()) | (uval >> 1);
        if (uval & msb<l>()) {
            PSW |= PSW_BIT_N;
        }
        if (!(PSW & PSW_BIT_C) ^ !(PSW & PSW_BIT_N)) {
            PSW |= PSW_BIT_V;
        }
        setZ(uval == 0);
        write<l>(op.operand, uval, dpage);
    }

    template <auto l> void ASL(const u16 instr) {
        Operand op = DA<l>(instr) ;
        if (stackTrap == STACK_TRAP_RED) {
            return ;
        }
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        // TODO(dfc) doesn't need to be an sval
        u32 sval = read<l>(op.operand, dpage, false);
        PSW &= PSW_MASK_COND;
        if (sval & msb<l>()) {
            PSW |= PSW_BIT_C;
        }
        if (sval & (msb<l>() >> 1)) {
            PSW |= PSW_BIT_N;
        }
        if ((sval ^ (sval << 1)) & msb<l>()) {
            PSW |= PSW_BIT_V;
        }
        sval = (sval << 1) & max<l>();
        setZ(sval == 0);
        write<l>(op.operand, sval, dpage);
    }

    // INC 0052DD, INCB 1052DD
    template <auto l> void INC(const u16 instr) {
        Operand op = DA<l>(instr) ;
        if (stackTrap == STACK_TRAP_RED) {
            return ;
        }
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        const auto dst = read<l>(op.operand, dpage, false) + 1;
        setNZV<l>(dst);
        write<l>(op.operand, dst, dpage);
    }

    // BIT 03SSDD, BITB 13SSDD
    template <auto l> void _BIT(const u16 instr) {
        const auto src = SS<l>(instr);
        Operand op = DA<l>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        const auto dst = read<l>(op.operand, dpage, false);
        const auto result = src & dst;
        setNZ<l>(result);
    }

    // TST 0057DD, TSTB 1057DD
    template <auto l> void TST(const u16 instr) {
        Operand op = DA<l>(instr) ;
        const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
        const auto dst = read<l>(op.operand, dpage, false);
        PSW &= PSW_MASK_COND;
        if ((dst & max<l>()) == 0) {
            PSW |= PSW_BIT_Z;
        }
        if (dst & msb<l>()) {
            PSW |= PSW_BIT_N;
        }
    }

    // MOV 01SSDD, MOVB 11SSDD
    template <auto len> void MOV(const u16 instr) {
        const auto src = SS<len>(instr);
        if (!(instr & 0x38) && (len == 1)) {
            // Special case: movb sign extends register to word size
            RR[REG(instr & 7)] = src & 0x80 ? 0xff00 | src : src;
            setNZ<len>(src);
            return;
        }
        setNZ<len>(src);

        // const bool dpage = denabled() && dmode(instr) ;
        Operand op = DA<len>(instr) ;
        if (stackTrap == STACK_TRAP_RED) {
            return ;
        }
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
    void RTI();
    void RTT();
    void RESET();
    void WAIT();
};