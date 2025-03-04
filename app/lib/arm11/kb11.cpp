#include "kb11.h"

#include <circle/setjmp.h>

#include <cons/cons.h>
#include <circle/logger.h>

/*
            ***** Update 1/3/2023 ISS *****
            * This update resolves a fundamental design flaw which placed the
            * CPU registers at an arbirary and umappped location in the IOPAGE.
            * Due to the fact that any instruction can reference either a register directly eg INC R0
            * or, using a register as an address (with optional indirection) eg INC (R0), with only 16 bits of address
            * being passed to the read/write routines, a method of determining if the operand is a register
            * or an address is required. The minimal implmentation of this is the use of rflag to indicate
            * if an operand is a register or a memory address. This is set if the destination is a register and cleared if not.
            * The flag is cleared at the beginning of each CPU step and set in DA(...).
*/

extern volatile bool interrupted ;

void disasm(u16 ia);
void fp11(int IR);

KB11::KB11() {
    for (int i = 0; i < 128; i++) {
        irqs[i] = 0 ;
    }
}

void KB11::reset(u16 start) {
    RR[7] = start;
    datapath = RR[REG(0)] ;
    stacklimit = 0400 ;
    switchregister = 0;
    mmu.reset() ;
    unibus.reset(false);
    wtstate = false;
    errorRegister = 0 ;
}

u16 KB11::readW(const u16 va, bool d, bool src) {
    const auto a = mmu.decode<false>(va, currentmode(), d, src);
    return read16(a) ;
}

u16 KB11::read16(const u32 a) {
        ldat = a ;
        switch (a) {
            case 017777776:
                return PSW;
            case 017777774:
                return stacklimit;
            case 017777772:
                return pirqr ;
            case 017777770:
                return microbrreg ;
            case 017777766: // cpu error register
                return errorRegister ;
            case 017777570:
                return switchregister;
            case 017777740:
                return lowErrorAddressRegister ;
            case 017777742:
                return highErrorAddressRegister ;
            case 017777744:
                return memorySystemErrorRegister ;
            case 017777746:
                return memoryControlRegister ;
            case 017777750:
                return memoryMaintenanceRegister ;
            case 017777752:
                return hitMissRegister ;
            case 017777764:
                return 011064 ; // System I/D
            case 017777762:
                return 0 ; // Upper Size
            case 017777760:
                return (MEMSIZE >> 6) - 1 ; // Lower Size
            default:
                return unibus.read16(a);
        }
}

void KB11::writeW(const u16 va, const u16 v, bool d, bool src) {
    const auto a = mmu.decode<true>(va, currentmode(), d, src);
    write16(a, v) ;
}

void KB11::write16(const u32 a, const u16 v) {
    switch (a) {
        case 017777772: {
                pirqr = v & 0177000 ;
                u8 pia = 0 ;
                if (pirqr & 01000) {
                    pia = 042 ;
                }
                if (pirqr & 02000) {
                    pia = 0104 ;
                }
                if (pirqr & 04000) {
                    pia = 0146 ;
                }
                if (pirqr & 010000) {
                    pia = 0210 ;
                }
                if (pirqr & 020000) {
                    pia = 0252 ;
                }
                if (pirqr & 040000) {
                    pia = 0314 ;
                }
                if (pirqr & 0100000) {
                    pia = 0356 ;
                }

                pirqr |= pia ;
            }
            break ;
        case 017777776:
            writePSW(v);
            updatePriority() ;
            break;
        case 017777774:
            stacklimit = v & 0177400 ;
            break;
        case 017777770:
            microbrreg = v ;
            break ;
        case 017777766: // cpu error register
            errorRegister = v ;
            break;
        case 017777740:
            lowErrorAddressRegister = v ;
            break ;
        case 017777742:
            highErrorAddressRegister = v ;
            break;
        case 017777744:
            memorySystemErrorRegister &= ~v ;
            break;
        case 017777746:
            memoryControlRegister = v & 077 ;
            break ;
        case 017777750:
            memoryMaintenanceRegister = v & 0177776 ;
            break ;
        case 017777752:
            hitMissRegister = v ;
            break ;
        case 017777570:
            displayregister = v;
            break;
        default:
            unibus.write16(a, v);
    }
}


