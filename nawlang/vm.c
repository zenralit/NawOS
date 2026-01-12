#include "vm.h"
#include "drivers/screen/screen.h"


static int stack[256];
static int sp = 0;

static int vars[MAX_VARS];

static void push(int v) { stack[sp++] = v; }
static int pop() { return stack[--sp]; }

void vm_run(Instruction* code) {
    int ip = 0;

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
            default:
                print("VM: bad opcode\n");
                return;
        }
    }ip++;
}
