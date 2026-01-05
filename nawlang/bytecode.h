#ifndef NAW_BYTECODE_H
#define NAW_BYTECODE_H

typedef enum {
    OP_HALT = 0,

    OP_PUSH_INT,
    OP_LOAD_VAR,
    OP_STORE_VAR,

    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,

    OP_PRINT,

    OP_JMP,
    OP_JMP_IF_FALSE
} OpCode;

typedef struct {
    OpCode op;
    int arg;
} Instruction;

#define MAX_CODE 1024
#define MAX_VARS 64

#endif
