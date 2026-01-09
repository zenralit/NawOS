#include "compiler.h"
#include "vm.h"

#include "drivers/keyboard/keyboard.h"
#include "lib/nawstring.h"
#include "lib/math.h"

static const char* p;

// Объявления функций
static int emit(Instruction* c, int* ip, OpCode op, int arg);
static int var_index(const char* name);
static void parse_compare(Instruction* code, int* ip);
static void parse_expr(Instruction* code, int* ip);
static const char* compile_if(const char* src, Instruction* code, int* ip);
static const char* compile_statement(const char* src, Instruction* code, int* ip);
static const char* compile_block(const char* src, Instruction* code, int* ip);

// Определение compile_block
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

// Определение compile_statement
static const char* compile_statement(const char* src, Instruction* code, int* ip) {
    while (*src == ' ' || *src == '\n') src++;

    // if
    if (src[0] == 'i' && src[1] == 'f')
        return compile_if(src, code, ip);

    // print
    if (src[0] == 'p' && src[1] == 'r') {
        src += 5; // print
        parse_expr(code, ip);
        emit(code, ip, OP_PRINT, 0);
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

        parse_expr(code, ip);
        emit(code, ip, OP_STORE_VAR, var_index(name));

        while (*src && *src != '\n') src++;
        return src;
    }

    return src + 1;
}

// Определение parse_factor
static void parse_factor(Instruction* code, int* ip) {
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

    // variable
    char name[16];
    int k = 0;
    while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')) {
        name[k++] = *p++;
    }
    name[k] = 0;

    emit(code, ip, OP_LOAD_VAR, var_index(name));
}

// Определение parse_term
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

// Определение parse_expr
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

// Определение compile_expr
void compile_expr(const char* expr, Instruction* code, int* ip) {
    p = expr;
    parse_compare(code, ip);
}

// Определение emit
static int emit(Instruction* c, int* ip, OpCode op, int arg) {
    c[*ip].op = op;
    c[*ip].arg = arg;
    return (*ip)++;
}

// Определение var_index
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

// Определение naw_compile
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
                    compile_expr(line, out, &ip);
                    emit(out, &ip, OP_PRINT, 0);
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

            // закрывающая '}'
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
                        compile_expr(line, out, &ip);
                        emit(out, &ip, OP_PRINT, 0);
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

        //PRINT 
        if (strncmp(line, "print", 5) == 0) {
            line += 5;
            compile_expr(line, out, &ip);
            emit(out, &ip, OP_PRINT, 0);
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

        // следующая строка
        while (*line && *line != '\n') line++;
    }

    emit(out, &ip, OP_HALT, 0);
    return ip;
}

// Определение parse_compare
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

// Определение compile_if
static const char* compile_if(const char* src, Instruction* code, int* ip) {
    src += 2; // skip "if"

    while (*src == ' ') src++;
    if (*src != '(') return src;
    src++;

    // условие
    compile_expr(src, code, ip);

    while (*src && *src != ')') src++;
    if (*src == ')') src++;

    int jmp_false = *ip;
    emit(code, ip, OP_JMP_IF_FALSE, 0);

    // then block
    src = compile_block(src, code, ip);

    // else?
    while (*src == ' ') src++;

    if (src[0] == 'e' && src[1] == 'l' &&
        src[2] == 's' && src[3] == 'e') {

        int jmp_end = *ip;
        emit(code, ip, OP_JMP, 0);

        // патчим JMP_IF_FALSE → else
        code[jmp_false].arg = *ip;

        src += 4;
        src = compile_block(src, code, ip);

        // патчим JMP → конец
        code[jmp_end].arg = *ip;
    } else {
        // без else
        code[jmp_false].arg = *ip;
    }

    return src;
}