// ADD 06SSDD
void KB11::ADD(const u16 instr) {
    const auto src = SS<2>(instr);
    Operand op = DA<2>(instr) ;
    if (stackTrap == STACK_TRAP_RED) {
        return ;
    }
    const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
    const auto dst = read<2>(op.operand, dpage, false);
    const auto sum = src + dst;
    PSW &= 0xFFF0;
    setNZ<2>(sum);
    if (((~src ^ dst) & (src ^ sum)) & 0x8000) {
        PSW |= PSW_BIT_V;
    }
    if ((s32(src) + s32(dst)) > 0xFFFF) {
        PSW |= PSW_BIT_C;
    }
    write<2>(op.operand, sum, dpage);
    datapath = sum ;
}

// SUB 16SSDD
void KB11::SUB(const u16 instr) {
    const auto val1 = SS<2>(instr);
    Operand op = DA<2>(instr) ;
    if (stackTrap == STACK_TRAP_RED) {
        return ;
    }
    const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
    const auto val2 = read<2>(op.operand, dpage, false);
    const auto uval = (val2 - val1) & 0xFFFF;
    PSW &= 0xFFF0;
    setNZ<2>(uval);
    if (((val1 ^ val2) & (~val1 ^ uval)) & 0x8000) {
        PSW |= PSW_BIT_V;
    }
    if (val1 > val2) {
        PSW |= PSW_BIT_C;
    }
    write<2>(op.operand, uval, dpage);
    datapath = uval ;
}

#define GET_SIGN_W(v) (((v) >> 15) & 1)

// MUL 070RSS
void KB11::MUL(const u16 instr) {
	const auto reg = REG((instr >> 6) & 7);
    Operand op = DA<2>(instr) ;
    const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
	s32 src2 = read<2>(op.operand, dpage, false) ;
	s32 src = RR[reg] ;
    if (GET_SIGN_W(src2)) {
        src2 = src2 | ~077777 ;
    }
    if (GET_SIGN_W(src)) {
        src = src | ~077777 ;
    }

	s32 dst = src * src2 ;
    // CLogger::Get()->Write("MUL", LogError, "%i * %i = %i", val1, val2, sval) ;
    RR[reg] = (dst >> 16) & 0177777 ;
	RR[reg | 1] = dst & 0177777 ;
	PSW &= PSW_MASK_COND ;
    setPSWbit(PSW_BIT_N, dst < 0) ;
    setPSWbit(PSW_BIT_Z, dst == 0) ;
    setPSWbit(PSW_BIT_C, ((dst > 077777) || (dst < -0100000))) ;
}

void KB11::DIV(const u16 instr) {
	const auto reg = REG((instr >> 6) & 7);
	s32 src = (((u32)RR[reg] << 16)) | (RR[reg | 1]);
    Operand op = DA<2>(instr) ;
    const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
	s32 src2 = read<2>(op.operand, dpage, false) ;

    // CLogger::Get()->Write("DIV", LogError, "%i / %i", src, src2) ;

    if (src2 == 0) {
        PSW &= PSW_MASK_COND ;
		PSW |= PSW_BIT_C | PSW_BIT_V | PSW_BIT_Z ;
        // CLogger::Get()->Write("DIV", LogError, "%d : /SS=0, PSW %06o", __LINE__, PSW) ;
        return ;
    }

    if ((((u32)src) == 020000000000) && (src2 == 0177777)) {
        PSW &= PSW_MASK_COND ;
		PSW |= PSW_BIT_V ;
        // CLogger::Get()->Write("DIV", LogError, "%d : PSW %06o", __LINE__, PSW) ;
        return ;
    }

    if (GET_SIGN_W(src2)) {
        src2 = src2 | ~077777 ;
    }

    if (GET_SIGN_W(RR[reg])) {
        src = src | ~017777777777 ;
    }

    s32 dst = src / src2 ;
    // CLogger::Get()->Write("DIV", LogError, "%i / %i = %i", src, src2, dst) ;

    setPSWbit(PSW_BIT_N, dst < 0) ;
    // CLogger::Get()->Write("DIV", LogError, "%d : PSW %06o", __LINE__, PSW) ;

    if (dst > 077777) {
        setPSWbit(PSW_BIT_V, true) ;
        setPSWbit(PSW_BIT_C | PSW_BIT_Z, false) ;
        return ;
    }

    if (dst < -0100000) {
        setPSWbit(PSW_BIT_V | PSW_BIT_N, true) ;
        setPSWbit(PSW_BIT_C, false) ;
        return ;
    }

    RR[reg] = dst & 0177777 ;
    RR[reg | 1] = (src - (src2 * dst)) & 0177777 ;
    PSW &= ~(PSW_BIT_Z | PSW_BIT_C | PSW_BIT_V) ;
    if (dst == 0) {
        PSW |= PSW_BIT_Z ;
    }
    // CLogger::Get()->Write("DIV", LogError, "%d : PSW %06o", __LINE__, PSW) ;
}

