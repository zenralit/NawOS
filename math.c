int is_digit(char c) {
    return c >= '0' && c <= '9';
}

int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}
double eval_expr(const char* expr) {
    double result = 0;
    double current = 0;
    char op = '+';

    while (*expr) {
        while (*expr == ' ') expr++;

        double num = 0;
        int frac = 0;
        double factor = 0.1;

        
        while (*expr && (is_digit(*expr) || *expr == '.')) {
            if (*expr == '.') {
                frac = 1;
                expr++;
                continue;
            }

            if (!frac) {
                num = num * 10 + (*expr - '0');
            } else {
                num += (*expr - '0') * factor;
                factor *= 0.1;
            }
            expr++;
        }

        
        switch (op) {
            case '+': result += current; current = num; break;
            case '-': result += current; current = -num; break;
            case '*': current *= num; break;
            case '/': current /= num; break;
        }

        while (*expr == ' ') expr++;
        if (*expr == '\0') break;
        op = *expr++;
    }

    result += current;
    
    return result;
}