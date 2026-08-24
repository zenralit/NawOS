#ifndef NAW_BYTECODE_H
#define NAW_BYTECODE_H

#define MAX_CODE 1024
#define MAX_VARS 64

typedef enum {
    OP_PUSH_INT,
    OP_PUSH_STR,
    OP_PRINT_INT,
    OP_PRINT_STR,
    OP_READ_INT,
    OP_LOAD_VAR,
    OP_STORE_VAR,

    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,

    OP_EQ,
    OP_NE,
    OP_LT,
    OP_GT,
    OP_LE,
    OP_GE,

    OP_JMP,
    OP_JMP_IF_FALSE,

    /* arg = naw_math_native_id_t; аргументы на стеке — строки-выражения. */
    OP_NATIVE_CALL,

    // OP_CALL,
    // OP_RET,      

    OP_PRINT,
    OP_HALT
} OpCode;

typedef struct {
    OpCode op;
    int arg; /* для OP_PUSH_STR — указатель на строку, приведённый к int */
} Instruction;


#endif
