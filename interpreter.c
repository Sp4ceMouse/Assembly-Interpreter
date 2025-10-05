#include "interpreter.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void splitString(const char *input, char *str1, char *str2, char *str3) {
  char temp[100]; 
  strncpy(temp, input, sizeof(temp) - 1);
  temp[sizeof(temp) - 1] = '\0'; 

  str1[0] = str2[0] = str3[0] = '\0';

  char *token = strtok(temp, " ");
  if (token) strcpy(str1, token);

  token = strtok(NULL, " ");
  if (token) strcpy(str2, token);

  token = strtok(NULL, " ");
  if (token) strcpy(str3, token);
}

/* reset the system to a defulat status */
void initialize_system(System *sys) {
  sys->registers[EAX] = 0;
  sys->registers[EDX] = 0;
  sys->registers[ECX] = 0;
  sys->registers[ESP] = MEMORY_SIZE - 256;
  sys->registers[EBP] = MEMORY_SIZE - 256;
  sys->registers[EIP] = 0;  // Program counter

  sys->memory.num_instructions = 0;
  for (int i = 0; i < MEMORY_SIZE; i++) {
    sys->memory.instruction[i] = NULL;
    sys->memory.data[i] = 0;
  }
  sys->comparison_flag = 0;
}

/* Remove leading and extra space, and \n from the input string and return the
 * length of updated string */
int reformat(char *line) {
  int idx, size = 0, flag = 0;
  int line_size = strlen(line);
  for (idx = 0; idx < line_size && line[idx] == ' '; idx++)
    ;
  for (; idx < line_size; idx++) {
    if (line[idx] == '\n') break;
    if (line[idx] == ' ') {
      if (!flag) {
        line[size++] = line[idx];
        flag = 1;
      }
    } else {
      line[size++] = line[idx];
      flag = 0;
    }
  }

  line[size] = '\0';
  return size;
}

/* Load all the instruction from the file into the instruction segment in the
 * system */
void load_instructions_from_file(System *sys, const char *filename) {
  FILE *file = fopen(filename, "r");
  if (!file) {
    perror("Error opening file");
    exit(EXIT_FAILURE);
  }

  char line[256];
  int address = 0;

  while (fgets(line, sizeof(line), file) != NULL && address < MEMORY_SIZE) {
    // Remove newline character
    line[strlen(line) - 1] = '\0';
    // Save instruction to the memory
    int size = reformat(line);
    if (size == 0) continue;
    sys->memory.instruction[address] = strdup(line);
    address++;
    // Reach out the end of the instruction
    if (strcmp(line, "END") == 0) break;
  }
  sys->memory.num_instructions = address;

  fclose(file);
}

/* Return value could be the name of one of the valid registers, or NOT_REG for
 * any other input */
RegisterName get_register_by_name(const char *name) {
  if (strcmp(name, "%EAX") == 0) return EAX;
  if (strcmp(name, "%EDX") == 0) return EDX;
  if (strcmp(name, "%ECX") == 0) return ECX;
  if (strcmp(name, "%ESP") == 0) return ESP;
  if (strcmp(name, "%EBP") == 0) return EBP;
  if (strcmp(name, "%EIP") == 0) return EIP;
  return NOT_REG;  // indicate this is not a register
}

/*
This function accepts an operand that can be represented in different formats:
registers, memory, or constant values. 

For example, a valid operand could be %EAX, (%EAX), -20(%EAX), or $10.

The function returns one of the following: 
  REG for a register operand, 
  MEM for a memory operand, or 
  CONST for a constant operand. 

If the parsing succeeds, the function will return the appropriate type; otherwise, it will return UNKNOWN in the event of a parsing error.

For a register operand: 
  reg will be one of the registers, and value will be -1.

For a memory operand: 
  reg will be the register storing the memory address, and value will be the offset of that memory address. 

For a constant operand: 
  reg will be NOT_REG, and value will be the constant value.
*/
MemoryType get_memory_type(const char *operand) {
  MemoryType result = {UNKNOWN, NOT_REG, -1};
  result.reg = get_register_by_name(operand);
  if (result.reg != NOT_REG) {
    result.type = REG;
  } else {
    if (operand[0] == '$') {
      result.type = CONST;
      result.value = atoi(&operand[1]);
    } else if (strstr(operand, "(") && strstr(operand, ")")) {
      char str[5];
      if (operand[0] == '(') {
        sscanf(operand, "(%s)", str);
        result.value = 0;
      } else {
        sscanf(operand, "%d(%s", &result.value, str);
      }
      str[strlen(str) - 1] = '\0';
      result.reg = get_register_by_name(str);
      if (result.reg != NOT_REG) {
        result.type = MEM;
      }
    }
  }
  return result;
}