void KB11::ASH(const u16 instr) {
	const auto reg = REG((instr >> 6) & 7);
	const auto val1 = RR[reg];
    Operand op = DA<2>(instr, false) ;
    const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
	auto val2 = read<2>(op.operand, dpage, false) & 077;
	PSW &= 0xFFF0;
	s32 sval = val1;
	if (val2 & 040) {
		val2 = 0100 - val2;
		if (sval & 0100000)
			sval |= 0xffff0000;
		sval = sval << 1;
		sval = sval >> val2;
		if (sval & 1)
			PSW |= PSW_BIT_C;
		sval = sval >> 1;
	}
	else {
		u32 msk = 0xffff << val2;
		sval = sval << val2;
		if (sval & 0xffff0000)
			if ((sval & 0xffff0000) != (msk & 0xffff0000))
				PSW |= PSW_BIT_V;
		if ((sval & 0x8000) != (val1 & 0x8000))
			PSW |= PSW_BIT_V;
		if (sval & 0200000)
			PSW |= PSW_BIT_C;
	}
	sval &= 0xffff;
	setZ(sval == 0);
	if (sval & 0100000) {
		PSW |= PSW_BIT_N;
	}
	RR[reg] = sval;
    datapath = sval ;
}

void KB11::ASHC(const u16 instr) {
	const auto reg = REG((instr >> 6) & 7) ;
    Operand op = DA<2>(instr, false) ;
    const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
	auto nbits = read<2>(op.operand, dpage, false) & 077;

    s32 sign = ((RR[reg]) >> 15) & 1 ;
    s32 src = (((u32) RR[reg]) << 16) | RR[reg | 1];
    s32 dst, i ;

    if (nbits == 0) {                            /* [0] */
        dst = src;
        setPSWbit(PSW_BIT_V, false) ;
        setPSWbit(PSW_BIT_C, false) ;
    } else if (nbits <= 31) {                      /* [1,31] */
        dst = ((u32) src) << nbits;
        i = (src >> (32 - nbits)) | (-sign << nbits);
        setPSWbit(PSW_BIT_V, i != ((dst & 020000000000)? -1: 0)) ;
        setPSWbit(PSW_BIT_C, i & 1) ;
    } else if (nbits == 32) {                      /* [32] = -32 */
        dst = -sign;
        setPSWbit(PSW_BIT_V, false) ;
        setPSWbit(PSW_BIT_C, sign) ;
    } else {                                      /* [33,63] = -31,-1 */
        dst = (src >> (64 - nbits)) | (-sign << (nbits - 32));
        setPSWbit(PSW_BIT_V, false) ;
        setPSWbit(PSW_BIT_C, (src >> (63 - nbits)) & 1) ;
    }
    
    i = RR[reg] = (dst >> 16) & 0177777;
    dst = RR[reg | 1] = dst & 0177777;
    setPSWbit(PSW_BIT_N, ((i) >> 15) & 1) ;
    setPSWbit(PSW_BIT_Z, (dst | i) == 0) ;
    datapath = dst ;
}

