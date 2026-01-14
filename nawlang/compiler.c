#include "compiler.h"
#include "vm.h"

#include "drivers/keyboard/keyboard.h"
#include "lib/nawstring.h"
#include "lib/math.h"
#define MAX_STRINGS 64
#define MAX_STRING_LEN 64

typedef struct {
    int loop_start;
    int break_jumps[32];
    int break_count;
    int continue_jumps[32];
    int continue_count;
} LoopContext;


static char string_pool[MAX_STRINGS][MAX_STRING_LEN];
static int string_count = 0;
static const char* p;
static LoopContext loop_stack[8];
static int loop_sp = 0;

static int emit(Instruction* c, int* ip, OpCode op, int arg);
static int var_index(const char* name);
static void parse_factor(Instruction* code, int* ip);
static void parse_compare(Instruction* code, int* ip);
static void parse_expr(Instruction* code, int* ip);
void compile_expr(const char* expr, Instruction* code, int* ip);
static const char* compile_if(const char* src, Instruction* code, int* ip);
static const char* compile_statement(const char* src, Instruction* code, int* ip);
static const char* compile_block(const char* src, Instruction* code, int* ip);
static const char* compile_while(const char* src, Instruction* code, int* ip);

// static int is_alpha(char c) {
//     return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
// }

// static int func_index(const char* name) {
//     for (int i = 0; i < func_count; i++) {
//         if (strcmp(functions[i].name, name) == 0)
//             return i;
//     }
//     return -1;
// }

static char* add_string(const char* s) {
    int i = 0;

    for (i = 0; i < string_count; i++) {
        int j = 0;
        while (string_pool[i][j] == s[j] && s[j]) j++;
        if (string_pool[i][j] == 0 && s[j] == 0)
            return string_pool[i];
    }

    i = 0;
    while (s[i] && i < MAX_STRING_LEN - 1) {
        string_pool[string_count][i] = s[i];
        i++;
    }
    string_pool[string_count][i] = 0;

    return string_pool[string_count++];
}


static const char* compile_block(const char* src, Instruction* code, int* ip) {
    while (*src == ' ') src++;

    if (*src != '{') return src;
    src++;

    while (*src && *src != '}') {
        src = compile_statement(src, code, ip);
        while (*src == '\n' || *src == ' ') src++;
    }

    if (*src == '}') src++;
    return src;
}


static const char* compile_statement(const char* src, Instruction* code, int* ip) {
    while (*src == ' ' || *src == '\n') src++;

    // if
    if (src[0] == 'i' && src[1] == 'f')
        return compile_if(src, code, ip);

    if (src[0]=='w' && src[1]=='h') {
        return compile_while(src, code, ip);
    }

    if (src[0] == 'p' && src[1] == 'r') {
        src += 5; // print
        while (*src == ' ') src++;


        if (*src == '"') {
            p = src;
            parse_factor(code, ip); 
            emit(code, ip, OP_PRINT_STR, 0);
            src = p;
        }

        else {
            compile_expr(src, code, ip);
            emit(code, ip, OP_PRINT_INT, 0);
        }

        while (*src && *src != '\n') src++;
        return src;
    }
   
    if (src[0]=='b' && src[1]=='r') { 
        LoopContext* ctx = &loop_stack[loop_sp-1];
        ctx->break_jumps[ctx->break_count++] = *ip;
        emit(code, ip, OP_JMP, 0);
        while (*src && *src != '\n') src++;
        return src;
    }

    if (src[0]=='c' && src[1]=='o') {
        LoopContext* ctx = &loop_stack[loop_sp-1];
        ctx->continue_jumps[ctx->continue_count++] = *ip;
        emit(code, ip, OP_JMP, ctx->loop_start);
        while (*src && *src != '\n') src++;
        return src;
    }

    // assignment
    if ((*src >= 'a' && *src <= 'z') ||
        (*src >= 'A' && *src <= 'Z')) {

        char name[16];
        int k = 0;

        while ((*src >= 'a' && *src <= 'z') ||
               (*src >= 'A' && *src <= 'Z'))
            name[k++] = *src++;

        name[k] = 0;

        while (*src == ' ') src++;
        if (*src == '=') src++;

        compile_expr(src, code, ip);
        emit(code, ip, OP_STORE_VAR, var_index(name));

        while (*src && *src != '\n') src++;
        return src;
    }
    return src;

}


