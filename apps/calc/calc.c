#include "apps/calc/calc.h"
#include "kernel/terminal/terminal.h"
#include "lib/math.h"

void calc_run_expression(const char* expr) {
    while (*expr == ' ') {
        expr++;
    }

    if (*expr == '\0') {
        terminal_write("calculator is not implemented yet.\n");
        return;
    }

    {
        double result = eval_expr(expr);

        terminal_write(expr);
        terminal_write(" = ");
        terminal_write_double(result);
        terminal_write("\n");
    }
}