// XOR 074RDD
void KB11::XOR(const u16 instr) {
    const auto reg = RR[REG((instr >> 6) & 7)];
    Operand op = DA<2>(instr) ;
    if (stackTrap == STACK_TRAP_RED) {
        return ;
    }
    const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
    const auto dst = reg ^ read<2>(op.operand, dpage, false);
    setNZ<2>(dst);
    write<2>(op.operand, dst, dpage);
}

// SOB 077RNN

void KB11::SOB(const u16 instr) {
    const auto reg = REG((instr >> 6) & 7);
    RR[reg]--;
    if (RR[reg]) {
        RR[7] -= (instr & 077) << 1;
    }
}

// JSR 004RDD
void KB11::JSR(const u16 instr) {
    if (((instr >> 3) & 7) == 0) {
        trap(INTINVAL);
    }
    Operand op = DA<2>(instr, false) ;
    if (stackTrap == STACK_TRAP_RED) {
        return ;
    }

    checkStackLimit(RR[6]) ;
    if (stackTrap == STACK_TRAP_RED) {
        return ;
    }

    const auto reg = REG((instr >> 6) & 7);
    push(RR[reg]);
    RR[reg] = RR[7];
    RR[7] = op.operand;
}

// JMP 0001DD
void KB11::JMP(const u16 instr) {
    if (((instr >> 3) & 7) == 0) {
        trap(INTINVAL);
    } else {
        RR[7] = DA<2>(instr, false).operand ;
    }
}

// MARK 0064NN
void KB11::MARK(const u16 instr) {
    RR[6] = RR[7] + ((instr & 077) << 1);
    RR[7] = RR[REG(5)];
    RR[REG(5)] = pop();
}

// MFPI 0065SS
void KB11::MFPI(const u16 instr) {
    // MFPI in UU mode prev UU mode operates as MFPD
    if ((PSW & 0170000) == 0170000) {
        MFPD(instr) ;
        return ;
    }

    u16 uval;
    if (!(instr & 070)) {
        const auto reg = REG(instr & 7);
        if (reg == 6 && currentmode() != previousmode()) {
            uval = stackpointer[previousmode()];
        } else {
            uval = RR[reg];
        }
    } else {
        const auto da = DA<2>(instr, false).operand;
        uval = unibus.read16(mmu.decode<false>(da, previousmode()));
    }
    setNZ<2>(uval);
    checkStackLimit(RR[6] -2) ;
    if (stackTrap == STACK_TRAP_RED) {
        return ;
    }
    push(uval);
}

// MFPD 1065SS
void KB11::MFPD(const u16 instr) {
    u16 uval;
    if (!(instr & 070)) {
        const auto reg = REG(instr & 7);
        if (reg == 6 && currentmode() != previousmode()) {
            uval = stackpointer[previousmode()];
        } else {
            uval = RR[reg];
        }
    } else {
        const auto op = DA<2>(instr, false) ;
        uval = unibus.read16(mmu.decode<false>(op.operand, previousmode(), denabled(), false));
    }
    setNZ<2>(uval);
    checkStackLimit(RR[6] -2) ;
    if (stackTrap == STACK_TRAP_RED) {
        return ;
    }
    push(uval);
}

// MTPI 0066DD
void KB11::MTPI(const u16 instr) {
    const auto uval = pop();
    if (!(instr & 0x38)) {
        const auto reg = REG(instr & 7);
        if (reg == 6 && currentmode() != previousmode()) {
            stackpointer[previousmode()] = uval;
        } else {
            RR[reg] = uval;
        }
    } else {
        const auto da = DA<2>(instr).operand;
        if (stackTrap == STACK_TRAP_RED) {
            return ;
        }
        unibus.write16(mmu.decode<true>(da, previousmode()), uval);
    }
    setNZ<2>(uval);
}

// MTPI 1066DD
void KB11::MTPD(const u16 instr) {
    const auto uval = pop();
    if (!(instr & 0x38)) {
        const auto reg = REG(instr & 7);
        setNZ<2>(uval);
        if (reg == 6 && currentmode() != previousmode()) {
            stackpointer[previousmode()] = uval;
        } else {
            RR[reg] = uval;
        }
    } else {
        const auto op = DA<2>(instr);
        if (stackTrap == STACK_TRAP_RED) {
            return ;
        }
        setNZ<2>(uval);
        unibus.write16(mmu.decode<true>(op.operand, previousmode(), denabled(), false), uval);
    }
}