/*
This function takes a string that represnts a label in the instruction.
It returns the memory address of the next instruction
corresponding to the label in the system. It returns -1 if the label is not
found or the label does not come with . as the first character
*/
int get_addr_from_label(System *sys, const char *label) {
  if (label[0] != '.') {
    return -1;
  }
  for (int i = 0; i < sys->memory.num_instructions; i++) {
    if (strcmp(sys->memory.instruction[i], label) == 0) {
      return (i + 1) * 4;
    }
  }
  return -1;
}


/*
The execute_movl function validates and executes a movl instruction, ensuring source and destination operands are of known and appropriate types, and then performs the move operation if valid.

It will return Success if there is no error.

++It will return INSTRUCTION_ERROR 
    if src or dst is a undifined memory space. In
    this case, the type of a MemoryType data will be UNKNOWN. 
    
++It will return INSTRUCTION_ERROR 
    if dst is a constant value instead of a register or memory address. 
    
++It will return INSTRUCTION_ERROR 
    if both src and dst are memory addresses. 
    
It will return MEMORY_ERROR 
    if there is a memory address from src or dst that is an invalid memory address (less than 0, or greater than (MEMORY_SIZE - 1) * 4).

If there is any error, all the system registers, memory, and system status should remain unchanged.

Do not change EIP in this function.

HINT: you may use get_memory_type in this function.
*/
ExecResult execute_movl(System *sys, char *src, char *dst) {
  MemoryType source = get_memory_type(src);
  MemoryType destination = get_memory_type(dst);

  if(source.type == REG){
    if(destination.type == CONST){
      return INSTRUCTION_ERROR;
    }
    else if(destination.type == REG){
      sys->registers[destination.reg] = sys->registers[source.reg];
      return SUCCESS;
    }
    else if(destination.type == MEM){
      int totalVal = sys->registers[destination.reg] + destination.value;
      if((totalVal < 0 || totalVal > ((MEMORY_SIZE - 1) * 4))){
        return MEMORY_ERROR;
      }
      sys->memory.data[totalVal / 4] = sys->registers[source.reg];
      return SUCCESS;
    }
    else{
      return INSTRUCTION_ERROR;
    }
  }
  else if(source.type == CONST){
    if(destination.type == CONST){
      return INSTRUCTION_ERROR;
    }
    else if(destination.type == REG){
      sys->registers[destination.reg] = source.value;
      return SUCCESS;
    }
    else if(destination.type == MEM){
      int totalVal = sys->registers[destination.reg] + destination.value;
      if((totalVal < 0 || totalVal > ((MEMORY_SIZE - 1) * 4))){
        return MEMORY_ERROR;
      }
      else{
        sys->memory.data[totalVal / 4] = source.value;
        return SUCCESS;
      }
    }
    else{
      return INSTRUCTION_ERROR;
    }
  }
  else if(source.type == MEM){
    if(destination.type == CONST){
      return INSTRUCTION_ERROR;
    }
    else if(destination.type == REG){
      int totalVal = sys->registers[source.reg] + source.value;

      if((totalVal < 0 || totalVal > ((MEMORY_SIZE - 1) * 4))){
        return MEMORY_ERROR;
      }
      else{
        sys->registers[destination.reg] = sys->memory.data[totalVal / 4];
        return SUCCESS;
      }
    }
    else{
      return INSTRUCTION_ERROR;
    }
  }
  else{
    return INSTRUCTION_ERROR;
  }
  
  return INSTRUCTION_ERROR;
}

