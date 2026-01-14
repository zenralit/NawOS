#ifndef NAW_VM_H
#define NAW_VM_H
#include "bytecode.h"

#define MAX_FUNCS 64
#define MAX_PARAMS 8
#define MAX_CALL_STACK 64

// typedef struct {
//     char name[16];
//     int entry_ip;
//     int param_count;
// } Function;

// extern Function functions[MAX_FUNCS];
// extern int func_count;
void vm_run(Instruction* code);
#endif