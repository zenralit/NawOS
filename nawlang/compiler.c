#include "compiler.h"
#include "vm.h"

#include "drivers/keyboard/keyboard.h"
#include "lib/nawstring.h"
#include "lib/math.h"

#define MAX_IF_STACK 16

//utils

static int emit(Instruction* code, int* ip, OpCode op, int arg) {
    code[*ip].op  = op;
    code[*ip].arg = arg;
    return (*ip)++;
}

int naw_strncmp(const char* a, const char* b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i] || !a[i] || !b[i])
            return a[i] - b[i];
    }
    return 0;
}


// variables

static int var_index(const char* name) {
    static char vars[MAX_VARS][16];
    static int count = 0;

    for (int i = 0; i < count; i++) {
        if (!strcmp(vars[i], name))
            return i;
    }

    strncpy(vars[count], name, 15);
    vars[count][15] = 0;
    return count++;
}

//compiler

int naw_compile(const char* src, Instruction* code) {
    int ip = 0;
    char line[128];
    int pos = 0;

    int if_stack[MAX_IF_STACK];
    int if_sp = 0;

    for (int i = 0;; i++) {
        char c = src[i];

        if (c == '\n' || c == 0) {
            line[pos] = 0;
            pos = 0;

            //int x = expr 
            if (!naw_strncmp(line, "int ", 4)) {
                char name[16] = {0};
                char* p = line + 4;
                int k = 0;

                while (*p && *p != '=' && *p != ' ')
                    name[k++] = *p++;

                while (*p && *p != '=') p++;
                if (*p == '=') p++;

                int v = eval_expr(p);
                emit(code, &ip, OP_PUSH_INT, v);
                emit(code, &ip, OP_STORE_VAR, var_index(name));
            }

            //print expr 
            else if (!naw_strncmp(line, "print ", 6)) {
                int v = eval_expr(line + 6);
                emit(code, &ip, OP_PUSH_INT, v);
                emit(code, &ip, OP_PRINT, 0);
            }

            //if (cond)
            else if (!naw_strncmp(line, "if", 2)) {
                char* p = find_char(line, '(');
                char* q = find_char(line, ')');

                if (p && q && q > p) {
                    char cond[64];
                    int len = q - p - 1;
                    if (len > 63) len = 63;

                    strncpy(cond, p + 1, len);
                    cond[len] = 0;

                    int v = eval_expr(cond);
                    emit(code, &ip, OP_PUSH_INT, v);

                    if_stack[if_sp++] =
                        emit(code, &ip, OP_JMP_IF_FALSE, 0);
                }
            }

            //else
            else if (!naw_strncmp(line, "else", 4)) {
                int j = emit(code, &ip, OP_JMP, 0);
                code[if_stack[if_sp - 1]].arg = ip;
                if_stack[if_sp - 1] = j;
            }

            //}
            else if (!naw_strncmp(line, "}", 1)) {
                code[if_stack[--if_sp]].arg = ip;
            }

            if (c == 0)
                break;
        } else {
            if (pos < 127)
                line[pos++] = c;
        }
    }

    emit(code, &ip, OP_HALT, 0);
    return ip;
}