/*
The execute_addl function validates and executes a addl instruction
  ensuring source and destination operands are of known and appropriate types

,and then performs the add operation if valid.

It will return SUCCESS if there is no error.

It will return INSTRUCTION_ERROR 
  if src or dst is a undifined memory space.
  In this case, the type of a MemoryType data will be UNKNOWN.

It will return INSTRUCTION_ERROR 
  if dst is a constant value instead of a register or memory address.

It will return INSTRUCTION_ERROR 
  if both src and dst are memory addresses.

It will return MEMORY_ERROR 
  if there is a memory address from src or dst that is an invalid memory address (less than 0, or greater than (MEMORY_SIZE - 1) * 4).

If there is any error, all the system registers, memory, and system status should remain unchanged.
Do not change EIP in this function.

HINT: you may use get_memory_type in this function.
*/
ExecResult execute_addl(System *sys, char *src, char *dst) {
  MemoryType source = get_memory_type(src);
  MemoryType destination = get_memory_type(dst);
  int totalValDest = sys->registers[destination.reg] + destination.value;
  int totalValSrc = sys->registers[source.reg] + source.value;

  switch (destination.type) {
    case REG:
      if(source.type == CONST){
        sys->registers[destination.reg] += source.value;
        return SUCCESS;
      }
      else if(source.type == MEM){
        //int totalValSrc = sys->registers[source.reg] + source.value;
        if((totalValSrc < 0 || totalValSrc > ((MEMORY_SIZE - 1) * 4))){
          return MEMORY_ERROR;
        }
        sys->registers[destination.reg] += sys->memory.data[totalValSrc / 4];
        return SUCCESS;
      }
      else if(source.type == REG){
        sys->registers[destination.reg] += sys->registers[source.reg];
        return SUCCESS;
      }
      else{
        return INSTRUCTION_ERROR;
      }
      break; 

    case MEM:
      //int totalValDest = sys->registers[destination.reg] + destination.value;
      if((totalValDest < 0 || totalValDest > ((MEMORY_SIZE - 1) * 4))){
        return MEMORY_ERROR;
      }
      else if(source.type == CONST){
        sys->memory.data[totalValDest / 4] += source.value;
        return SUCCESS;
      }
      else if(source.type == MEM){
        return INSTRUCTION_ERROR;
      }
      else if(source.type == REG){
        sys->memory.data[totalValDest / 4] += sys->registers[source.reg];
        return SUCCESS;
      }
      else{
        return INSTRUCTION_ERROR;
      }
      break;

    case CONST:
      return INSTRUCTION_ERROR;
      break;

    default:
      return INSTRUCTION_ERROR;
      break;
    }

  return INSTRUCTION_ERROR;
}

/*
The execute_push function validates and executes a pushl instruction, ensuring source operands is of known and appropriate type, and then performs the push operation if valid.

It will return SUCCESS if there is no error.
It will return INSTRUCTION_ERROR 
  if src is a undifined memory space. In this case, the type of a MemoryType data will be UNKNOWN. 
It will return MEMORY_ERROR
  if the address stored in src is an invalid memory address (less than 0, or greater than (MEMORY_SIZE - 1) * 4).
It will return MEMORY_ERROR 
  if esp is an invalid memory address: less than 4, greater than or equal to MEMORY_SIZE * 4).

If there is any error, all the system registers, memory, and system 
  status should remain unchanged.

Do not change EIP in this function.
HINT: you may use get_memory_type in this function.
*/
ExecResult execute_push(System *sys, char *src) {
  MemoryType source = get_memory_type(src);

  int totalValSrc = sys->registers[source.reg] + source.value;
  int valToCopy;

  if(sys->registers[ESP] - 4 < 0 || sys->registers[ESP] - 4 > ((MEMORY_SIZE - 1) * 4)){
    return MEMORY_ERROR;
  }

  switch (source.type) {
    case UNKNOWN:
      return INSTRUCTION_ERROR;
      break;

    case MEM:
      if((totalValSrc < 0 || totalValSrc > ((MEMORY_SIZE - 1) * 4))){
        return MEMORY_ERROR;
      }
      sys->registers[ESP] -= 4;
      sys->memory.data[sys->registers[ESP] / 4] = sys->memory.data[totalValSrc / 4];
      return SUCCESS;
      break;

    case REG:
      valToCopy = sys->registers[source.reg];
      sys->registers[ESP] -= 4;
      sys->memory.data[sys->registers[ESP] / 4] = valToCopy;
      return SUCCESS;
      break;

    case CONST:
      sys->registers[ESP] -= 4;
      sys->memory.data[sys->registers[ESP] / 4] = source.value;
      return SUCCESS;
      break;

    default:
      return INSTRUCTION_ERROR;
      break;
    }

  return INSTRUCTION_ERROR;
}

