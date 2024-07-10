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

/* Sign extend a `bit_count`-length signed num `x` to a 16-bit signed num.
 * https://en.wikipedia.org/wiki/Two%27s_complement
 */
uint16_t sign_extend(uint16_t x, int bit_count) {
  if ((x >> (bit_count - 1)) & 1) {  // If x<0 (sign bit is 1)
    x |= (0xFFFF << bit_count);  // Fill in the left bits with 1s
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
uint16_t r0; // bits [11:9]
uint16_t r1; // bits [8:6]
uint16_t imm_flag; // bit [5]

void set_reg(uint16_t reg_id, uint16_t val) {
  reg[reg_id] = val;
  update_flags(reg_id);
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
  set_reg(R_R0, (uint16_t)getchar());
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
    // TODO: profile if this is even faster than just assigning w/out checking
    if (check_mode(op)) imm_flag = (instr >> 5) & 0x1; // imm mode = bit[5]
    if (check_r1(op))   r1 = (instr >> 6) & 0x7;       // bits[8:6]
    if (check_r0(op))   r0 = (instr >> 9) & 0x7;       // bits[11:9]

    switch (op) {

      case OP_ADD:
        set_reg(r0, imm_flag ?
          // imm mode: R0 = R1 + offset5
          reg[r1] + sign_extend(instr & 0x1F, 5) :
          // normal mode: R0 = R1 + reg[2:0]
          reg[r1] + reg[instr & 0x7]
        );
        break;

      case OP_AND:
        set_reg(r0, imm_flag ?
          // imm mode: R0 = R1 & offset5
          reg[r1] & sign_extend(instr & 0x1F, 5) :
          // normal mode: R0 = R1 & reg[2:0]
          reg[r1] & reg[instr & 0x7]
        );
        break;

      case OP_NOT:
        set_reg(r0, ~reg[r1]);
        break;

      case OP_BR:
        if (r0 & reg[R_COND]) // instr flags ([11:9]) & most recent cond flags
          set_reg(R_PC,
            reg[R_PC] + sign_extend(instr & 0x1FF, 9));  // PC += offset9
        break;

      case OP_JMP:
        set_reg(R_PC,
          (r1 == 0x7) ?
          reg[R_R7] : // RET
          reg[r1]     // JMP to reg[8:6]
        );
        break;

      case OP_JSR:
        set_reg(R_R7, reg[R_PC]);  // Stash PC in R7.
        set_reg(R_PC,
          (((instr >> 11) & 1) == 0) ? // bit[11] == 0
          reg[r1] : // JSRR
          reg[R_PC] + sign_extend(instr & 0x7FF, 11) // JSR: PC += offset11
        );
        break;

      case OP_LD:
        // LD: load reg with value at location (PC + offset9)
        set_reg(r0, mem_read(reg[R_PC] + sign_extend(instr & 0x1FF, 9)));
        break;

      case OP_LDI:
        // LDI: load reg with value via pointer in location (PC + offset9)
        set_reg(r0, mem_read(mem_read(reg[R_PC] + sign_extend(instr & 0x1FF, 9))));
        break;

      case OP_LDR:
        // LDR: load reg with value at location (baseR + offset6)
        set_reg(r0, mem_read(reg[r1] + sign_extend(instr & 0x3F, 6)));
        break;

      case OP_LEA:
        // LEA: load reg with address (PC + offset9)
        set_reg(r0, reg[R_PC] + sign_extend(instr & 0x1FF, 9));
        break;

      case OP_ST:
        // ST: Store value in reg at location (PC + offset9)
        mem_write(
          reg[R_PC] + sign_extend(instr & 0x1FF, 9),
          reg[r0]
        );
        break;

      case OP_STI:
        // STI: Store value in reg at pointer in location (PC + offset9)
        mem_write(
          mem_read(reg[R_PC] + sign_extend(instr & 0x1FF, 9)),
          reg[r0]
        );
        break;

      case OP_STR:
        // STR: Store value in reg at location (reg[8:6] + offset6)
        mem_write(
          reg[r1] + sign_extend(instr & 0x3F, 6),
          reg[r0]
        );
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
