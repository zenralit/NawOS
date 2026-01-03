#include "nawlang.h"
#include "fs/nawfs.h"
#include "drivers/screen/screen.h"
#include "lib/nawstring.h"
#include "lib/math.h"


int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, unsigned int n);
char* find_char(const char* str, char ch);
static void replace_var(char* expr, const char* name, int value);
static int eval_condition(char* cond);
static int read_block(const char** lines, int i, int count, int exec);
#define MAX_VARS 64
#define MAX_LINE 128

typedef struct {
    char name[16];
    int value;
} naw_var;

static naw_var vars[MAX_VARS];
static int var_count = 0;

//вспом

static int find_var(const char* name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0)
            return i;
    }
    return -1;
}

static void set_var(const char* name, int value) {
    int idx = find_var(name);
    if (idx >= 0) {
        vars[idx].value = value;
        return;
    }

    if (var_count < MAX_VARS) {
        strncpy(vars[var_count].name, name, 15);
        vars[var_count].value = value;
        var_count++;
    }
}

static int get_var(const char* name, int* ok) {
    int idx = find_var(name);
    if (idx >= 0) {
        *ok = 1;
        return vars[idx].value;
    }
    *ok = 0;
    return 0;
}

//ВЫПОЛНЕНИЕ СТРОКИ

static void execute_line(char* line) {
    while (*line == ' ') line++;
    if (*line == 0 || *line == '#') return;

    //print
    if (strncmp(line, "print ", 6) == 0) {
        char expr[128];
        strncpy(expr, line + 6, 127);

        // подставляем переменные
        for (int i = 0; i < var_count; i++) {
            replace_var(expr, vars[i].name, vars[i].value);
        }

        int result = eval_expr(expr);
        print_dec(result);
        print("\n");
        return;
    }

    //int x = expr
    if (strncmp(line, "int ", 4) == 0) {
        char* p = line + 4;

        char name[16] = {0};
        int i = 0;

        while (*p && *p != ' ' && *p != '=') {
            if (i < 15) name[i++] = *p;
            p++;
        }
        name[i] = 0;

        while (*p == ' ' || *p == '=') p++;

        char expr[128];
        strncpy(expr, p, 127);

        for (int j = 0; j < var_count; j++) {
            replace_var(expr, vars[j].name, vars[j].value);
        }

        int value = eval_expr(expr);
        set_var(name, value);
        return;
    }

    //x = expr
    char* eq = find_char(line, '=');
    if (eq) {
        char name[16] = {0};
        int len = eq - line;
        if (len > 15) len = 15;
        strncpy(name, line, len);

        char* expr = eq + 1;

        for (int i = 0; i < var_count; i++) {
            replace_var(expr, vars[i].name, vars[i].value);
        }

        int value = eval_expr(expr);
        set_var(name, value);
        return;
    }

    print("Unknown instruction: ");
    print(line);
    print("\n");
}
static int is_if(const char* s) {
    return s[0]=='i' && s[1]=='f' && (s[2]==' ' || s[2]=='(');
}

static int is_else(const char* s) {
    return s[0]=='e' && s[1]=='l' && s[2]=='s' && s[3]=='e' && s[4]==0;
}

static void extract_condition(const char* src, char* out) {
    int i = 0;
    while (*src && *src != '(') src++;
    if (*src == '(') src++;

    while (*src && *src != ')') {
        out[i++] = *src++;
    }
    out[i] = 0;
}

//запуск

void nawlang_run(const char* filename) {
    char name[9] = {0};
    char ext[4] = {0};

    char* dot = find_char(filename, '.');
    if (!dot) {
        print("Invalid filename\n");
        return;
    }

    int nlen = dot - filename;
    if (nlen > 8) nlen = 8;
    strncpy(name, filename, nlen);
    strncpy(ext, dot + 1, 3);

    const char* data = fs_read(name, ext);
    if (!data) {
        print("File not found\n");
        return;
    }

    char line[MAX_LINE];
    int pos = 0;

    int skip = 0;
    int last_if = 0;

    for (int i = 0;; i++) {
        char c = data[i];

        if (c == '\n' || c == '\0') {
            line[pos] = 0;
            pos = 0;

            // IF
            if (is_if(line)) {
                char cond[64];
                extract_condition(line, cond);
                last_if = eval_condition(cond);
                skip = !last_if;
                continue;
            }

            // ELSE
            if (is_else(line)) {
                skip = last_if;
                continue;
            }

            // {
            if (line[0]=='{' && line[1]==0)
                continue;

            // }
            if (line[0]=='}' && line[1]==0) {
                skip = 0;
                continue;
            }

            if (!skip && line[0])
                execute_line(line);

            if (c == '\0')
                break;
        } else {
            if (pos < MAX_LINE - 1)
                line[pos++] = c;
        }
    }
}

//
static void replace_var(char* expr, const char* name, int value) {
    char val[16];
    itoa(value, val);

    char* pos = strstr(expr, name);
    if (!pos) return;

    char buffer[128];
    int i = 0;


    while (expr[i] && &expr[i] < pos) {
        buffer[i] = expr[i];
        i++;
    }

    int j = 0;
    while (val[j]) {
        buffer[i++] = val[j++];
    }

    pos += strlen(name);

    while (*pos) {
        buffer[i++] = *pos++;
    }

    buffer[i] = 0;

    for (i = 0; buffer[i]; i++) {
        expr[i] = buffer[i];
    }
    expr[i] = 0;
}

static int eval_condition(char* cond) {
    char* ops[] = { "<=", ">=", "==", "!=", "<", ">" };
    for (int i = 0; i < 6; i++) {
        char* op = strstr(cond, ops[i]);
        if (!op) continue;

        *op = 0;
        char* left = cond;
        char* right = op + strlen(ops[i]);

        int a = eval_expr(left);
        int b = eval_expr(right);

        if (strcmp(ops[i], "<") == 0) return a < b;
        if (strcmp(ops[i], ">") == 0) return a > b;
        if (strcmp(ops[i], "<=") == 0) return a <= b;
        if (strcmp(ops[i], ">=") == 0) return a >= b;
        if (strcmp(ops[i], "==") == 0) return a == b;
        if (strcmp(ops[i], "!=") == 0) return a != b;
    }

    return 0;
}
static int read_block(const char** lines, int i, int count, int exec) {
    i++; 
    while (i < count) {
        if (strcmp(lines[i], "}") == 0) {
            return i;
        }
        if (exec) {
            execute_line((char*)lines[i]);
        }
        i++;
    }
    return i;
}

