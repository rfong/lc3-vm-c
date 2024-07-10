/* Check outputs for some boolean functions */

int check_r0(uint16_t op) {
  // Given an opcode, return true if we need to parse bits [11:9]
  return (
    !((op >> 2) == 0x3 || (op & 0x3) == 0x0) || // ^(11.. or ..00)
    op == 0xE ||                                // 1110 = 14
    op == 0x0
  );
}

int check_r1(uint16_t op) {
  // Given an opcode, return true if we need to parse bits [8:6]
  return (
    (op & 0xC) == 0x4 || // 01__
    (op & 0x7) == 0x1 || // _001
    op == 0xC            // 1100
  );
}

int check_mode(uint16_t op) {
  // Given an opcode, return true if we need to parse bit [5]
  return (
    (op >> 3) == 0 &&
    (op & 0x3) == 0x1
  );
}