static void parse_factor(Instruction* code, int* ip) {
        if (*p == '"') {
        p++;

        char buf[64];
        int k = 0;

        while (*p && *p != '"' && k < 63)
            buf[k++] = *p++;

        buf[k] = 0;
        if (*p == '"') p++;

        char* s = add_string(buf);
        emit(code, ip, OP_PUSH_STR, (int)s);
        return;
    }

    while (*p == ' ') p++;

    if (*p == '(') {
        p++;
        parse_expr(code, ip);
        if (*p == ')') p++;
        return;
    }

    // number
    if (*p >= '0' && *p <= '9') {
        int v = 0;
        while (*p >= '0' && *p <= '9')
            v = v * 10 + (*p++ - '0');
        emit(code, ip, OP_PUSH_INT, v);
        return;
    }

//     if (is_alpha(*p)) {
//     char name[16];
//     int k = 0;
//     while (is_alpha(*p)) name[k++] = *p++;
//     name[k] = 0;

//     if (*p == '(') {
//         p++;
//         int argc = 0;
//         while (*p && *p != ')') {
//             parse_expr(code, ip);
//             argc++;
//             if (*p == ',') p++;
//         }
//         p++;

//         int fidx = func_index(name);
//         emit(code, ip, OP_CALL, fidx);
//         return;
//     }
// }

    // variable
    char name[16];
    int k = 0;
    while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')) {
        name[k++] = *p++;
    }
    name[k] = 0;

    emit(code, ip, OP_LOAD_VAR, var_index(name));
}

static void parse_term(Instruction* code, int* ip) {
    parse_factor(code, ip);

    for (;;) {
        while (*p == ' ') p++;
        if (*p == '*' || *p == '/') {
            char op = *p++;
            parse_factor(code, ip);
            emit(code, ip, op == '*' ? OP_MUL : OP_DIV, 0);
        } else break;
    }
}

static void parse_expr(Instruction* code, int* ip) {
    parse_term(code, ip);

    for (;;) {
        while (*p == ' ') p++;
        if (*p == '+' || *p == '-') {
            char op = *p++;
            parse_term(code, ip);
            emit(code, ip, op == '+' ? OP_ADD : OP_SUB, 0);
        } else break;
    }
}

void compile_expr(const char* expr, Instruction* code, int* ip) {
    p = expr;
    parse_compare(code, ip);
}

static int emit(Instruction* c, int* ip, OpCode op, int arg) {
    c[*ip].op = op;
    c[*ip].arg = arg;
    return (*ip)++;
}

static int var_index(const char* name) {
    static char vars[64][16];
    static int count = 0;

    for (int i = 0; i < count; i++) {
        int j = 0;
        while (vars[i][j] && name[j] && vars[i][j] == name[j])
            j++;

        if (vars[i][j] == 0 && name[j] == 0)
            return i;
    }

    int j = 0;
    while (name[j] && j < 15) {
        vars[count][j] = name[j];
        j++;
    }
    vars[count][j] = 0;

    return count++;
}