// RTS 00020R
void KB11::RTS(const u16 instr) {
    const auto reg = REG(instr & 7);
    RR[7] = RR[reg];
    RR[reg] = pop();
}

// RTI 000004
void KB11::RTI() {
    RR[7] = pop() ;
    auto psw = pop() ;
    const u16 mode = currentmode() ;
    if (mode == 1) {
        psw = (psw & 0174000) | (PSW & 0174360) ;
    } else if (mode == 3) {
        psw = (psw & 0174000) | (PSW & 0174360) ;
    }

    writePSW(psw, true) ;
}

// RTT 000006
void KB11::RTT() {
    wasRTT = true ;

    RR[7] = pop() ;
    auto psw = pop() ;
    const u16 mode = currentmode() ;
    if (mode == 1) {
        psw = (psw & 0174000) | (PSW & 0174360) ;
    } else if (mode == 3) {
        psw = (psw & 0174000) | (PSW & 0174360) ;
    }

    writePSW(psw, true) ;
}

void KB11::WAIT() {
    if (currentmode() == 3) {
        return ;
    }

    datapath = RR[REG(0)] ;
    wtstate = true;
    wasRTT = false ;
}

void KB11::RESET() {
    pirqr = 0 ;
    datapath = RR[REG(0)] ;
    if (currentmode()) {
        // RESET is ignored outside of kernel mode
        return;
    }
    stacklimit = 0 ;
    unibus.reset();
    mmu.reset() ;
    // PSW = PSW & PSW_BIT_PRIORITY ;
    wtstate = false ;
    errorRegister = 0 ;
}

// SWAB 0003DD
void KB11::SWAB(const u16 instr) {
    Operand op = DA<2>(instr) ;
    if (stackTrap == STACK_TRAP_RED) {
        return ;
    }
    const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
    auto dst = read<2>(op.operand, dpage, false);
    dst = (dst << 8) | (dst >> 8);
    PSW &= 0xFFF0;
    if ((dst & 0xff) == 0) {
        PSW |= PSW_BIT_Z;
    }
    if (dst & 0x80) {
        PSW |= PSW_BIT_N;
    }
    write<2>(op.operand, dst, dpage);
    datapath = dst ;
}

// SXT 0067DD
void KB11::SXT(const u16 instr) {
    Operand op = DA<2>(instr) ;
    if (stackTrap == STACK_TRAP_RED) {
        return ;
    }
    const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
    PSW &= ~PSW_BIT_V;
    if (N()) {
        PSW &= ~PSW_BIT_Z;
        write<2>(op.operand, 0xffff, dpage);
        datapath = 0xffff ;
    } else {
        PSW |= PSW_BIT_Z;
        write<2>(op.operand, 0, dpage);
        datapath = 0 ;
    }
}

