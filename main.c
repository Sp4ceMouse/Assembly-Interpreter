#include <stdio.h>
#include <stdlib.h>
#include "interpreter.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <instruction_file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  System sys;
  initialize_system(&sys);

  // Load instructions from the file specified in the program argument
  load_instructions_from_file(&sys, argv[1]);

  // Initialize some registers for testing
  sys.registers[EAX] = 5;
  sys.registers[EDX] = 3;
  sys.registers[ECX] = 2;

  // Execute instructions
  execute_instructions(&sys);

  // Print the result
  printf("Register EAX: %d\n", sys.registers[EAX]);
  printf("Register EDX: %d\n", sys.registers[EDX]);
  printf("Register ECX: %d\n", sys.registers[ECX]);

  

  return 0;
}
