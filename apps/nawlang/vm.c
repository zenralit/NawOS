#include "vm.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/screen/screen.h"
#include "lib/math.h"

typedef enum {
    VALUE_INT = 0,
    VALUE_STR = 1
} ValueType;

/* Стек VM хранит либо int, либо указатель на строку (для print и math native). */
typedef struct {
    ValueType type;
    int int_value;
    const char* str_value;
} Value;

static Value stack[256];
static int sp = 0;
static int vars[MAX_VARS];

static void vm_error(const char* message) {
    print(message);
    print("\n");
}

static void vm_push_int(int value) {
    if (sp >= (int)(sizeof(stack) / sizeof(stack[0]))) {
        vm_error("VM: stack overflow");
        return;
    }

    stack[sp].type = VALUE_INT;
    stack[sp].int_value = value;
    stack[sp].str_value = 0;
    sp++;
}

static void vm_push_str(const char* value) {
    if (sp >= (int)(sizeof(stack) / sizeof(stack[0]))) {
        vm_error("VM: stack overflow");
        return;
    }

    stack[sp].type = VALUE_STR;
    stack[sp].int_value = 0;
    stack[sp].str_value = value ? value : "";
    sp++;
}

static int vm_pop(Value* out) {
    if (sp <= 0) {
        return 0;
    }

    *out = stack[--sp];
    return 1;
}

static int vm_pop_int(int* out) {
    Value value;

    if (!vm_pop(&value) || value.type != VALUE_INT) {
        return 0;
    }

    *out = value.int_value;
    return 1;
}

static void vm_print_value(Value value) {
    if (value.type == VALUE_STR) {
        print(value.str_value ? value.str_value : "");
    } else {
        print_dec((uint32_t)value.int_value);
    }

    print("\n");
}

static int vm_native_arg_count(int native_id) {
    /* Арность math_* native; -1 — неизвестный id. */
    switch (native_id) {
        case NAW_MATH_NATIVE_EVAL:
        case NAW_MATH_NATIVE_SQRT:
        case NAW_MATH_NATIVE_SIN:
        case NAW_MATH_NATIVE_COS:
        case NAW_MATH_NATIVE_TAN:
        case NAW_MATH_NATIVE_ABS:
        case NAW_MATH_NATIVE_ARG:
        case NAW_MATH_NATIVE_CONJ:
        case NAW_MATH_NATIVE_NORM:
            return 1;
        case NAW_MATH_NATIVE_ADD:
        case NAW_MATH_NATIVE_SUB:
        case NAW_MATH_NATIVE_MUL:
        case NAW_MATH_NATIVE_DIV:
        case NAW_MATH_NATIVE_POW:
            return 2;
        case NAW_MATH_NATIVE_QUAD:
            return 3;
        default:
            return -1;
    }
}

void vm_run(Instruction* code) {
    int ip = 0;

    sp = 0;
    for (int i = 0; i < MAX_VARS; i++) {
        vars[i] = 0;
    }

    for (;;) {
        Instruction in = code[ip++];

        switch (in.op) {
            case OP_HALT:
                return;

            case OP_PUSH_INT:
                vm_push_int(in.arg);
                break;

            case OP_PUSH_STR:
                vm_push_str((const char*)in.arg);
                break;

            case OP_LOAD_VAR:
                vm_push_int(vars[in.arg]);
                break;

            case OP_STORE_VAR: {
                int value;

                if (!vm_pop_int(&value)) {
                    vm_error("VM: type mismatch");
                    return;
                }

                vars[in.arg] = value;
                break;
            }

            case OP_ADD: {
                int b;
                int a;

                if (!vm_pop_int(&b) || !vm_pop_int(&a)) {
                    vm_error("VM: type mismatch");
                    return;
                }

                vm_push_int(a + b);
                break;
            }

            case OP_SUB: {
                int b;
                int a;

                if (!vm_pop_int(&b) || !vm_pop_int(&a)) {
                    vm_error("VM: type mismatch");
                    return;
                }

                vm_push_int(a - b);
                break;
            }

            case OP_MUL: {
                int b;
                int a;

                if (!vm_pop_int(&b) || !vm_pop_int(&a)) {
                    vm_error("VM: type mismatch");
                    return;
                }

                vm_push_int(a * b);
                break;
            }

            case OP_DIV: {
                int b;
                int a;

                if (!vm_pop_int(&b) || !vm_pop_int(&a) || b == 0) {
                    vm_error("VM: type mismatch");
                    return;
                }

                vm_push_int(a / b);
                break;
            }

            case OP_EQ:
            case OP_NE:
            case OP_LT:
            case OP_GT:
            case OP_LE:
            case OP_GE: {
                int b;
                int a;
                int result;

                if (!vm_pop_int(&b) || !vm_pop_int(&a)) {
                    vm_error("VM: type mismatch");
                    return;
                }

                if (in.op == OP_EQ) {
                    result = (a == b);
                } else if (in.op == OP_NE) {
                    result = (a != b);
                } else if (in.op == OP_LT) {
                    result = (a < b);
                } else if (in.op == OP_GT) {
                    result = (a > b);
                } else if (in.op == OP_LE) {
                    result = (a <= b);
                } else {
                    result = (a >= b);
                }

                vm_push_int(result);
                break;
            }

            case OP_JMP:
                ip = in.arg;
                continue;

            case OP_JMP_IF_FALSE: {
                int condition;

                if (!vm_pop_int(&condition)) {
                    vm_error("VM: type mismatch");
                    return;
                }

                if (condition == 0) {
                    ip = in.arg;
                    continue;
                }
                break;
            }

            case OP_NATIVE_CALL: {
                const char* args[3];
                int argc = vm_native_arg_count(in.arg);

                if (argc < 0 || sp < argc) {
                    vm_error("VM: bad native call");
                    return;
                }

                /* Математический API принимает текстовые выражения, не int-значения Lelya. */
                for (int i = argc - 1; i >= 0; i--) {
                    Value arg;

                    if (!vm_pop(&arg) || arg.type != VALUE_STR) {
                        vm_error("VM: native math expects string arguments");
                        return;
                    }

                    args[i] = arg.str_value ? arg.str_value : "";
                }

                vm_push_str(naw_math_native_call((naw_math_native_id_t)in.arg, args, argc));
                break;
            }

            case OP_PRINT:
            case OP_PRINT_INT:
            case OP_PRINT_STR: {
                Value value;

                if (!vm_pop(&value)) {
                    vm_error("VM: stack underflow");
                    return;
                }

                vm_print_value(value);
                break;
            }

            case OP_READ_INT:
                vars[in.arg] = keyboard_read_int();
                break;

            default:
                vm_error("VM: bad opcode");
                return;
        }
    }
}
