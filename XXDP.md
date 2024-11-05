# XXDP

- Copy test file to SD card as PTR.TAP
- Boot to BOOTMON (configuration 1)
- Load tape (b rp)
- Optionally patch test visual notification
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
| ZKAGA0 | ? | PDP11 INSTR. (NOT COMPARE) TEST 7 | passsed |
| ZKAHA0 | ? | PDP11 INSTR. (MOVE) TEST 8 | passed |
| ZKAIA0 | ? | PDP11 INSTR. (BIS,BIC,BIT) TEST 9 | passed |
| ZKAJA0 | ? | PDP11 INSTR. (ADD) TEST 10 | passed |
| ZKAKA0 | ? | PDP11 INSTR. (SUBTR.) TEST 11 | passed |
| ZKALA0 | ? | PDP11 INSTR. (JUMP) TEST 12 | passed |


### C = 11/45
