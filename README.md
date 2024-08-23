# KA ReClassEx

Pre-compiled:
1. Load procexp driver
2. Map kernel.sys inside of BIN folder
3. Run RCE64.exe as admin

## New
- Kernel driver plugin (RCEPlugin)
- All icons changed
- All "ReClass" strings removed
- Highlight bytes in red on change
- Pointer section name
- Pointer function name based on export table
- Scroll faster pressing CTRL (like IDA)
- Show "Invalid Pointer" when Pointer type is wrongly set

## New Hot keys

- CTRL + S = save classes

- UP/DOWN arrow or W/S to navigate

- SPACE / TAB / ENTER = close/open classes and pointers

### Set types

- R = Reset (Set Hex64)
- P = Set Pointer
- F = Set Float
- D = Set Double
- I = Set Int (int32)

- F1 = Add 4
- F2 = Add 8
- F3 = Add 64
- F4 = Add 1024
- F5 = Add 2048

- 1 = Insert 4
- 2 = Insert 8
- 3 = Insert 64
- 4 = Insert 1024
- 5 = Insert 2048