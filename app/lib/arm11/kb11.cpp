#include "kb11.h"

#include <circle/setjmp.h>

#include "bootmon.h"
#include "bootstrap.h"
#include "boot_defs.h"
#include <cons/cons.h>

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
    for (int i = 0; i < 255; i += 2) {
        irqs[i] = IRQ_EMPTY ;
    }
}

void KB11::reset(u16 start) {
    for (int i = 0; i < BOOTSTRAP_LENGTH; i++) {
        unibus.write16(BOOTSTRAP_BASE + (i * 2), bootstrap[i]);
    }

    for (int i = 0; i < ABSLOADER_LENGTH; i++) {
        unibus.write16(ABSLOADER_BASE + (i * 2), absloader[i]);
    }

    for (int i = 0; i < BOOTMON_LENGTH; i++) {
        unibus.write16(BOOTMON_BASE + (i * 2), bootmon[i]);
    }

    RR[7] = start;
    stacklimit = 0400 ;
    switchregister = 0;
    unibus.reset(false);
    wtstate = false;
}

void KB11::write16(const u16 va, const u16 v, bool d, bool src) {
    const auto a = mmu.decode<true>(va, currentmode(), d, src);
    switch (a) {
        case 0777772: {
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
                irqs[INTPIR] = IRQ_EMPTY ;
            }

            break ;
        case 0777776:
            writePSW(v);
            break;
        case 0777774:
            stacklimit = v & 0177400 ;
            break;
        case 0777770:
            microbrreg = v ;
            break ;
        case 0777570:
            displayregister = v;
            mmu.T = v ;
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
}

// MUL 070RSS
void KB11::MUL(const u16 instr) {
	const auto reg = REG((instr >> 6) & 7);
	s32 val1 = RR[reg];
	if (val1 & 0x8000) {
		val1 = -((0xFFFF ^ val1) + 1);
	}

    Operand op = DA<2>(instr) ;
    const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
	s32 val2 = read<2>(op.operand, dpage, false);
	if (val2 & 0x8000) {
		val2 = -((0xFFFF ^ val2) + 1);
	}
	s32 sval = val1 * val2;
	RR[reg] = sval >> 16;
	RR[reg | 1] = sval & 0xFFFF;
	PSW &= PSW_MASK_COND ;
    setPSWbit(PSW_BIT_N, sval < 0) ;
    setPSWbit(PSW_BIT_Z, sval == 0) ;
    setPSWbit(PSW_BIT_C, (sval > 077777) || (sval < -0100000)) ;
}

void KB11::DIV(const u16 instr) {
	const auto reg = REG((instr >> 6) & 7);
	const s32 val1 = (RR[reg] << 16) | (RR[reg | 1]);
    Operand op = DA<2>(instr) ;
    const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
	s32 val2 = read<2>(op.operand, dpage, false);
	PSW &= 0xFFF0;
	if (val2 > 32767)
		val2 |= 0xffff0000;
	if (val2 == 0) {
		PSW |= PSW_BIT_C | PSW_BIT_V;
		return;
	}
	if ((val1 / val2) >= 0x10000) {
		PSW |= PSW_BIT_V;
		return;
	}
	RR[reg] = (val1 / val2) & 0xFFFF;
	RR[reg | 1] = (val1 % val2) & 0xFFFF;
	if (RR[reg] == 0) {
		PSW |= PSW_BIT_Z;
	}
	if (RR[reg] & 0100000) {
		PSW |= PSW_BIT_N;
	}
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

void KB11::FIS(const u16 instr)
{
    union fltint {
        u32 xint;
        float xflt;
    };
    // u32 arg1, arg2;
    fltint bfr;
    u16 adr = RR[REG(instr & 7)];      // Base address from specfied reg
    float op1, op2;

    bfr.xint = read<2>(adr + 6) | (read<2>(adr + 4) << 16);
    op1 = bfr.xflt;
    bfr.xint = read<2>(adr + 2) | (read<2>(adr) << 16);
    op2 = bfr.xflt;
    PSW &= ~(PSW_BIT_N | PSW_BIT_V | PSW_BIT_Z | PSW_BIT_C);
    switch (instr & 070) {
    case 0:                 // FADD
        bfr.xflt = op1 + op2;
        break;
    case 010:
        bfr.xflt = op1 - op2;
        break;
    case 020:
        bfr.xflt = (op1 * op2) / 4.0f;
        break;
    case 030:
        if (op2 == 0.0) {
            PSW |= (PSW_BIT_N | PSW_BIT_V | PSW_BIT_C);
            trap(INTFIS);
        }
        bfr.xflt = (op1 / op2) * 4.0f;
        break;
    }
    RR[REG(instr & 7)] += 4;
    if (bfr.xflt == 0.0)
        PSW |= PSW_BIT_Z;
    if (bfr.xflt < 0.0)
        PSW |= PSW_BIT_N;
    write<2>(adr + 4, bfr.xint >> 16);
    write<2>(adr + 6, bfr.xint);
}


// MTPS

void KB11::MTPS(const u16 instr) {
    Operand op = DA<1>(instr, false) ;
    const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
    auto src = read<1>(op.operand, dpage, false);
    PSW = (PSW & 0177400) | (src & 0357);
}

// MFPS

void KB11::MFPS(const u16 instr) {
    Operand op = DA<1>(instr) ;
    if (stackTrap == STACK_TRAP_RED) {
        return ;
    }
    const bool dpage = denabled() && op.operandType == OPERAND_DATA ;
    auto dst = PSW & 0357;
    if (PSW & msb<1>() && ((instr & 030) == 0)) {
        dst |= 0177400;
        write<2>(op.operand, dst, dpage) ;
    } else{
        write<1>(op.operand, dst, dpage) ;
    }
    setNZ<1>(PSW & 0377);
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
    const auto reg = REG((instr >> 6) & 7);
    push(RR[reg]);
    RR[reg] = RR[7];
    RR[7] = op.operand;
}

// JMP 0001DD
void KB11::JMP(const u16 instr) {
    if (((instr >> 3) & 7) == 0) {
        // Registers don't have a virtual address so trap!
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
    if (!(instr & 0x38)) {
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
    push(uval);
    setNZ<2>(uval);
}

// MFPD 1065SS
void KB11::MFPD(const u16 instr) {
    u16 uval;
    if (!(instr & 0x38)) {
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
    push(uval);
    setNZ<2>(uval);
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

// MFPT 000007
void KB11::MFPT() {
    trap(INTINVAL); // not a PDP11/44
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
    if (currentmode()) {
        return ;
    }
    wtstate = true;
}

void KB11::RESET() {
    pirqr = 0 ;
    if (currentmode()) {
        // RESET is ignored outside of kernel mode
        return;
    }
    stacklimit = 0 ;
    unibus.reset();
    mmu.SR[0]=0;
    mmu.SR[3]=0;
    PSW = PSW & PSW_BIT_PRIORITY ;
    wtstate = false ;
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
    } else {
        PSW |= PSW_BIT_Z;
        write<2>(op.operand, 0, dpage);
    }
}

void KB11::step() {
    PC = RR[7];
    rflag = 0;
    const auto instr = fetch16();
    
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
                                        trap(INTBUS);
                                    }
                                    // Console::get()->printf(" HALT:\r\n");
                                    // printstate();
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
                                    MFPT();
                                    return;
                                default: // We don't know this 0000xx instruction
                                    gprintf("unknown 0000xx instruction\n");
                                    printstate();
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
                                    gprintf("unknown 0002xR instruction\n");
                                    printstate();
                                    trap(INTINVAL);
                                    return;
                            }
                        case 3: // SWAB 0003DD
                            SWAB(instr);
                            return;
                        default:
                            gprintf("unknown 000xDD instruction\n");
                            printstate();
                            trapat(INTINVAL);
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
                            gprintf("unknown 00xxDD instruction\n");
                            printstate();
                            trapat(INTINVAL);
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
                case 5: // FIS
                    FIS(instr);
                    return;
                case 6:
                    //printf("Invalid instruction:%06o\r\n",instr);
                    trap(INTINVAL);
                case 7: // SOB 077Rnn
                    SOB(instr);
                    return;
                default: // We don't know this 07xRSS instruction
                    gprintf("unknown 07xRSS instruction\n");
                    printstate();
                    trapat(INTINVAL);
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
                case 8:          // EMT 1040 operand
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
                        case 064: // MTPS 1064SS
                            MTPS(instr);
                            return;
                        case 067: // MFPS 106700
                            MFPS(instr);
                            return;
                        case 065: // MFPD 1065DD
                            MFPD(instr);
                            return;
                        case 066:
                            MTPD(instr); // MTPD 1066DD
                            return;
                        // case 0o67: // MTFS 1064SS
                        default: // We don't know this 0o10xxDD instruction
                            gprintf("unknown 0o10xxDD instruction\n");
                            printstate();
                            trapat(INTINVAL);
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
            //printf("invalid 17xxxx FPP instruction\n");
            //return;
            //printstate();
            trap(INTINVAL);
    }
}

void KB11::interrupt(const u8 vec, const u8 pri) {
    if (vec & 1) {
        gprintf("Thou darst calling interrupt() with an odd vector number?\n");
        while(!interrupted);
    }

    if ((irqs[vec] & 7) > pri) {
        return ;
    }

    irqs[vec] = pri ;
    wtstate = false;
}

void KB11::trapat(u8 vec) {
    if (vec & 1) {
        gprintf("Thou darst calling trapat() with an odd vector number?");
        while(!interrupted) ;
    }

    u16 PC = RR[7] ;

    auto opsw = PSW;
    auto npsw = unibus.read16(mmu.decode<false>(vec, 0, mmu.SR[3] & 4, true) + 2);
    writePSW((npsw & ~PSW_BIT_PRIV_MODE) | (currentmode() << 12), true);
    push(opsw);
    push(RR[7]);
    RR[7] = unibus.read16(mmu.decode<false>(vec, 0, mmu.SR[3] & 4, true));       // Get from K-apace
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
    Console::get()->printf("]\r\n    instr %06o: %06o   ", RR[7], read16(RR[7]));

    disasm(RR[7]);

    Console::get()->printf("\r\n");
}

void KB11::pirq() {
    if (pirqr == 0) {
        return ;
    }

    u8 pri = (pirqr >> 1) & 7 ;
    interrupt(INTPIR, pri) ;
}