/*
The execute_pop function validates and executes a popl instruction, ensuring the destination operand is of known and appropriate type, and then performs the pop
operation if valid.

It will return SUCCESS 
  if there is no error.
It will return INSTRUCTION_ERROR 
  if dst is not a register or memory address.
It will return MEMORY_ERROR 
  if dst is an invalid memory address: less than 0, or greater than (MEMORY_SIZE - 1) * 4).
It will return MEMORY_ERROR 
  if the address stored in esp is an invalid memory address: less than 0, or greater than (MEMORY_SIZE - 1) * 4).

If there is any error, all the system registers, memory, and system status should remain unchanged.

Do not change EIP in this function.
HINT: you may use get_memory_type in this function.
*/
ExecResult execute_pop(System *sys, char *dst) {
  MemoryType destination = get_memory_type(dst);

  int totalValDest = sys->registers[destination.reg] + destination.value;
  int valToCopy;

  if(sys->registers[ESP] + 4 < 0 || sys->registers[ESP] + 4 > ((MEMORY_SIZE - 1) * 4)){
    return MEMORY_ERROR;
  }

  switch (destination.type) {
    case UNKNOWN:
      return INSTRUCTION_ERROR;
      break;

    case MEM:
      if((totalValDest < 0 || totalValDest > ((MEMORY_SIZE - 1) * 4))){
        return MEMORY_ERROR;
      }
      valToCopy = sys->memory.data[sys->registers[ESP] / 4];
      sys->memory.data[totalValDest / 4] = valToCopy;
      //sys->memory.data[totalValDest / 4];
      sys->registers[ESP] += 4;
      return SUCCESS;
      break;

    case REG:
      valToCopy = sys->memory.data[sys->registers[ESP] / 4];
      sys->registers[destination.reg] = valToCopy;
      sys->registers[ESP] += 4;
      return SUCCESS;
      break;

    case CONST:
      return INSTRUCTION_ERROR;
      break;

    default:
      return INSTRUCTION_ERROR;
      break;
    }

  return INSTRUCTION_ERROR;
}

/*
The execute_cmpl function validates and executes a cmpl instruction, ensuring
the source and destination operands are of known and appropriate types, and then
performs the compare operation and update comparison_flag in the system if
valid.

It will return SUCCESS 
  if there is no error.
It will return INSTRUCTION_ERROR 
  if src or dst is an undefined memory space. (unkown type?)
It will return INSTRUCTION_ERROR 
  if both src and dst are memory addresses.
It will return MEMORY_ERROR 
  if there is a memory address from src or dst that is an invalid memory address (less than 0, or greater than (MEMORY_SIZE - 1) * 4).

comparison
cmpl src2, src1
  like computing src1 - src2
cf=1 if carry out from msb
zf=1 if (src1==src2)
sf=1 if (src1-src2 < 0)
of=1 if two's complement

If there is any error, all the system registers, memory, and
system status should remain unchanged. You can decide the value of
comparison_flag for each comparison result.
Do not change EIP in this function.
HINT: you may use get_memory_type in this function.
*/
ExecResult execute_cmpl(System *sys, char *src, char *dst) {
  MemoryType source2 = get_memory_type(dst);
  MemoryType source1 = get_memory_type(src);

  int totalValSrc2 = sys->registers[source2.reg] + source2.value;
  int totalValSrc1 = sys->registers[source1.reg] + source1.value;

  int val1, val2;

  switch (source1.type) { 
    case UNKNOWN:
      return INSTRUCTION_ERROR;
    break;
    case MEM:
      if(source2.type == MEM){
        return INSTRUCTION_ERROR;
      }
      if((totalValSrc1 < 0 || totalValSrc1 > ((MEMORY_SIZE - 1) * 4))){
        return MEMORY_ERROR;
      }
      val1 = sys->memory.data[totalValSrc1 / 4];
      break;
    case REG:
      val1 = sys->registers[source1.reg];
      break;
    case CONST:
      val1 = source1.value;
      break;
    default:
      return INSTRUCTION_ERROR;
      break;
  }

  switch (source2.type) {
    case UNKNOWN:
      return INSTRUCTION_ERROR;
    break;
    case MEM:
      if(source1.type == MEM){
        return INSTRUCTION_ERROR;
      }
      if((totalValSrc2 < 0 || totalValSrc2 > ((MEMORY_SIZE - 1) * 4))){
        return MEMORY_ERROR;
      }
      val2 = sys->memory.data[totalValSrc2 / 4];
      break;
    case REG:
      val2 = sys->registers[source2.reg];
      break;
    case CONST:
      val2 = source2.value;
      break;
    default:
      return INSTRUCTION_ERROR;
      break;
  }

  sys->comparison_flag = val2 - val1;

  return SUCCESS;
}

