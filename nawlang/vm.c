#include "vm.h"
#include "drivers/screen/screen.h"
#include "drivers/keyboard/keyboard.h"

typedef struct {
    int return_ip;
    int base_sp;
} CallFrame;


// Function functions[MAX_FUNCS];
// int func_count = 0;
static int stack[256];
static int sp = 0;
// static CallFrame call_stack[MAX_CALL_STACK];
// static int call_sp = 0;
static int vars[MAX_VARS];

static void push(int v) { stack[sp++] = v; }
static int pop() { return stack[--sp]; }

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
                push(in.arg);
                break;

            case OP_LOAD_VAR:
                push(vars[in.arg]);
                break;

            case OP_STORE_VAR: {
                int v = pop();
                vars[in.arg] = v;
                break;
            }

            case OP_ADD: push(pop() + pop()); break;
            case OP_SUB: { int b=pop(), a=pop(); push(a-b); } break;
            case OP_MUL: push(pop() * pop()); break;
            case OP_DIV: { int b=pop(), a=pop(); push(a/b); } break;

            case OP_PRINT_INT:
                print_dec(pop());
                print("\n");
                break;

            case OP_PRINT_STR:
                print((char*)pop());
                print("\n");
                break;

            case OP_READ_INT:
                vars[in.arg] = keyboard_read_int();
                break;

            case OP_PUSH_STR:
                push(in.arg);
                break;

            case OP_JMP:
                ip = in.arg;
                continue; 

            case OP_JMP_IF_FALSE: {
                int cond = pop();
                if (cond == 0) {
                    ip = in.arg;
                    continue;
                }
                break;
            }
            case OP_EQ: {
                int b = pop();
                int a = pop();
                push(a == b);
                break;
            }
            case OP_NE: {
                int b = pop();
                int a = pop();
                push(a != b);
                break;
            }
            case OP_LT: {
                int b = pop();
                int a = pop();
                push(a < b);
                break;
            }
            case OP_GT: {
                int b = pop();
                int a = pop();
                push(a > b);
                break;
            }
            case OP_LE: {
                int b = pop();
                int a = pop();
                push(a <= b);
                break;
            }
            case OP_GE: {
                int b = pop();
                int a = pop();
                push(a >= b);
                break;
            }
            // case OP_CALL: {
            //     int fidx = in.arg;

            //     call_stack[call_sp++] = (CallFrame){
            //         .return_ip = ip,
            //         .base_sp = sp - functions[fidx].param_count
            //     };

            //     ip = functions[fidx].entry_ip;
            //     break;
            // }
            // case OP_RET: {
            //     int ret = pop();
            //     CallFrame frame = call_stack[--call_sp];

            //     sp = frame.base_sp;
            //     push(ret);
            //     ip = frame.return_ip;
            //     break;
            // }

            default:
                print("VM: bad opcode\n");
                return;
        }
    }
}
