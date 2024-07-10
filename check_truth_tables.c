#include <stdio.h>
#include <stdlib.h>

#include "truth_tables.c"

int main() {
  printf("op\t11:9\t8:6\t5\n");
  printf("--\t--\t--\t--\n");
  for (uint16_t i=0; i<=0xF; i++) {
    printf("%d\t%d\t%d\t%d\n", i, check_r0(i), check_r1(i), check_mode(i));
  };
}
