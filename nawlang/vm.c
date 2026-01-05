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

            case OP_STORE_VAR:
                vars[in.arg] = pop();
                break;

            case OP_ADD: push(pop() + pop()); break;
            case OP_SUB: { int b=pop(), a=pop(); push(a-b); } break;
            case OP_MUL: push(pop() * pop()); break;
            case OP_DIV: { int b=pop(), a=pop(); push(a/b); } break;

            case OP_PRINT:
                print_dec(pop());
                print("\n");
                break;

            case OP_JMP:
                ip = in.arg;
                break;

            case OP_JMP_IF_FALSE:
                if (!pop()) ip = in.arg;
                break;

            default:
                print("VM: bad opcode\n");
                return;
        }
    }
}