int naw_compile(const char* src, Instruction* out) {
    int ip = 0;
    const char* line = src;

    while (*line) {
        while (*line == ' ' || *line == '\n') line++;

        //IF / ELSE 
        if (line[0] == 'i' && line[1] == 'f') {
            line += 2;

            while (*line == ' ') line++;

            // '('
            if (*line != '(') {
                return ip;
            }
            line++;

            // compile condition
            compile_expr(line, out, &ip);

            while (*line && *line != ')') line++;
            if (*line == ')') line++;

            // JMP_IF_FALSE (
            int jmp_false = ip;
            emit(out, &ip, OP_JMP_IF_FALSE, 0);

            // skip spaces
            while (*line == ' ') line++;

            // '{'
            if (*line != '{') return ip;
            line++;

            // THEN block
            while (*line && *line != '}') {
                const char* inner = line;
                line = inner;
                
                while (*line == ' ' || *line == '\n') line++;

                if (strncmp(line, "print", 5) == 0) {
                    line += 5;
                    while (*line == ' ') line++;

                    if (*line == '"') {
                        p = line;
                        parse_factor(out, &ip);
                        emit(out, &ip, OP_PRINT_STR, 0);
                        line = p;
                    } else {
                        compile_expr(line, out, &ip);
                        emit(out, &ip, OP_PRINT_INT, 0);
                    }
                }
                else if ((*line >= 'a' && *line <= 'z') ||
                         (*line >= 'A' && *line <= 'Z')) {

                    char name[16];
                    int k = 0;

                    while ((*line >= 'a' && *line <= 'z') ||
                           (*line >= 'A' && *line <= 'Z')) {
                        name[k++] = *line++;
                    }
                    name[k] = 0;

                    while (*line == ' ') line++;
                    if (*line == '=') line++;

                    compile_expr(line, out, &ip);
                    emit(out, &ip, OP_STORE_VAR, var_index(name));
                }

                while (*line && *line != '\n') line++;
                if (*line == '\n') line++;
            }

            //  }
            if (*line == '}') line++;

            // skip spaces
            while (*line == ' ') line++;

            // ELSE?
            if (line[0] == 'e' && line[1] == 'l' &&
                line[2] == 's' && line[3] == 'e') {

                // JMP через else
                int jmp_end = ip;
                emit(out, &ip, OP_JMP, 0);

                // JMP_IF_FALSE начало else
                out[jmp_false].arg = ip;

                line += 4;
                while (*line == ' ') line++;

                if (*line != '{') return ip;
                line++;

                // ELSE bock
                while (*line && *line != '}') {
                    while (*line == ' ' || *line == '\n') line++;

                    if (strncmp(line, "print", 5) == 0) {
                        line += 5;
                        while (*line == ' ') line++;

                        if (*line == '"') {
                            p = line;
                            parse_factor(out, &ip);      
                            emit(out, &ip, OP_PRINT_STR, 0);
                            line = p;
                        } else {
                            compile_expr(line, out, &ip);
                            emit(out, &ip, OP_PRINT_INT, 0);
                        }
                    }
                    else if ((*line >= 'a' && *line <= 'z') ||
                             (*line >= 'A' && *line <= 'Z')) {

                        char name[16];
                        int k = 0;

                        while ((*line >= 'a' && *line <= 'z') ||
                               (*line >= 'A' && *line <= 'Z')) {
                            name[k++] = *line++;
                        }
                        name[k] = 0;

                        while (*line == ' ') line++;
                        if (*line == '=') line++;

                        compile_expr(line, out, &ip);
                        emit(out, &ip, OP_STORE_VAR, var_index(name));
                    }

                    while (*line && *line != '\n') line++;
                    if (*line == '\n') line++;
                }

                if (*line == '}') line++;

                //JMP  конец
                out[jmp_end].arg = ip;
            } else {
                // без else
                out[jmp_false].arg = ip;
            }

            continue;
        }

            if (line[0]=='w' && line[1]=='h') {
                line = compile_while(line, out, &ip);
                continue;
        }
        if (strncmp(line, "print", 5) == 0) {
            line += 5;
            while (*line == ' ') line++;

            if (*line == '"') {
                p = line;
                parse_factor(out, &ip);
                emit(out, &ip, OP_PRINT_STR, 0);
                line = p;
            } else {
                compile_expr(line, out, &ip);
                emit(out, &ip, OP_PRINT_INT, 0);
            }
        }

        // ASSIGN 
        else if ((*line >= 'a' && *line <= 'z') ||
                 (*line >= 'A' && *line <= 'Z')) {

            char name[16];
            int k = 0;

            while ((*line >= 'a' && *line <= 'z') ||
                   (*line >= 'A' && *line <= 'Z')) {
                name[k++] = *line++;
            }
            name[k] = 0;

            while (*line == ' ') line++;
            if (*line == '=') line++;

            compile_expr(line, out, &ip);
            emit(out, &ip, OP_STORE_VAR, var_index(name));
        }

        // след строка
        while (*line && *line != '\n') line++;
    }

    emit(out, &ip, OP_HALT, 0);
    return ip;
}


