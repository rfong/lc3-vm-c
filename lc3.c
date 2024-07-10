/* INCLUDES */
#include <execinfo.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
/* unix only */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

#include "lc3.h"
#include "truth_tables.c"

/* Memory storage */
uint16_t memory[MEMORY_MAX];  /* 65,536 locations */
/* Register storage */
uint16_t reg[R_COUNT];

/* Input buffering */
struct termios original_tio;

void disable_input_buffering() {
  tcgetattr(STDIN_FILENO, &original_tio);
  struct termios new_tio = original_tio;
  new_tio.c_lflag &= ~ICANON & ~ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering() {
  tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

uint16_t check_key() {
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(STDIN_FILENO, &readfds);

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

/* Handle interrupt signal */
void handle_interrupt(int signal) {
  void *array[10];
  size_t size;
  // get void*'s for all entries on stack
  size = backtrace(array, 10);
  // print frames to stderr
  fprintf(stderr, "Error: signal %d:\n", signal);
  backtrace_symbols_fd(array, size, STDERR_FILENO);

  restore_input_buffering();
  printf("\n");
  exit(-2);
}

/* Sign extend a `bit_count`-length signed num to a 16-bit signed num.
 * https://en.wikipedia.org/wiki/Two%27s_complement
 */
uint16_t sign_extend(uint16_t x, int bit_count) {
  /* If negative (sign bit is 1), fill in the left bits with 1s.
   * Otherwise just return the value.
   */
  if ((x >> (bit_count - 1)) & 1) {
    x |= (0xFFFF << bit_count);
  }
  return x;
}

/* Swap endian-ness */
uint16_t swap16(uint16_t x) {
  return (x << 8) | (x >> 8);
}

/* Update flags any time a value is written */
void update_flags(uint16_t r) {
  if (reg[r] == 0) {
    reg[R_COND] = FL_ZRO;
  } else if (reg[r] >> 15) { // a 1 in the left-most bit indicates negative
    reg[R_COND] = FL_NEG;
  } else {
    reg[R_COND] = FL_POS;
  }
}

/* Read an LC-3 image file into memory */
void read_image_file(FILE* file) {
  // The origin tells us where in memory to place the image
  uint16_t origin;
  fread(&origin, sizeof(origin), 1, file);
  origin = swap16(origin);

  // We know the maximum file size so we only need one fread
  uint16_t max_read = MEMORY_MAX - origin;
  uint16_t* p = memory + origin;
  size_t read = fread(p, sizeof(uint16_t), max_read, file);

  // Swap to little endian
  while (read-- > 0) {
    *p = swap16(*p);
    ++p;
  }
}

/* Read an LC-3 program, given the path to the image file */
int read_image(const char* image_path) {
  FILE* file = fopen(image_path, "rb");
  if (!file) { return 0; };
  read_image_file(file);
  fclose(file);
  return 1;
}

/* Memory access */
void mem_write(uint16_t address, uint16_t val) {
  memory[address] = val;
}
uint16_t mem_read(uint16_t address) {
  if (address == MR_KBSR) {
    if (check_key()) {
      memory[MR_KBSR] = (1 << 15);
      memory[MR_KBDR] = getchar();
    }
    else {
      memory[MR_KBSR] = 0;
    }
  }
  return memory[address];
}

/* Opcode implementations
 *   `instr` is the rightmost 12 bits of instruction */
// Some things that need to get parsed often
//uint16_t r0; // bits [11:9]
uint16_t r1; // bits [8:6]
uint16_t imm_flag; // bit [5]

void op_add(uint16_t instr) {
  /* ADD: Add together two values.
   * normal mode: ADD R2 R0 R1 ; R2 = R0 + R1
   * imm mode: ADD R0 R0 val(0-31) ; R0 += val
  */
  // destination register (DR): bits [11:9]
  uint16_t r0 = (instr >> 9) & 0x7;

  if (imm_flag) {
    // If in imm mode, sign-extend the rightmost 5 bits as 2nd value.
    uint16_t imm5 = sign_extend(instr & 0x1F, 5);
    reg[r0] = reg[r1] + imm5;
  } else {
    // If not in imm mode, get second operand (SR2): bits [2:0]
    uint16_t r2 = instr & 0x7;
    reg[r0] = reg[r1] + reg[r2];
  }
  update_flags(r0);
}

void op_and(uint16_t instr) {
  /* AND: bitwise-and together two values.
   * normal mode: AND R2 R0 R1 ; R2 = R0 & R1
   * imm mode: AND R0 R0 val(0-31) ; R0 &= val
  */
  // destination register (DR): bits [11:9]
  uint16_t r0 = (instr >> 9) & 0x7;

  if (imm_flag) {
    // If in imm mode, sign-extend the rightmost 5 bits as 2nd value.
    uint16_t imm5 = sign_extend(instr & 0x1F, 5);
    reg[r0] = reg[r1] & imm5;
  } else {
    // If not in imm mode, get second operand (SR2): bits [2:0]
    uint16_t r2 = instr & 0x7;
    reg[r0] = reg[r1] & reg[r2];
  }
  update_flags(r0);
}

void op_br(uint16_t instr) {
  /* BR: conditional branch if condition is met
   * BRn = jump if result is negative
   * BRz = jump if result == 0
   * BRp = jump if result is positive
   * BRzp = jump if result is zero OR positive
   * ... and so on with BRnp, BRnz, BRnzp
   */
  uint16_t cond = (instr >> 9) & 0x7;
  if (cond & reg[R_COND]) {  // mask value flags with instruction flags
    reg[R_PC] += sign_extend(instr & 0x1FF, 9);  // bits[8:0]
    update_flags(R_PC);
  }
}

void op_jmp(uint16_t instr) {
  if (r1 == 0x7) {
    // RET
    reg[R_PC] = reg[R_R7];
  } else {
    // JMP: jump to reg [8:6]
    reg[R_PC] = reg[r1];
  }
  update_flags(R_PC);
}

void op_jsr(uint16_t instr) {
  reg[R_R7] = reg[R_PC];  // Stash PC in R7.
  update_flags(R_R7);
  if (((instr >> 11) & 1) == 0) {  // If bit 11 == 0
    // JSR
    reg[R_PC] = reg[r1];
  } else {
    // JSRR
    reg[R_PC] += sign_extend(instr & 0x7FF, 11);  // offset [10:0]
    update_flags(R_PC);
  }
}

void op_ld(uint16_t instr) {
  /* LD: Load contents at location (PC + offset) */
  // destination register (DR): bits [11:9]
  uint16_t r0 = (instr >> 9) & 0x7;
  // Offset value: bits [8:0]
  uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
  // Load
  reg[r0] = mem_read(reg[R_PC] + pc_offset);
  update_flags(r0);
}

void op_ldi(uint16_t instr) {
  /* LDI (load indirect): Load a value from a mem location into a register.
   * Offset value limited to 9 bits.
   */
  // destination register (DR): bits 9-11
  uint16_t r0 = (instr >> 9) & 0x7;
  // Offset value: bits 0-8
  uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
  // Add pc_offset + current PC, then look at that location for final addr.
  reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
  update_flags(r0);
}

void op_ldr(uint16_t instr) {
  /* LDR (Load base+offset)
   * Load contents of location SEXT([5:0]) + reg[8:6] into reg[11:9]
   */
  uint16_t r0 = (instr >> 9) & 0x7;
  uint16_t offset6 = sign_extend(instr & 0x3F, 6);
  reg[r0] = mem_read(reg[r1] + offset6);
  update_flags(r0);
}

void op_lea(uint16_t instr) {
  /* LEA: load effective address */
  // destination register (DR): bits 9-11
  uint16_t r0 = (instr >> 9) & 0x7;
  // Offset value: bits 0-8
  uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
  // Add offset to PC, load address into DR
  reg[r0] = pc_offset + reg[R_PC];
  update_flags(r0);
}

void op_not(uint16_t instr) {
  /* NOT: bitwise-not a value. */
  // destination register (DR): bits 9-11
  uint16_t r0 = (instr >> 9) & 0x7;
  // Invert value at SR1 and store
  reg[r0] = ~reg[r1];
  update_flags(r0);
}

void op_rti(uint16_t instr) {
  printf("op_rti is not implemented yet\n");
  exit(1);
}

void op_st(uint16_t instr) {
  /* ST (Store)
   * Store contents of reg[11:9] in location PC + SEXT([8:0])
   */
  uint16_t r0 = (instr >> 9) & 0x7;
  uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
  mem_write(reg[R_PC] + pc_offset, reg[r0]);
}

void op_sti(uint16_t instr) {
  /* STI (Store Indirect)
   * Store contents of register [11:9] in a memory location at [8:0]
   */
  uint16_t r0 = (instr >> 9) & 0x7;
  uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
  mem_write(mem_read(reg[R_PC] + pc_offset), reg[r0]);
}

void op_str(uint16_t instr) {
  /* STR (Store base+offset)
   * Store contents of register [11:9] in a location SEXT([5:0]) + reg[8:6]
   */
  uint16_t r0 = (instr >> 9) & 0x7;
  uint16_t offset6 = sign_extend(instr & 0x3F, 6);
  mem_write(reg[r1] + offset6, reg[r0]);
}

/* TRAP routines*/

void op_trap_puts() {
  /* TRAP PUTS writes a null-terminated string starting at R0 to console.
   * Before beginning the trap, you must store the address of the first char
   * to display in register 0. Writing terminates on occurrence of x0000.
   */
  /* one 16b char per word */
  uint16_t* c = memory + reg[R_R0];
  while (*c) {
    putc((char)*c, stdout);
    ++c;
  }
  fflush(stdout);
}

void op_trap_out() {
  /* TRAP OUT writes a character in R0[7:0] to console. */
  putc((char)reg[R_R0], stdout);
  fflush(stdout);
}

void op_trap_getc() {
  /* TRAP GETC reads a single ASCII char from keyboard input into R0. */
  reg[R_R0] = (uint16_t)getchar();
  update_flags(R_R0);
}

void op_trap_in() {
  /* TRAP IN prints a prompt and reads a single ASCII char from input.
   * The ASCII char is echoed to console and copied into R0.
   * The high 8 bits of R0 are cleared.
   */
  op_trap_puts(); // Print string at R0.
  op_trap_getc(); // Read char input.
  op_trap_out();  // Print char.
}

void op_trap_putsp() {
  /* TRAP PUTSP writes an ASCII string to console, where two chars are stored
   * per memory location, starting at R0. First the ASCII char at bits [7:0] is 
   * written, then the ASCII char at bits [15:8].
   * If an odd number of chars is to be written, the final location will have 
   * x00 in bits [15:8]. Otherwise terminates on x0000.
   */
  uint16_t* loc = memory + reg[R_R0];
  while (*loc) {
    putc((char)((*loc) & 0xFF), stdout);  // Write bits [7:0]
    char char2 = (*loc) >> 8;
    if (char2) putc(char2, stdout);       // Write bits [15:8]
    ++loc;
  }
  fflush(stdout);
}

/* Main loop (read and execute instructions) */
int main (int argc, const char* argv[]) {
  // Load arguments
  if (argc < 2) {
    // Show usage string
    printf("lc3 [image-file1] ...\n");
    exit(2);
  }
  for (int j=1; j<argc; ++j) {
    if (!read_image(argv[j])) {
      printf("failed to load image: %s\n", argv[j]);
      exit(1);
    }
  }

  // Setup
  signal(SIGINT, handle_interrupt);
  disable_input_buffering();

  // Exactly one condition flag should be set at any given time, so set ZRO.
  reg[R_COND] = FL_ZRO;

  // Set the PC to the starting position, default 0x3000.
  // This leaves space for trap routine code.
  enum { PC_START = 0x3000 };
  reg[R_PC] = PC_START;

  int running = 1;
  while (running) {
    /* Fetch/parse */
    uint16_t instr = mem_read(reg[R_PC]++);
    uint16_t op = instr >> 12;  /* read opcode from the leftmost 4 bits */
    if (check_mode(op)) imm_flag = (instr >> 5) & 0x1;
    if (check_r1(op))   r1 = (instr >> 6) & 0x7;

    switch (op) {
      case OP_ADD:
        op_add(instr);
        break;
      case OP_AND:
        op_and(instr);
        break;
      case OP_NOT:
        op_not(instr);
        break;
      case OP_BR:
        op_br(instr);
        break;
      case OP_JMP:
        op_jmp(instr);
        break;
      case OP_JSR:
        op_jsr(instr);
        break;
      case OP_LD:
        op_ld(instr);
        break;
      case OP_LDI:
        op_ldi(instr);
        break;
      case OP_LDR:
        op_ldr(instr);
        break;
      case OP_LEA:
        op_lea(instr);
        break;
      case OP_ST:
        op_st(instr);
        break;
      case OP_STI:
        op_sti(instr);
        break;
      case OP_STR:
        op_str(instr);
        break;
      case OP_TRAP:
        reg[R_R7] = reg[R_PC];  // Stash PC in R7.
        switch (instr & 0xFF) {  // trap code is in bits [7:0]
          case TRAP_HALT:
            puts("HALT");
            fflush(stdout);
            running = 0;  // halt execution
            break;
          case TRAP_GETC:
            op_trap_getc();
            break;
          case TRAP_OUT:
            op_trap_out();
            break;
          case TRAP_PUTS:
            op_trap_puts();
            break;
          case TRAP_IN:
            op_trap_in();
            break;
          case TRAP_PUTSP:
            op_trap_putsp();
            break;
        }
        break;
      case OP_RES:
      case OP_RTI:
        abort();
      default:
        printf("BAD OPCODE\n");
        exit(1);
    }
  }
  /* Shutdown (restore terminal settings) */
  restore_input_buffering();
}