void KB11::step() {
    PC = RR[7];
    rflag = 0;
    const auto instr = fetch16();
    lda = ldat ;
    
    if (!(mmu.SR[0] & 0160000)) {
        mmu.SR[2] = PC;
    }
    
    switch (instr >> 12) {    // xxSSDD Mostly double operand instructions
        case 0:                   // 00xxxx mixed group
            switch (instr >> 8) { // 00xxxx 8 bit instructions first (branch & JSR)
                case 0:               // 000xXX Misc zero group
                    switch (instr >> 6) { // 000xDD group (4 case full decode)
                        case 0:               // 0000xx group
                            switch (instr) {
                                case 0: // HALT 000000
                                    if (currentmode()) {
                            			errorRegister = 0200 ;
                                        trap(INTBUS);
                                    }
                                    // Console::get()->printf(" HALT:\r\n");
                                    // printstate();
                                    datapath = RR[REG(0)] ;
                                    cpuStatus = CPU_STATUS_HALT ;
                                    return ;
                                case 1: // WAIT 000001
                                    WAIT();
                                    return;
                                case 3:          // BPT  000003
                                    trap(INTDEBUG); // Trap 14 - BPT
                                    return;
                                case 4: // IOT  000004
                                    trap(INTIOT);
                                    return;
                                case 5: // RESET 000005
                                    RESET();
                                    return;
                                case 2: // RTI 000002
                                    RTI();
                                    return;
                                case 6: // RTT 000006
                                    RTT();
                                    return;
                                case 7: // MFPT
                                    trap(INTINVAL);
                                    return;
                                default: // We don't know this 0000xx instruction
                                    trap(INTINVAL);
                                    return;
                            }
                        case 1: // JMP 0001DD
                            JMP(instr);
                            return;
                        case 2:                         // 00002xR single register group
                            switch ((instr >> 3) & 7) { // 00002xR register or CC
                                case 0:                     // RTS 00020R
                                    RTS(instr);
                                    return;
                                case 3: // SPL 00023N
                                    if (currentmode()) {
                                        return ;
                                    }
                                    PSW = ((PSW & 0177037) | ((instr & 7) << 5));
                                    wasSPL = true ;
                                    return;
                                case 4: // CLR CC 00024C Part 1 without N
                                case 5: // CLR CC 00025C Part 2 with N
                                    PSW = (PSW & ~(instr & 017));
                                    return;
                                case 6: // SET CC 00026C Part 1 without N
                                case 7: // SET CC 00027C Part 2 with N
                                    PSW = (PSW | (instr & 017));
                                    return;
                                default: // We don't know this 00002xR instruction
                                    trap(INTINVAL);
                                    return;
                            }
                        case 3: // SWAB 0003DD
                            SWAB(instr);
                            return;
                        default:
                            trap(INTINVAL);
                            return;
                    }
                case 1: // BR 0004 offset
                    branch(instr);
                    return;
                case 2: // BNE 0010 offset
                    if (!Z()) {
                        branch(instr);
                    }
                    return;
                case 3: // BEQ 0014 offset
                    if (Z()) {
                        branch(instr);
                    }
                    return;
                case 4: // BGE 0020 offset
                    if (!(N() xor V())) {
                        branch(instr);
                    }
                    return;
                case 5: // BLT 0024 offset
                    if (N() xor V()) {
                        branch(instr);
                    }
                    return;
                case 6: // BGT 0030 offset
                    if ((!(N() xor V())) && (!Z())) {
                        branch(instr);
                    }
                    return;
                case 7: // BLE 0034 offset
                    if ((N() xor V()) || Z()) {
                        branch(instr);
                    }
                    return;
                case 8: // JSR 004RDD In two parts
                case 9: // JSR 004RDD continued (9 bit instruction so use 2 x 8 bit
                    JSR(instr);
                    return;
                default: // Remaining 0o00xxxx instructions where xxxx >= 05000
                    switch (instr >> 6) { // 00xxDD
                        case 050:             // CLR 0050DD
                            CLR<2>(instr);
                            return;
                        case 051: // COM 0051DD
                            COM<2>(instr);
                            return;
                        case 052: // INC 0052DD
                            INC<2>(instr);
                            return;
                        case 053: // DEC 0053DD
                            _DEC<2>(instr);
                            return;
                        case 054: // NEG 0054DD
                            NEG<2>(instr);
                            return;
                        case 055: // ADC 0055DD
                            _ADC<2>(instr);
                            return;
                        case 056: // SBC 0056DD
                            SBC<2>(instr);
                            return;
                        case 057: // TST 0057DD
                            TST<2>(instr);
                            return;
                        case 060: // ROR 0060DD
                            ROR<2>(instr);
                            return;
                        case 061: // ROL 0061DD
                            ROL<2>(instr);
                            return;
                        case 062: // ASR 0062DD
                            ASR<2>(instr);
                            return;
                        case 063: // ASL 0063DD
                            ASL<2>(instr);
                            return;
                        case 064: // MARK 0064nn
                            MARK(instr);
                            return;
                        case 065: // MFPI 0065SS
                            MFPI(instr);
                            return;
                        case 066: // MTPI 0066DD
                            MTPI(instr);
                            return;
                        case 067: // SXT 0067DD
                            SXT(instr);
                            return;
                        default: // We don't know this 0o00xxDD instruction
                            trap(INTINVAL);
                            return;
                    }
                }
        case 1: // MOV  01SSDD
            MOV<2>(instr);
            return;
        case 2: // CMP 02SSDD
            CMP<2>(instr);
            return;
        case 3: // BIT 03SSDD
            _BIT<2>(instr);
            return;
        case 4: // BIC 04SSDD
            BIC<2>(instr);
            return;
        case 5: // BIS 05SSDD
            BIS<2>(instr);
            return;
        case 6: // ADD 06SSDD
            ADD(instr);
            return;
        case 7:                         // 07xRSS instructions
            switch ((instr >> 9) & 7) { // 07xRSS
                case 0:                     // MUL 070RSS
                    MUL(instr);
                    return;
                case 1: // DIV 071RSS
                    DIV(instr);
                    return;
                case 2: // ASH 072RSS
                    ASH(instr);
                    return;
                case 3: // ASHC 073RSS
                    ASHC(instr);
                    return;
                case 4: // XOR 074RSS
                    XOR(instr);
                    return;
                case 7: // SOB 077Rnn
                    SOB(instr);
                    return;
                default: // We don't know this 07xRSS instruction
                    trap(INTINVAL);
                    return;
            }
        case 8:                           // 10xxxx instructions
            switch ((instr >> 8) & 0xf) { // 10xxxx 8 bit instructions first
                case 0:                       // BPL 1000 offset
                    if (!N()) {
                        branch(instr);
                    }
                    return;
                case 1: // BMI 1004 offset
                    if (N()) {
                        branch(instr);
                    }
                    return;
                case 2: // BHI 1010 offset
                    if ((!C()) && (!Z())) {
                        branch(instr);
                    }
                    return;
                case 3: // BLOS 1014 offset
                    if (C() || Z()) {
                        branch(instr);
                    }
                    return;
                case 4: // BVC 1020 offset
                    if (!V()) {
                        branch(instr);
                    }
                    return;
                case 5: // BVS 1024 offset
                    if (V()) {
                        branch(instr);
                    }
                    return;
                case 6: // BCC 1030 offset
                    if (!C()) {
                        branch(instr);
                    }
                    return;
                case 7: // BCS 1034 offset
                    if (C()) {
                        branch(instr);
                    }
                    return;
                case 8:          // EMT 104xxx operand
                    trap(INTEMT); // Trap 30 - EMT instruction
                case 9:          // TRAP 1044 operand
                    trap(INTTRAP); // Trap 34 - TRAP instruction
                default: // Remaining 10xxxx instructions where xxxx >= 05000
                    switch ((instr >> 6) & 077) { // 10xxDD group
                        case 050:                     // CLRB 1050DD
                            CLR<1>(instr);
                            return;
                        case 051: // COMB 1051DD
                            COM<1>(instr);
                            return;
                        case 052: // INCB 1052DD
                            INC<1>(instr);
                            return;
                        case 053: // DECB 1053DD
                            _DEC<1>(instr);
                            return;
                        case 054: // NEGB 1054DD
                            NEG<1>(instr);
                            return;
                        case 055: // ADCB 01055DD
                            _ADC<1>(instr);
                            return;
                        case 056: // SBCB 01056DD
                            SBC<1>(instr);
                            return;
                        case 057: // TSTB 1057DD
                            TST<1>(instr);
                            return;
                        case 060: // RORB 1060DD
                            ROR<1>(instr);
                            return;
                        case 061: // ROLB 1061DD
                            ROL<1>(instr);
                            return;
                        case 062: // ASRB 1062DD
                            ASR<1>(instr);
                            return;
                        case 063: // ASLB 1063DD
                            ASL<1>(instr);
                            return;
                        case 065: // MFPD 1065DD
                            MFPD(instr);
                            return;
                        case 066:
                            MTPD(instr); // MTPD 1066DD
                            return;
                        // case 0o67: // MTFS 1064SS
                        default: // We don't know this 0o10xxDD instruction
                            trap(INTINVAL);
                            return;
                    }
            }
        case 9: // MOVB 11SSDD
            MOV<1>(instr);
            return;
        case 10: // CMPB 12SSDD
            CMP<1>(instr);
            return;
        case 11: // BITB 13SSDD
            _BIT<1>(instr);
            return;
        case 12: // BICB 14SSDD
            BIC<1>(instr);
            return;
        case 13: // BISB 15SSDD
            BIS<1>(instr);
            return;
        case 14: // SUB 16SSDD
            SUB(instr);
            return;
        case 15:
            fp11(instr);
            break;
            [[fallthrough]];
        default: // 15  17xxxx FPP instructions
            trap(INTINVAL);
            return ;
    }
}

