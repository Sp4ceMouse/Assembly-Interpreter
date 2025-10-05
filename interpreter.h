#ifndef __INTERPRETER_H
#define __INTERPRETER_H

#define MEMORY_SIZE 1024

// Declaration of Memory type:
typedef struct Memory {
  int num_instructions;
  char *instruction[MEMORY_SIZE];  // array of instructions
  int data[MEMORY_SIZE];           // array of data
} Memory;

/*** General Register Structures ***/
typedef int Registers;

// This represents all the registers used in the project.
enum RegisterName { EAX, EDX, ECX, ESP, EBP, EIP, NOT_REG };
typedef enum RegisterName RegisterName;

typedef struct System {
  Registers registers[6];  // 0: EAX, 1: EDX, 2: ECX, 3: ESP, 4: EBP, 5: EIP
  Memory memory;
  int comparison_flag;  // comparison flag to hold the result of comparisons
} System;

typedef enum DataType { REG, MEM, CONST, UNKNOWN } DataType;

/*
Memory Type could be register, memory, or constant

For Data in Register: reg will be one of the register and value will be -1

For Data in Memory: reg will be one of the register that
stores the memory address, and value will be offset of
that memory address

For constant value: reg will be NOT_REG and value will be
the constant value
*/
typedef struct MemoryType {
  DataType type;
  RegisterName reg;
  int value;
} MemoryType;

typedef enum ExecResult {
  SUCCESS,
  INSTRUCTION_ERROR,
  MEMORY_ERROR,
  PC_ERROR
} ExecResult;

void initialize_system(System *sys);
RegisterName get_register_by_name(const char *name);
MemoryType get_memory_type(const char *name);

void load_instructions_from_file(System *sys, const char *filename);
ExecResult execute_movl(System *sys, char *src, char *dst);
ExecResult execute_addl(System *sys, char *src, char *dst);
ExecResult execute_push(System *sys, char *src);
ExecResult execute_pop(System *sys, char *dst);
ExecResult execute_cmpl(System *sys, char *src, char *dst);
ExecResult execute_jmp(System *sys, char *condition, char *dst);
ExecResult execute_call(System *sys, char *dst);
ExecResult execute_ret(System *sys);
void execute_instructions(System *sys);

#endif
