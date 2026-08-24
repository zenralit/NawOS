#include "compiler.h"
#include "vm.h"
#include "lib/math.h"

#include "lib/nawstring.h"

#define MAX_STRINGS 64
#define MAX_STRING_LEN 128
#define MAX_VAR_NAME_LEN 16
#define MAX_LOOP_DEPTH 8
#define MAX_NATIVE_ARGS 3

typedef struct {
    int loop_start;
    int break_jumps[32];
    int break_count;
    int continue_jumps[32];
    int continue_count;
} LoopContext;

static char string_pool[MAX_STRINGS][MAX_STRING_LEN];
static int string_count = 0;
static char variable_names[MAX_VARS][MAX_VAR_NAME_LEN];
static int variable_count = 0;
static const char* p;
static LoopContext loop_stack[MAX_LOOP_DEPTH];
static int loop_sp = 0;

static int emit(Instruction* c, int* ip, OpCode op, int arg);
static int var_index(const char* name);
static int is_ident_start(char c);
static int is_ident_char(char c);
static int is_inline_space(char c);
static int match_keyword(const char* src, const char* keyword);
static const char* skip_inline_space(const char* src);
static const char* skip_whitespace(const char* src);
static const char* skip_to_line_end(const char* src);
static int read_identifier(const char** src, char* name);
static char* add_string(const char* s);
static int native_math_id_from_name(const char* name);
static int capture_native_math_argument(const char** src, char* out, int out_size);
static int compile_native_math_call(const char* name, const char** src, Instruction* code, int* ip);
static void parse_factor(Instruction* code, int* ip);
static void parse_term(Instruction* code, int* ip);
static void parse_expr(Instruction* code, int* ip);
static void parse_compare(Instruction* code, int* ip);
void compile_expr(const char* expr, Instruction* code, int* ip);
static const char* compile_if(const char* src, Instruction* code, int* ip);
static const char* compile_statement(const char* src, Instruction* code, int* ip);
static const char* compile_block(const char* src, Instruction* code, int* ip);
static const char* compile_while(const char* src, Instruction* code, int* ip);

static int is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

