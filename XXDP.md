# XXDP

- Copy test file to SD card as PTR.TAP
- Boot to BOOTMON (configuration 1)
- Load tape (b rp)
- Optionally patch test visual notification
- Reset CPU (r)
- Run test (g 200)

Patch : check and replace Bell / Visual Bell (7 / 207) symbol with '*'

    e addr
    d addr 52

## A list of tests

### Z = unspecified / any platform

| NAME   | PATCH addr | DESCRIPTION | STATE     |
| ------ | ---------: | ----------- | --------- |
| ZKAAA0 | 014212 | PDP11 INSTRUCTION (BRANCH) TEST 1 | passed |
| ZKABA0 |  04336 | PDP11 INSTR. (CON. BRANCH) TEST 2 | passed |
| ZKACA0 |  05526 | PDP11 INSTR. (UNARY) TEST 3 | passed |
| ZKADA0 | 016370 | PDP11 INSTR. (UNARY & BINARY) TEST 4 | passed |
| ZKAEA0 | ? | PDP11 INSTR. (ROTATE & SHIFT) TEST 5 | passed |
| ZKAFA0 | ? | PDP11 INSTR. (COMPARE) TEST 6 | passed |
| ZKAGA0 | ? | PDP11 INSTR. (NOT COMPARE) TEST 7 | passed |
| ZKAHA0 | ? | PDP11 INSTR. (MOVE) TEST 8 | passed |
| ZKAIA0 | ? | PDP11 INSTR. (BIS,BIC,BIT) TEST 9 | passed |
| ZKAJA0 | ? | PDP11 INSTR. (ADD) TEST 10 | passed |
| ZKAKA0 | ? | PDP11 INSTR. (SUBTR.) TEST 11 | passed |
| ZKALA0 | ? | PDP11 INSTR. (JUMP) TEST 12 | passed |
| ZKAMA0 | ? | PDP11 INSTR. (JSR,RIS RTI) TEST 13 | passed |
| ZKLAE0 | ? | KL11 / DL11-A TELETYPE TEST | ? |


### C = 11/45

| NAME   | PATCH addr | DESCRIPTION | STATE     |
| ------ | ---------: | ----------- | --------- |
| CKBAB0 | ? | 11/35/40/45 SXT INSTRUCTION TEST | passed |
| CKBBB0 | ? | 11/35/40/45 SOB INSTRUCTION TEST | passed |
| CKBCB0 | ? | 11/35/40/45 XOR INSTRUCTION TEST | passed |
| CKBDC0 | ? | 11/35/40/45 MARK INSTRUCTION TEST | passed |
| CKBEC0 | ? | 11/35/40/45 RTI/RTT INSTRUCTION TEST | passed |
| CKBFD0 | ? | 11/35/40/45 STACK LIMIT TEST | passed |
| CKBGB0 | ? | 11/35/40/45 SPL INSTRUCTION TEST  | **passed?** |
| CKBHB0 | ? | 11/45 REGISTER TEST | passed |
| CKBIB0 | ? | 11/45 ASH INSTRUCTION TEST | passed |
| CKBJA0 | ? | 11/45 ASHC INSTRUCTION TEST | passed |
| CKBKA0 | ? | 11/45 MUL INSTRUCTION TEST | passed |
| CKBLA0 | ? | 11/45 DIV INSTRUCTION TEST | passed |
| CKBME0 | ? | 11/45 CPU TRAP TEST | **failed** |
| CKBNC0 | ? | 11/45 PIRQ TEST | passed |
| CKBOA0 | ? | 11/45 STATES (USER, KERNEL) TEST | **failed** |
| CKTAB0 | 017412 | 11/45 KT11-C MEMORY MANAG. LOGIC TEST 1 | passed |
| CKTBC0 | ? | 11/45 KT11-C MEMORY MANAG. LOGIC TEST 2 | passed |
| CKTCA0 | ? | 11/45 KT11-C MEMORY MANAG. LOGIC TEST 3 | passed |
| CKTDA0 | ? | 11/45 KT11-C MTPD/I TEST | passed |
| CKTEB0 | ? | 11/45 KT11-C MFPD/I TEST | passed |
| CKTEB0 | ? | 11/45 KT11-C MFPD/I TEST | passed |
| CKTFD0 | ? | 11/45 KT11-C Abort logic TEST | **failed** |