void KB11::calc_irqs() {
    irq_vec = 0 ;
    u8 pri = 0 ;
    
    for (int i = 0; i < 128; i++) {
        if (!irqs[i]) {
            continue ;
        }

        if (irqs[i] >= pri) {
            irq_vec = i << 1;
            pri = irqs[i] ;
        }
    }
}

u8 KB11::interrupt_vector() {
    if (irq_dirty) {
        calc_irqs() ;
        irq_dirty = false ;
    }

    if (!irq_vec) {
        return 0 ;
    }

    u8 v = irq_vec >> 1;

    if (irqs[v] > cpuPriority) {
        irqs[v] = 0 ;
        irq_vec = 0 ;
        irq_dirty = true ;
        return v << 1;
    }

    return 0 ;
}

void KB11::interrupt(const u8 vec, const u8 pri) {
    if (vec & 1) {
        gprintf("Thou darst calling interrupt() with an odd vector number?\n");
        while(!interrupted);
    }

    irqs[vec >> 1] = pri ;
    irq_dirty = true ;
}

void KB11::trapat(u8 vec) {
    if (vec & 1) {
        gprintf("Thou darst calling trapat() with an odd vector number?");
        while(!interrupted) ;
    }

    u16 PC = RR[7] ;
    u16 opsw = PSW;
    
    if (RR[6] & 1) {
        vec = INTBUS ;
    }

    auto npsw = unibus.read16(mmu.decode<false>(vec, 0, mmu.SR[3] & 4, true) + 2);
    writePSW((npsw & ~PSW_BIT_PRIV_MODE) | (currentmode() << 12), true);
    RR[7] = unibus.read16(mmu.decode<false>(vec, 0, mmu.SR[3] & 4, true));       // Get from K-apace
    if ((RR[6] & 1) == 0) {
        RR[6] -= 4 ;
        writeW(RR[6]+2, opsw, mmu.SR[3] & 4) ;
        writeW(RR[6],   PC,   mmu.SR[3] & 4) ;
    }
    wtstate = false;

    if (cpuStatus != CPU_STATUS_ENABLE) {
        CLogger::Get()->Write("KB11", LogError, "trapat vec:%03o, at:%06o, oldPSW:%06o, to:%06o, npsw:%06o, newPSW:%06o", vec, PC, opsw, RR[7], npsw, PSW) ;
    }
}