/*
The execute_jmp function validates and executes a condition or direct jump instruction, ensuring the destination operands is of known label,
and then performs the direct jump operation, or condition jump if condition is met.

A valid condition argument should be one of the following strings: "JE", "JNE", "JL", "JG", or "JMP"

It will return SUCCESS 
  if the jump is executed successfully no matter whether condition is met. 
It will return PC_ERROR 
  if the destination label cannot be found in the instruction segment in the system.

If there is any error, all the system registers (except for EIP), memory, and system status should remain unchanged.

Please update program counter (EIP) in this function.

HINT: you may use get_addr_from_label in this function.
*/
ExecResult execute_jmp(System *sys, char *condition, char *dst) {

  int memAdd = get_addr_from_label(sys, dst);

  if((memAdd < 0 || memAdd > ((MEMORY_SIZE - 1) * 4))){
    return PC_ERROR;
  }

  if(strcmp(condition, "JE") == 0){
    if(sys->comparison_flag == 0){
      sys->registers[EIP] += 4;
      sys->registers[EIP] = memAdd;
    }
    else{
      sys->registers[EIP] += 4;
    }
    return SUCCESS;
  }
  else if(strcmp(condition, "JNE") == 0){
    if(sys->comparison_flag > 0 || sys->comparison_flag < 0){
      sys->registers[EIP] += 4;
      sys->registers[EIP] = memAdd;
    }
    else{
      sys->registers[EIP] += 4;
    }
    return SUCCESS;
  }
  else if(strcmp(condition, "JL") == 0){
    if(sys->comparison_flag < 0){
      sys->registers[EIP] += 4;
      sys->registers[EIP] = memAdd;
    }
    else{
      sys->registers[EIP] += 4;
    }
    return SUCCESS;
  }
  else if(strcmp(condition, "JG") == 0){
    if(sys->comparison_flag > 0){
      sys->registers[EIP] += 4;
      sys->registers[EIP] = memAdd;
    }
    else{
      sys->registers[EIP] += 4;
    }
    return SUCCESS;
  }
  else if(strcmp(condition, "JMP") == 0){
    sys->registers[EIP] += 4;
    sys->registers[EIP] = memAdd;
    return SUCCESS;
  }
  else{
    sys->registers[EIP] += 4;
    return PC_ERROR;
  }
  return PC_ERROR;
}

/*
The execute_call function validates and executes a call instruction, ensuring
the destination operand is a known label, and then performs the call operation.

It will return SUCCESS 
  if the call is executed successfully.
It will return PC_ERROR 
  if the destination label cannot be found in the instruction segment in the system.

If there is any error, all the system registers (except for EIP), memory, and
system status should remain unchanged.

Please update program counter (EIP) in this function.

HINT: you may use get_addr_from_label in this function.
*/
ExecResult execute_call(System *sys, char *dst) {

  int memAdd = get_addr_from_label(sys, dst);

  if((memAdd < 0 || memAdd > ((MEMORY_SIZE - 1) * 4))){
    return PC_ERROR;
  }

  sys->registers[EIP] += 4;

  execute_push(sys, "%EIP");

  sys->registers[EIP] = memAdd;
  
  return SUCCESS;
}