static int is_ident_char(char c) {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

static int is_inline_space(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

static int match_keyword(const char* src, const char* keyword) {
    int i = 0;

    while (keyword[i] && src[i] == keyword[i]) {
        i++;
    }

    return keyword[i] == 0 && !is_ident_char(src[i]);
}

static const char* skip_inline_space(const char* src) {
    while (is_inline_space(*src)) {
        src++;
    }

    return src;
}

static const char* skip_whitespace(const char* src) {
    while (*src == '\n' || is_inline_space(*src)) {
        src++;
    }

    return src;
}

static const char* skip_to_line_end(const char* src) {
    while (*src && *src != '\n') {
        src++;
    }

    return src;
}

static int read_identifier(const char** src, char* name) {
    const char* cur = *src;
    int k = 0;

    if (!is_ident_start(*cur)) {
        name[0] = 0;
        return 0;
    }

    while (is_ident_char(*cur)) {
        if (k < MAX_VAR_NAME_LEN - 1) {
            name[k++] = *cur;
        }
        cur++;
    }

    name[k] = 0;
    *src = cur;
    return 1;
}

static char* add_string(const char* s) {
    int i;

    for (i = 0; i < string_count; i++) {
        int j = 0;

        while (string_pool[i][j] == s[j] && s[j]) {
            j++;
        }

        if (string_pool[i][j] == 0 && s[j] == 0) {
            return string_pool[i];
        }
    }

    if (string_count >= MAX_STRINGS) {
        return string_pool[MAX_STRINGS - 1];
    }

    i = 0;
    while (s[i] && i < MAX_STRING_LEN - 1) {
        string_pool[string_count][i] = s[i];
        i++;
    }
    string_pool[string_count][i] = 0;

    return string_pool[string_count++];
}

static int native_math_id_from_name(const char* name) {
    if (strcmp(name, "math_eval") == 0) return NAW_MATH_NATIVE_EVAL;
    if (strcmp(name, "math_add") == 0) return NAW_MATH_NATIVE_ADD;
    if (strcmp(name, "math_sub") == 0) return NAW_MATH_NATIVE_SUB;
    if (strcmp(name, "math_mul") == 0) return NAW_MATH_NATIVE_MUL;
    if (strcmp(name, "math_div") == 0) return NAW_MATH_NATIVE_DIV;
    if (strcmp(name, "math_pow") == 0) return NAW_MATH_NATIVE_POW;
    if (strcmp(name, "math_sqrt") == 0) return NAW_MATH_NATIVE_SQRT;
    if (strcmp(name, "math_sin") == 0) return NAW_MATH_NATIVE_SIN;
    if (strcmp(name, "math_cos") == 0) return NAW_MATH_NATIVE_COS;
    if (strcmp(name, "math_tan") == 0) return NAW_MATH_NATIVE_TAN;
    if (strcmp(name, "math_abs") == 0) return NAW_MATH_NATIVE_ABS;
    if (strcmp(name, "math_arg") == 0) return NAW_MATH_NATIVE_ARG;
    if (strcmp(name, "math_conj") == 0) return NAW_MATH_NATIVE_CONJ;
    if (strcmp(name, "math_norm") == 0) return NAW_MATH_NATIVE_NORM;
    if (strcmp(name, "math_quad") == 0) return NAW_MATH_NATIVE_QUAD;

    return -1;
}

static int capture_native_math_argument(const char** src, char* out, int out_size) {
    const char* cur = skip_inline_space(*src);
    int depth = 0;
    int in_string = 0;
    int pos = 0;
    int started = 0;

    if (*cur == '\0' || *cur == ')') {
        return 0;
    }

    /*
     * Аргумент math_* не компилируется как expr Lelya, а сохраняется текстом
     * для naw_eval_complex_expr. Запятые внутри скобок/строк не разделяют args.
     */
    while (*cur) {
        char c = *cur;

        if (in_string) {
            if (c == '"') {
                in_string = 0;
            }

            if (pos < out_size - 1) {
                out[pos++] = c;
            }
            cur++;
            started = 1;
            continue;
        }

        if (c == '"') {
            in_string = 1;
            if (pos < out_size - 1) {
                out[pos++] = c;
            }
            cur++;
            started = 1;
            continue;
        }

        if (c == '(') {
            depth++;
            if (pos < out_size - 1) {
                out[pos++] = c;
            }
            cur++;
            started = 1;
            continue;
        }

        if (c == ')') {
            if (depth == 0) {
                break;
            }

            depth--;
            if (pos < out_size - 1) {
                out[pos++] = c;
            }
            cur++;
            started = 1;
            continue;
        }

        if (c == ',' && depth == 0) {
            break;
        }

        if (pos < out_size - 1) {
            out[pos++] = c;
        }
        cur++;
        started = 1;
    }

    while (pos > 0 && (out[pos - 1] == ' ' || out[pos - 1] == '\t')) {
        pos--;
    }

    out[pos] = 0;
    *src = cur;
    return started;
}

/*
 * Компиляция math_*(...); 1 — ок, 0 — это не native, -1 — синтаксическая ошибка вызова.
 * На стек кладутся строки аргументов, затем OP_NATIVE_CALL с id функции.
 */
static int compile_native_math_call(const char* name, const char** src, Instruction* code, int* ip) {
    const char* cur = *src;
    char args[MAX_NATIVE_ARGS][MAX_STRING_LEN];
    int native_id;
    int argc = 0;

    native_id = native_math_id_from_name(name);
    if (native_id < 0) {
        return 0;
    }

    cur = skip_inline_space(cur);
    if (*cur != '(') {
        return 0;
    }
    cur++;

    cur = skip_inline_space(cur);
    if (*cur == ')') {
        cur++;
    } else {
        while (1) {
            if (argc >= MAX_NATIVE_ARGS) {
                *src = cur;
                return -1;
            }

            if (!capture_native_math_argument(&cur, args[argc], sizeof(args[argc]))) {
                *src = cur;
                return -1;
            }

            argc++;
            cur = skip_inline_space(cur);

            if (*cur == ',') {
                cur++;
                cur = skip_inline_space(cur);
                continue;
            }

            if (*cur == ')') {
                cur++;
                break;
            }

            *src = cur;
            return -1;
        }
    }

    for (int i = 0; i < argc; i++) {
        char* stored = add_string(args[i]);
        emit(code, ip, OP_PUSH_STR, (int)stored);
    }

    emit(code, ip, OP_NATIVE_CALL, native_id);
    *src = cur;
    return 1;
}

static const char* compile_block(const char* src, Instruction* code, int* ip) {
    src = skip_whitespace(src);

    if (*src != '{') {
        return src;
    }
    src++;

    while (*src && *src != '}') {
        src = compile_statement(src, code, ip);
        src = skip_whitespace(src);
    }

    if (*src == '}') {
        src++;
    }

    return src;
}

static const char* compile_statement(const char* src, Instruction* code, int* ip) {
    char name[MAX_VAR_NAME_LEN];

    src = skip_whitespace(src);

    if (*src == 0 || *src == '}') {
        return src;
    }

    if (match_keyword(src, "if")) {
        return compile_if(src, code, ip);
    }

    if (match_keyword(src, "while")) {
        return compile_while(src, code, ip);
    }

    if (match_keyword(src, "print")) {
        src += 5;
        src = skip_inline_space(src);

        compile_expr(src, code, ip);
        emit(code, ip, OP_PRINT, 0);

        return skip_to_line_end(src);
    }

    if (match_keyword(src, "break")) {
        if (loop_sp > 0) {
            LoopContext* ctx = &loop_stack[loop_sp - 1];
            ctx->break_jumps[ctx->break_count++] = *ip;
            emit(code, ip, OP_JMP, 0);
        }

        return skip_to_line_end(src);
    }

    if (match_keyword(src, "continue")) {
        if (loop_sp > 0) {
            LoopContext* ctx = &loop_stack[loop_sp - 1];
            ctx->continue_jumps[ctx->continue_count++] = *ip;
            emit(code, ip, OP_JMP, ctx->loop_start);
        }

        return skip_to_line_end(src);
    }

    if (match_keyword(src, "int")) {
        src += 3;
        src = skip_inline_space(src);

        if (!read_identifier(&src, name)) {
            return skip_to_line_end(src);
        }

        src = skip_inline_space(src);
        if (*src == '=') {
            src++;
            compile_expr(skip_inline_space(src), code, ip);
        } else {
            emit(code, ip, OP_PUSH_INT, 0);
        }

        emit(code, ip, OP_STORE_VAR, var_index(name));
        return skip_to_line_end(src);
    }

    if (match_keyword(src, "read")) {
        src += 4;
        src = skip_inline_space(src);

        if (!read_identifier(&src, name)) {
            return skip_to_line_end(src);
        }

        emit(code, ip, OP_READ_INT, var_index(name));
        return skip_to_line_end(src);
    }

    if (is_ident_start(*src)) {
        if (!read_identifier(&src, name)) {
            return skip_to_line_end(src);
        }

        src = skip_inline_space(src);
        if (*src != '=') {
            return skip_to_line_end(src);
        }

        src++;
        compile_expr(skip_inline_space(src), code, ip);
        emit(code, ip, OP_STORE_VAR, var_index(name));
        return skip_to_line_end(src);
    }

    return skip_to_line_end(src);
}

static void parse_factor(Instruction* code, int* ip) {
    char name[MAX_VAR_NAME_LEN];

    p = skip_inline_space(p);

    if (*p == '"') {
        char* s;
        char buf[MAX_STRING_LEN];
        int k = 0;

        p++;
        while (*p && *p != '"' && k < MAX_STRING_LEN - 1) {
            buf[k++] = *p++;
        }

        buf[k] = 0;
        if (*p == '"') {
            p++;
        }

        s = add_string(buf);
        emit(code, ip, OP_PUSH_STR, (int)s);
        return;
    }

    if (*p == '(') {
        p++;
        parse_expr(code, ip);
        p = skip_inline_space(p);
        if (*p == ')') {
            p++;
        }
        return;
    }

    if (*p >= '0' && *p <= '9') {
        int v = 0;

        while (*p >= '0' && *p <= '9') {
            v = v * 10 + (*p++ - '0');
        }

        emit(code, ip, OP_PUSH_INT, v);
        return;
    }

    if (read_identifier(&p, name)) {
        const char* after_name = p;
        int native_id = native_math_id_from_name(name);
        int native_status = 0;

        /* math_*(...) имеет приоритет над одноимённой переменной. */
        if (native_id >= 0) {
            native_status = compile_native_math_call(name, &after_name, code, ip);
            if (native_status == 1) {
                p = after_name;
                return;
            }
            if (native_status == -1) {
                p = after_name;
                /* Битый вызов: подставляем 0, чтобы компиляция не падала жёстко. */
                emit(code, ip, OP_PUSH_INT, 0);
                return;
            }
        }

        p = after_name;
        emit(code, ip, OP_LOAD_VAR, var_index(name));
        return;
    }

    emit(code, ip, OP_PUSH_INT, 0);
}

static void parse_term(Instruction* code, int* ip) {
    parse_factor(code, ip);

    for (;;) {
        p = skip_inline_space(p);

        if (*p == '*' || *p == '/') {
            char op = *p++;
            parse_factor(code, ip);
            emit(code, ip, op == '*' ? OP_MUL : OP_DIV, 0);
        } else {
            break;
        }
    }
}

static void parse_expr(Instruction* code, int* ip) {
    parse_term(code, ip);

    for (;;) {
        p = skip_inline_space(p);

        if (*p == '+' || *p == '-') {
            char op = *p++;
            parse_term(code, ip);
            emit(code, ip, op == '+' ? OP_ADD : OP_SUB, 0);
        } else {
            break;
        }
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
    int i;

    for (i = 0; i < variable_count; i++) {
        int j = 0;

        while (variable_names[i][j] && name[j] && variable_names[i][j] == name[j]) {
            j++;
        }

        if (variable_names[i][j] == 0 && name[j] == 0) {
            return i;
        }
    }

    if (variable_count >= MAX_VARS) {
        return MAX_VARS - 1;
    }

    i = 0;
    while (name[i] && i < MAX_VAR_NAME_LEN - 1) {
        variable_names[variable_count][i] = name[i];
        i++;
    }
    variable_names[variable_count][i] = 0;

    return variable_count++;
}

int naw_compile(const char* src, Instruction* out) {
    int ip = 0;
    const char* line = src;

    string_count = 0;
    variable_count = 0;
    loop_sp = 0;

    while (*line) {
        line = compile_statement(line, out, &ip);
        line = skip_whitespace(line);
    }

    emit(out, &ip, OP_HALT, 0);
    return ip;
}

static void parse_compare(Instruction* code, int* ip) {
    parse_expr(code, ip);

    for (;;) {
        p = skip_inline_space(p);

        if (*p == '=' && *(p + 1) == '=') {
            p += 2;
            parse_expr(code, ip);
            emit(code, ip, OP_EQ, 0);
        } else if (*p == '!' && *(p + 1) == '=') {
            p += 2;
            parse_expr(code, ip);
            emit(code, ip, OP_NE, 0);
        } else if (*p == '<' && *(p + 1) == '=') {
            p += 2;
            parse_expr(code, ip);
            emit(code, ip, OP_LE, 0);
        } else if (*p == '>' && *(p + 1) == '=') {
            p += 2;
            parse_expr(code, ip);
            emit(code, ip, OP_GE, 0);
        } else if (*p == '<') {
            p++;
            parse_expr(code, ip);
            emit(code, ip, OP_LT, 0);
        } else if (*p == '>') {
            p++;
            parse_expr(code, ip);
            emit(code, ip, OP_GT, 0);
        } else {
            break;
        }
    }
}

static const char* compile_if(const char* src, Instruction* code, int* ip) {
    int jmp_false;

    src += 2;
    src = skip_inline_space(src);
    if (*src != '(') {
        return skip_to_line_end(src);
    }
    src++;

    compile_expr(src, code, ip);

    while (*src && *src != ')') {
        src++;
    }
    if (*src == ')') {
        src++;
    }

    jmp_false = *ip;
    emit(code, ip, OP_JMP_IF_FALSE, 0);

    src = compile_block(src, code, ip);
    src = skip_whitespace(src);

    if (match_keyword(src, "else")) {
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
    int jmp_exit;
    LoopContext* ctx;

    src += 5;
    src = skip_inline_space(src);
    if (*src != '(') {
        return skip_to_line_end(src);
    }
    src++;

    if (loop_sp >= MAX_LOOP_DEPTH) {
        return skip_to_line_end(src);
    }

    ctx = &loop_stack[loop_sp++];
    ctx->loop_start = *ip;
    ctx->break_count = 0;
    ctx->continue_count = 0;

    p = src;
    parse_compare(code, ip);
    src = p;

    while (*src && *src != ')') {
        src++;
    }
    if (*src == ')') {
        src++;
    }

    jmp_exit = *ip;
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