void KB11::printstate() {
    ptstate();
}

static const char *modnames[4] = {
    "KK", "SS", "--", "UU"
} ;

void KB11::ptstate() {
    Console::get()->printf("    R%d %06o R%d %06o R%d %06o R%d %06o\r\n", REGNAME(0), RR[REG(0)], REGNAME(1), RR[REG(1)], REGNAME(2), RR[REG(2)], REGNAME(3), RR[REG(3)]);
    Console::get()->printf("    R%d %06o R%d %06o R6 %06o PS %06o\r\n", REGNAME(4), RR[REG(4)], REGNAME(5), RR[REG(5)], RR[6], PSW);
    
    Console::get()->printf("    PSW [%s%s%s%s%s",
        modnames[currentmode()],
        N() ? "N" : "-",
        Z() ? "Z" : "-",
        V() ? "V" : "-",
        C() ? "C" : "-");
    Console::get()->printf("]\r\n    instr %06o: %06o   ", RR[7], readW(RR[7]));

    disasm(RR[7]);

    Console::get()->printf("\r\n");
}

void KB11::pirq() {
    if (pirqr == 0) {
        clearIRQ(INTPIR) ;
        return ;
    }

    u8 pri = (pirqr >> 1) & 7 ;
    interrupt(INTPIR, pri) ;
}