static void parse_compare(Instruction* code, int* ip) {
    parse_expr(code, ip);

    for (;;) {
        while (*p == ' ') p++;

        if (*p == '=' && *(p+1) == '=') {
            p += 2;
            parse_expr(code, ip);
            emit(code, ip, OP_EQ, 0);
        }
        else if (*p == '!' && *(p+1) == '=') {
            p += 2;
            parse_expr(code, ip);
            emit(code, ip, OP_NE, 0);
        }
        else if (*p == '<' && *(p+1) == '=') {
            p += 2;
            parse_expr(code, ip);
            emit(code, ip, OP_LE, 0);
        }
        else if (*p == '>' && *(p+1) == '=') {
            p += 2;
            parse_expr(code, ip);
            emit(code, ip, OP_GE, 0);
        }
        else if (*p == '<') {
            p++;
            parse_expr(code, ip);
            emit(code, ip, OP_LT, 0);
        }
        else if (*p == '>') {
            p++;
            parse_expr(code, ip);
            emit(code, ip, OP_GT, 0);
        }
        else break;
    }
}

static const char* compile_if(const char* src, Instruction* code, int* ip) {
    src += 2; 

    while (*src == ' ') src++;
    if (*src != '(') return src;
    src++;

    compile_expr(src, code, ip);

    while (*src && *src != ')') src++;
    if (*src == ')') src++;

    int jmp_false = *ip;
    emit(code, ip, OP_JMP_IF_FALSE, 0);

    src = compile_block(src, code, ip);

    while (*src == ' ') src++;

    if (src[0] == 'e' && src[1] == 'l' &&
        src[2] == 's' && src[3] == 'e') {

        int jmp_end = *ip;
        emit(code, ip, OP_JMP, 0);

        code[jmp_false].arg = *ip;

        src += 4;
        src = compile_block(src, code, ip);

        code[jmp_end].arg = *ip;
    } else {
        code[jmp_false].arg = *ip;
    }

    return src;
}
static const char* compile_while(const char* src, Instruction* code, int* ip) {
    src += 5; 
    while (*src == ' ') src++;
    if (*src != '(') return src;
    src++;

    LoopContext* ctx = &loop_stack[loop_sp++];
    ctx->loop_start = *ip;
    ctx->break_count = 0;
    ctx->continue_count = 0;

    p = src;
    parse_compare(code, ip);
    src = p;

    while (*src && *src != ')') src++;
    if (*src == ')') src++;

    int jmp_exit = *ip;
    emit(code, ip, OP_JMP_IF_FALSE, 0);

    src = compile_block(src, code, ip);

    emit(code, ip, OP_JMP, ctx->loop_start);

    loop_sp--;

    for (int i = 0; i < ctx->break_count; i++) {
        code[ctx->break_jumps[i]].arg = *ip;
    }
    
    for (int i = 0; i < ctx->continue_count; i++) {
        code[ctx->continue_jumps[i]].arg = ctx->loop_start;
    }

    code[jmp_exit].arg = *ip;

    return src;
}

// static const char* compile_function(const char* src, Instruction* code, int* ip) {
//     src += 3;
//     while (*src == ' ') src++;

//     char name[16];
//     int k = 0;
//     while ((*src >= 'a' && *src <= 'z') ||
//            (*src >= 'A' && *src <= 'Z')) {
//         name[k++] = *src++;
//     }
//     name[k] = 0;

//     int fidx = func_count++;
//     strncpy(functions[fidx].name, name, 16);
//     functions[fidx].entry_ip = *ip;

//     while (*src && *src != '(') src++;
//     src++;

//     int param_count = 0;
//     while (*src && *src != ')') {
//         if ((*src >= 'a' && *src <= 'z') ||
//             (*src >= 'A' && *src <= 'Z')) {
//             param_count++;
//             while ((*src >= 'a' && *src <= 'z') ||
//                    (*src >= 'A' && *src <= 'Z')) src++;
//         }
//         if (*src == ',') src++;
//     }
//     src++;

//     functions[fidx].param_count = param_count;

//     src = compile_block(src, code, ip);

//     emit(code, ip, OP_PUSH_INT, 0);
//     emit(code, ip, OP_RET, 0);

//     return src;
// }