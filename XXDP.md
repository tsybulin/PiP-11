# XXDP

- Copy test file to SD card as PTR.TAP
- Boot to BOOTMON (configuration 1..)
- Load tape (b rp) or (h g 157500)
- Reset CPU (r)
- Run test (g 200)


## A list of tests

### Z = unspecified / any platform

| NAME   | DESCRIPTION | STATE     |
| ------ | ----------- | :-------: |
| ZKAAA0 | PDP11 INSTRUCTION (BRANCH) TEST 1 | **+** |
| ZKABA0 | PDP11 INSTR. (CON. BRANCH) TEST 2 | **+** |
| ZKACA0 | PDP11 INSTR. (UNARY) TEST 3 | **+** |
| ZKADA0 | PDP11 INSTR. (UNARY & BINARY) TEST 4 | **+** |
| ZKAEA0 | PDP11 INSTR. (ROTATE & SHIFT) TEST 5 | **+** |
| ZKAFA0 | PDP11 INSTR. (COMPARE) TEST 6 | **+** |
| ZKAGA0 | PDP11 INSTR. (NOT COMPARE) TEST 7 | **+** |
| ZKAHA0 | PDP11 INSTR. (MOVE) TEST 8 | **+** |
| ZKAIA0 | PDP11 INSTR. (BIS,BIC,BIT) TEST 9 | **+** |
| ZKAJA0 | PDP11 INSTR. (ADD) TEST 10 | **+** |
| ZKAKA0 | PDP11 INSTR. (SUBTR.) TEST 11 | **+** |
| ZKALA0 | PDP11 INSTR. (JUMP) TEST 12 | **+** |
| ZKAMA0 | PDP11 INSTR. (JSR,RIS RTI) TEST 13 | **+** |


### C = 11/45

| NAME   | DESCRIPTION | STATE     |
| ------ | ----------- | :-------: |
| CKBAB0 | 11/35/40/45 SXT INSTRUCTION TEST | **+** |
| CKBBB0 | 11/35/40/45 SOB INSTRUCTION TEST | **+** |
| CKBCB0 | 11/35/40/45 XOR INSTRUCTION TEST | **+** |
| CKBDC0 | 11/35/40/45 MARK INSTRUCTION TEST | **+** |
| CKBEC0 | 11/35/40/45 RTI/RTT INSTRUCTION TEST | **+** |
| CKBFD0 | 11/35/40/45 STACK LIMIT TEST | **+** |
| CKBGB0 | 11/35/40/45 SPL INSTRUCTION TEST  | **+** |
| CKBHB0 | 11/45 REGISTER TEST | **+** |
| CKBIB0 | 11/45 ASH INSTRUCTION TEST | **+** |
| CKBJA0 | 11/45 ASHC INSTRUCTION TEST | **+** |
| CKBKA0 | 11/45 MUL INSTRUCTION TEST | **+** |
| CKBLA0 | 11/45 DIV INSTRUCTION TEST | **+** |
| CKBME0 | 11/45 CPU TRAP TEST | x |
| CKBNC0 | 11/45 PIRQ TEST | **+** |
| CKBOA0 | 11/45 STATES (USER, KERNEL) TEST | x |
| CKTAB0 | 11/45 KT11-C MEMORY MANAG. LOGIC TEST 1 | **+** |
| CKTBC0 | 11/45 KT11-C MEMORY MANAG. LOGIC TEST 2 | **+** |
| CKTCA0 | 11/45 KT11-C MEMORY MANAG. LOGIC TEST 3 | **+** |
| CKTDA0 | 11/45 KT11-C MTPD/I TEST | **+** |
| CKTEB0 | 11/45 KT11-C MFPD/I TEST | **+** |
| ~~CKTFD0~~ | 11/45 KT11-C Abort logic TEST | x |

### E = 11/70

| NAME   | DESCRIPTION | STATE     |
| ------ | ---------: | ----------- |
| EKBAD0 | 11/70 CPU DIAGNOSTIC PART 1 | **+** |