/*
The execute_ret function validates and executes a return instruction, which pops
the return address from the stack and update EIP (program counter).

It will return SUCCESS if the return is executed successfully.
It will return PC_ERROR if the return address is invalid (less than 0 or greater
than or equal to the number of instructions).

If there is any error, all the system registers (except for EIP and ESP),
memory, and system status should remain unchanged.

Please update program counter (EIP) in this function.
*/
ExecResult execute_ret(System *sys) {
  execute_pop(sys, "%EIP");

  if(sys->registers[EIP] < 0 || sys->registers[EIP] > (MEMORY_SIZE - 1) || sys->registers[EIP] > sys->memory.num_instructions){
    return PC_ERROR;
  }

  return SUCCESS;
}

/*
Utilizing the EIP register's value (also known as the program counter), the
function fetches instructions from the instruction segment in system memory. It
then executes each instruction, which can be one of MOVL, ADDL PUSHL, POPL,
CMPL, CALL, RET, JMP, JNE, JE, JL, or JG, by employing the corresponding execute
functions. This process continues until the program encounters any Error status
or the END instruction. 

During the execution, it will ignore all the
instructions that are not listed above and continue to the next one.
Please update program counter (EIP) for MOVL, ADDL, PUSHL, POPL, and CMPL in
this function.
*/
void execute_instructions(System *sys) {
  char inst[256];  // you can use strcpy to copy instruction from memory to this
                   // variable
  // TODO
  // for(int i = 0; i < MEMORY_SIZE - 1; ++i){
  //   printf("this is an instruction: %s\n", sys->memory.instruction[i]);
  // }

  // this is an instruction: MOVL %EDX %EAX 
  // this is an instruction: MOVL %ECX %EDX 
  // this is an instruction: MOVL %EAX %ECX 
  // this is an instruction: END
  //[M,O,V,L, ,%,E,D,X, ,%,E,A,X]
  //[0,1,2,3,4,5,6,7,8,9,0,1,2,3]

  //[M,O,V,L, ,%,E,D,X, ,%,E,A,X]
  //[0,1,2,3,4,5,6,7,8,9,0,1,2,3]

  // MOVL
  // ADDL 
  // PUSHL
  // POPL
  // CMPL

  // CALL 
  // RET

  // JMP 
  // JNE 
  // JE 
  // JL  
  // JG 

  //load instruction from

  int num_instructions = sizeof(sys->memory.instruction) / sizeof(sys->memory.instruction[0]);

  for(;;){

    //printf("this is an instruction: %s\n", sys->memory.instruction[sys->registers[EIP] / 4]);

    char part1[20], part2[20], part3[20];

    splitString(sys->memory.instruction[sys->registers[EIP] / 4], part1, part2, part3);

    if(strcmp(part1, "MOVL") == 0){
      execute_movl(sys, part2, part3);
      sys->registers[EIP] += 4;
    }
    else if(strcmp(part1, "ADDL") == 0){
      execute_addl(sys, part2, part3);
      sys->registers[EIP] += 4;
    }
    else if(strcmp(part1, "PUSHL") == 0){
      execute_push(sys, part2);
      sys->registers[EIP] += 4;
    }
    else if(strcmp(part1, "POPL") == 0){
      execute_pop(sys, part2);
      sys->registers[EIP] += 4;
    }
    else if(strcmp(part1, "CMPL") == 0){
      execute_cmpl(sys, part2, part3);
      sys->registers[EIP] += 4;
    }
    else if(strcmp(part1, "CALL") == 0){
      execute_call(sys, part2);
    }
    else if(strcmp(part1, "RET") == 0){
      execute_ret(sys);
    }
    else if(strcmp(part1, "JMP") == 0 || strcmp(part1, "JNE") == 0 || strcmp(part1, "JE") == 0 || strcmp(part1, "JL") == 0 || strcmp(part1, "JG") == 0){
      execute_jmp(sys, part1, part2);
    }
    else if(strcmp(part1, "END") == 0){
      break;
    }
    else{
      sys->registers[EIP] += 4;
    }
  }
}