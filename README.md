My C implementation of [jmeiners' LC-3 VM tutorial](https://www.jmeiners.com/lc3-vm/), 

# About [LC-3 (Little Computer 3)](https://en.wikipedia.org/wiki/Little_Computer_3)

LC-3 is an educational 16-bit computer architecture with 16 opcodes and 8 
general-purpose registers. However, R7 is usually reserved for return addresses.
Data is stored in two's complement representation. Some I/O operations are 
provided through TRAP instructions.

# How to

## Setup
Install `gcc`. You will use `gcc` to compile your C file into an executable.
- MacOS: `brew install gcc`
- Unix: `sudo apt-get install gcc`

## Optional setup: assembler
If you want to be able to write any of your own intermediate test cases in 
Assembly, set up the 
[LC-3 tools](https://highered.mheducation.com/sites/0072467509/student_view0/lc-3_simulator.html).

Follow their setup instructions. You should then be able to run the assembler 
on an assembly file, like so:

`lc3as roms/hello.asm`

## Emulate a rom
Assembly files and assembled roms live in `roms/`.

To run a rom on the VM, pass the `.obj` file path to the VM executable.

`./lc3-vm roms/hello.obj`

If you have not implemented anything yet, you should see an error message 
about an op not being implemented.

If you installed the LC-3 tools, you can use my Make targets as shorthand to 
assemble new roms. For example, to reassemble and run `roms/hello.asm`, do 
`make run rom=roms/hello`.

## Compile the VM

When you make changes to the LC-3 implementation, you'll want to recompile the 
C code to an executable.

You can use `make compile` as shorthand. To see what it does, read `Makefile`.

# Assembly instruction usage

Instruction | Outcome
--- | ---
ADD R0 R1 R2 | R0 = R1 + R2
ADD R0 R1 #  | R0 = R1 + #, where # < 2^5
AND R0 R1 R2 | R0 = R1 & R2
AND R0 R1 #  | R0 = R1 & #, where # < 2^5
NOT R0 R1    | R0 = ^R1
BRn LABEL    | Jump if result is -
BRz LABEL    | Jump if result == 0
BRp LABEL    | Jump if result is +
BRzp LABEL   | Jump if result is 0 OR + 
BRnp LABEL   | Jump if result is - OR +
BRnz LABEL   | Jump if result is - OR 0
BRnzp LABEL  | Jump if result is - || 0 || +
JMP R0       | Jump to addr in reg (may use R0-R6)
JSR LABEL    | Jump to label
JSRR R0      | Jump to addr in R0
RET          | Return to addr in R7
