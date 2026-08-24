#include "apps/calc/calc.h"
#include "kernel/terminal/terminal.h"
#include "lib/math.h"
#include "lib/nawstring.h"

static const char* calc_skip_spaces(const char* text) {
    while (text && (*text == ' ' || *text == '\t')) {
        text++;
    }

    return text;
}

static int calc_read_argument(const char** cursor, char* out, int out_size) {
    const char* src = calc_skip_spaces(*cursor);
    int depth = 0;
    int in_string = 0;
    int pos = 0;
    int started = 0;

    if (!src || *src == '\0') {
        return 0;
    }

    /*
     * Аргумент для quad: режем по пробелу/запятой только на глубине 0,
     * чтобы "(2 + 2i)" не разбивался посередине.
     */
    while (*src) {
        char c = *src;

        if (in_string) {
            if (c == '"') {
                in_string = 0;
            }

            if (pos < out_size - 1) {
                out[pos++] = c;
            }

            src++;
            started = 1;
            continue;
        }

        if (c == '"') {
            in_string = 1;
            if (pos < out_size - 1) {
                out[pos++] = c;
            }
            src++;
            started = 1;
            continue;
        }

        if (c == '(') {
            depth++;
            if (pos < out_size - 1) {
                out[pos++] = c;
            }
            src++;
            started = 1;
            continue;
        }

        if (c == ')') {
            if (depth > 0) {
                depth--;
            }
            if (pos < out_size - 1) {
                out[pos++] = c;
            }
            src++;
            started = 1;
            continue;
        }

        if ((c == ',' || c == ' ' || c == '\t') && depth == 0) {
            break;
        }

        if (pos < out_size - 1) {
            out[pos++] = c;
        }
        src++;
        started = 1;
    }

    out[pos] = '\0';
    *cursor = src;
    return started;
}

static int calc_parse_complex_arg(const char** cursor, naw_complex_t* value) {
    char expr[128];

    if (!calc_read_argument(cursor, expr, sizeof(expr))) {
        return 0;
    }

    return naw_eval_complex_expr(expr, value);
}

static void calc_print_complex(naw_complex_t value) {
    char text[128];

    if (!naw_complex_to_text(value, text, sizeof(text))) {
        terminal_write("math error");
        return;
    }

    terminal_write(text);
}

static void calc_print_usage(void) {
    terminal_write("calc:\n");
    terminal_write("calc <expr>\n");
    terminal_write("calc quad <a> <b> <c>\n");
    terminal_write("funcs: sqrt, sin, cos, tan, abs, arg, pow, conj, norm, pi, e, i\n");
}

static void calc_eval_and_print(const char* expr) {
    naw_complex_t result;

    expr = calc_skip_spaces(expr);
    if (!expr || *expr == '\0') {
        calc_print_usage();
        return;
    }

    if (!naw_eval_complex_expr(expr, &result)) {
        terminal_write("Invalid complex expression.\n");
        return;
    }

    terminal_write(expr);
    terminal_write(" = ");
    calc_print_complex(result);
    terminal_write("\n");
}

static void calc_run_quadratic_mode(const char* input) {
    const char* cursor = input;
    naw_complex_t a;
    naw_complex_t b;
    naw_complex_t c;
    naw_quadratic_result_t result;
    naw_quadratic_status_t status;

    if (!calc_parse_complex_arg(&cursor, &a) ||
        !calc_parse_complex_arg(&cursor, &b) ||
        !calc_parse_complex_arg(&cursor, &c) ||
        calc_skip_spaces(cursor)[0] != '\0') {
        calc_print_usage();
        return;
    }

    status = naw_complex_solve_quadratic(a, b, c, &result);

    if (status == NAW_QUADRATIC_LINEAR) {
        terminal_write("Linear solution: x = ");
        calc_print_complex(result.x1);
        terminal_write("\n");
        return;
    }

    if (status == NAW_QUADRATIC_INFINITE) {
        terminal_write("Quadratic equation has infinitely many solutions.\n");
        return;
    }

    if (status == NAW_QUADRATIC_NONE) {
        terminal_write("Quadratic equation has no solution.\n");
        return;
    }

    terminal_write("x1 = ");
    calc_print_complex(result.x1);
    terminal_write("\n");

    terminal_write("x2 = ");
    calc_print_complex(result.x2);
    terminal_write("\n");
}

void calc_run_expression(const char* input) {
    const char* cursor = calc_skip_spaces(input);

    if (!cursor || *cursor == '\0') {
        calc_print_usage();
        return;
    }

    /* Подкоманды: help / quad / expr; иначе вся строка — комплексное выражение. */
    if (strncmp(cursor, "help", 4) == 0 && (cursor[4] == '\0' || cursor[4] == ' ' || cursor[4] == '\t')) {
        calc_print_usage();
        return;
    }

    if (strncmp(cursor, "quad", 4) == 0 && (cursor[4] == '\0' || cursor[4] == ' ' || cursor[4] == '\t')) {
        calc_run_quadratic_mode(cursor + 4);
        return;
    }

    if (strncmp(cursor, "expr", 4) == 0 && (cursor[4] == '\0' || cursor[4] == ' ' || cursor[4] == '\t')) {
        calc_eval_and_print(cursor + 4);
        return;
    }

    calc_eval_and_print(cursor);
}
