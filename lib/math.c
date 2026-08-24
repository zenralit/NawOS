#include "lib/math.h"

/*
 * Собственная реализация элементарных функций и комплексной арифметики
 * для ядра без libm (без soft-float от toolchain).
 *
 * Активный путь вычисления выражений — комплексный парсер методом рекурсивного спуска.
 * Старый вещественный парсер оставлен под #if 0 на случай отката.
 */

static const double NAW_MATH_LN2 = 0.69314718055994530942;
/* Допуск при проверке «является ли double целым» для pow с отрицательным основанием. */
static const double NAW_MATH_INTEGER_EPSILON = 1e-9;

typedef struct {
    const char* cursor;
    int error;
} naw_expr_parser_t;

static int naw_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int naw_is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int naw_is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int naw_is_identifier_char(char c) {
    return naw_is_alpha(c) || naw_is_digit(c) || c == '_';
}

static const char* naw_skip_spaces(const char* text) {
    while (text && naw_is_space(*text)) {
        text++;
    }

    return text;
}

static double naw_abs_double(double value) {
    return value < 0.0 ? -value : value;
}

static int naw_math_is_integer(double value) {
    double truncated = (double)(int)value;

    return naw_abs_double(value - truncated) < NAW_MATH_INTEGER_EPSILON;
}

static double naw_math_pow10_int(int exponent) {
    double result = 1.0;

    while (exponent > 0) {
        result *= 10.0;
        exponent--;
    }

    while (exponent < 0) {
        result *= 0.1;
        exponent++;
    }

    return result;
}

/* Умножение/деление на степень двойки циклом — без ldexp. */
static double naw_math_scale_pow2(double value, int exponent) {
    while (exponent > 0) {
        value *= 2.0;
        exponent--;
    }

    while (exponent < 0) {
        value *= 0.5;
        exponent++;
    }

    return value;
}

/* Сводим угол к (-pi, pi], иначе ряд Тейлора для sin/cos расходится по точности. */
static double naw_math_reduce_angle(double value) {
    int turns = (int)(value / NAW_MATH_TWO_PI);

    value -= (double)turns * NAW_MATH_TWO_PI;

    if (value > NAW_MATH_PI) {
        value -= NAW_MATH_TWO_PI;
    } else if (value < -NAW_MATH_PI) {
        value += NAW_MATH_TWO_PI;
    }

    return value;
}

/* Возведение в целую степень двоичным возведением (квадраты + биты показателя). */
static double naw_math_pow_int(double base, int exponent) {
    double result = 1.0;
    int power = exponent;

    if (power < 0) {
        power = -power;
    }

    while (power > 0) {
        if (power & 1) {
            result *= base;
        }

        base *= base;
        power >>= 1;
    }

    if (exponent < 0) {
        if (result == 0.0) {
            return 0.0;
        }

        return 1.0 / result;
    }

    return result;
}

/*
 * Квадратный корень методом Ньютона.
 * Для x<=0 возвращаем 0: на вещественном пути отрицательный корень — ошибка парсера,
 * а здесь функция используется как вспомогательная.
 */
static double naw_math_sqrt(double value) {
    double guess;
    int i;

    if (value <= 0.0) {
        return 0.0;
    }

    guess = value;
    if (guess < 1.0) {
        guess = 1.0;
    }

    for (i = 0; i < 12; i++) {
        guess = 0.5 * (guess + value / guess);
    }

    return guess;
}

/* Ряд Тейлора sin x; вызывается только после сведения аргумента к [0, pi/2]. */
static double naw_math_sin_series(double value) {
    double x2 = value * value;
    double term = value;
    double sum = value;

    term *= -x2 / (2.0 * 3.0);
    sum += term;
    term *= -x2 / (4.0 * 5.0);
    sum += term;
    term *= -x2 / (6.0 * 7.0);
    sum += term;
    term *= -x2 / (8.0 * 9.0);
    sum += term;
    term *= -x2 / (10.0 * 11.0);
    sum += term;

    return sum;
}

/* Ряд Тейлора cos x на том же укороченном интервале. */
static double naw_math_cos_series(double value) {
    double x2 = value * value;
    double term = 1.0;
    double sum = 1.0;

    term *= -x2 / (1.0 * 2.0);
    sum += term;
    term *= -x2 / (3.0 * 4.0);
    sum += term;
    term *= -x2 / (5.0 * 6.0);
    sum += term;
    term *= -x2 / (7.0 * 8.0);
    sum += term;
    term *= -x2 / (9.0 * 10.0);
    sum += term;

    return sum;
}

static double naw_math_sin(double value) {
    value = naw_math_reduce_angle(value);

    if (value < 0.0) {
        return -naw_math_sin(-value);
    }

    /* sin(pi - x) = sin x: сводим к [0, pi/2] для лучшей сходимости ряда. */
    if (value > NAW_MATH_HALF_PI) {
        value = NAW_MATH_PI - value;
    }

    return naw_math_sin_series(value);
}

static double naw_math_cos(double value) {
    int sign = 1;

    value = naw_math_reduce_angle(value);
    if (value < 0.0) {
        value = -value;
    }

    /* В (pi/2, pi] косинус отрицателен; знак выносим отдельно. */
    if (value > NAW_MATH_HALF_PI) {
        value = NAW_MATH_PI - value;
        sign = -1;
    }

    return sign * naw_math_cos_series(value);
}

static double naw_math_atan_series(double value) {
    double x2 = value * value;
    double term = value;
    double sum = value;
    int i;

    for (i = 1; i <= 5; i++) {
        term *= -x2;
        sum += term / (2.0 * i + 1.0);
    }

    return sum;
}

/*
 * arctan через ряд + редукцию аргумента:
 * |x|>1 -> pi/2 - arctan(1/x); |x|>0.5 -> сдвиг на pi/4 через формулу разности.
 * Без редукции ряд плохо сходится вблизи ±1.
 */
static double naw_math_atan(double value) {
    if (value > 1.0) {
        return NAW_MATH_HALF_PI - naw_math_atan(1.0 / value);
    }

    if (value < -1.0) {
        return -NAW_MATH_HALF_PI - naw_math_atan(1.0 / value);
    }

    if (value > 0.5) {
        return (NAW_MATH_PI * 0.25) + naw_math_atan((value - 1.0) / (value + 1.0));
    }

    if (value < -0.5) {
        return -(NAW_MATH_PI * 0.25) + naw_math_atan((value + 1.0) / (1.0 - value));
    }

    return naw_math_atan_series(value);
}

/* Четыре квадранта + оси; нужен для arg(z) и полярной формы. */
static double naw_math_atan2(double y, double x) {
    if (x > 0.0) {
        return naw_math_atan(y / x);
    }

    if (x < 0.0) {
        if (y >= 0.0) {
            return naw_math_atan(y / x) + NAW_MATH_PI;
        }

        return naw_math_atan(y / x) - NAW_MATH_PI;
    }

    if (y > 0.0) {
        return NAW_MATH_HALF_PI;
    }

    if (y < 0.0) {
        return -NAW_MATH_HALF_PI;
    }

    return 0.0;
}

/*
 * exp(x) = 2^k * exp(r), где x = k*ln2 + r и |r| невелик.
 * Ряд Тейлора по r, затем домножение на степень двойки.
 */
static double naw_math_exp(double value) {
    int scale = (int)(value / NAW_MATH_LN2);
    double sum = 1.0;
    double term = 1.0;
    int i;

    value -= (double)scale * NAW_MATH_LN2;

    for (i = 1; i <= 10; i++) {
        term *= value / (double)i;
        sum += term;
    }

    return naw_math_scale_pow2(sum, scale);
}

/*
 * Натуральный логарифм для x>0.
 * Нормализуем x в [1, 2) через деление/умножение на 2, затем
 * ln x = 2*artanh((x-1)/(x+1)) + scale*ln2.
 */
static double naw_math_log_positive(double value) {
    int scale = 0;
    double y;
    double y2;
    double term;
    double sum;

    if (value <= 0.0) {
        return 0.0;
    }

    while (value >= 2.0) {
        value *= 0.5;
        scale++;
    }

    while (value < 1.0) {
        value *= 2.0;
        scale--;
    }

    y = (value - 1.0) / (value + 1.0);
    y2 = y * y;
    term = y;
    sum = y;

    term *= y2;
    sum += term / 3.0;
    term *= y2;
    sum += term / 5.0;
    term *= y2;
    sum += term / 7.0;
    term *= y2;
    sum += term / 9.0;
    term *= y2;
    sum += term / 11.0;
    term *= y2;
    sum += term / 13.0;

    return 2.0 * sum + (double)scale * NAW_MATH_LN2;
}

/*
 * Вещественная степень: целый показатель — точный путь;
 * иначе exp(exponent * ln base). Отрицательное основание с нецелым
 * показателем в R не определено — возвращаем 0 (ошибку ловит вызывающий код).
 */
static double naw_math_pow(double base, double exponent) {
    if (exponent == 0.0) {
        return 1.0;
    }

    if (base == 0.0) {
        return 0.0;
    }

    if (base < 0.0) {
        if (!naw_math_is_integer(exponent)) {
            return 0.0;
        }

        return naw_math_pow_int(base, (int)exponent);
    }

    if (naw_math_is_integer(exponent)) {
        return naw_math_pow_int(base, (int)exponent);
    }

    return naw_math_exp(exponent * naw_math_log_positive(base));
}

static double naw_math_cosh(double value) {
    double exp_value = naw_math_exp(value);
    double exp_negative = naw_math_exp(-value);

    return 0.5 * (exp_value + exp_negative);
}

static double naw_math_sinh(double value) {
    double exp_value = naw_math_exp(value);
    double exp_negative = naw_math_exp(-value);

    return 0.5 * (exp_value - exp_negative);
}

static int naw_complex_is_zero(naw_complex_t value) {
    return value.real == 0.0 && value.imag == 0.0;
}

static int naw_identifier_equals(const char* start, int length, const char* token) {
    int i = 0;

    while (token[i] != '\0' && i < length && start[i] == token[i]) {
        i++;
    }

    return token[i] == '\0' && i == length;
}

static int naw_read_identifier(const char** cursor, const char** start, int* length) {
    const char* text = naw_skip_spaces(*cursor);
    const char* begin;
    int count = 0;

    if (!naw_is_alpha(*text)) {
        return 0;
    }

    begin = text;
    text++;
    count++;

    while (naw_is_identifier_char(*text)) {
        text++;
        count++;
    }

    *cursor = text;
    if (start) {
        *start = begin;
    }
    if (length) {
        *length = count;
    }

    return 1;
}

int naw_parse_double(const char* text, double* out, const char** endptr) {
    const char* cursor;
    int sign = 1;
    double value = 0.0;
    double fraction = 0.1;
    int has_digits = 0;

    if (!text) {
        if (endptr) {
            *endptr = 0;
        }
        return 0;
    }

    cursor = naw_skip_spaces(text);
    if (*cursor == '+' || *cursor == '-') {
        if (*cursor == '-') {
            sign = -1;
        }
        cursor++;
    }

    while (naw_is_digit(*cursor)) {
        value = value * 10.0 + (double)(*cursor - '0');
        cursor++;
        has_digits = 1;
    }

    if (*cursor == '.') {
        cursor++;
        while (naw_is_digit(*cursor)) {
            value += (double)(*cursor - '0') * fraction;
            fraction *= 0.1;
            cursor++;
            has_digits = 1;
        }
    }

    /* Научная запись: 'e' без цифр показателя не считается частью числа. */
    if (*cursor == 'e' || *cursor == 'E') {
        const char* exponent_cursor = cursor + 1;
        int exponent_sign = 1;
        int exponent_value = 0;
        int exponent_digits = 0;

        if (*exponent_cursor == '+' || *exponent_cursor == '-') {
            if (*exponent_cursor == '-') {
                exponent_sign = -1;
            }
            exponent_cursor++;
        }

        while (naw_is_digit(*exponent_cursor)) {
            exponent_value = exponent_value * 10 + (*exponent_cursor - '0');
            exponent_cursor++;
            exponent_digits = 1;
        }

        if (exponent_digits) {
            cursor = exponent_cursor;
            value *= naw_math_pow10_int(exponent_sign * exponent_value);
        }
    }

    if (!has_digits) {
        if (endptr) {
            *endptr = text;
        }
        return 0;
    }

    if (out) {
        *out = sign * value;
    }

    if (endptr) {
        *endptr = cursor;
    }

    return 1;
}

naw_complex_t naw_complex_make(double real, double imag) {
    naw_complex_t value;
    value.real = real;
    value.imag = imag;
    return value;
}

naw_complex_t naw_complex_from_real(double real) {
    return naw_complex_make(real, 0.0);
}

naw_polar_t naw_complex_to_polar(naw_complex_t value) {
    naw_polar_t polar;

    polar.radius = naw_complex_abs(value);
    polar.angle = naw_complex_arg(value);
    return polar;
}

naw_complex_t naw_complex_from_polar(naw_polar_t polar) {
    return naw_complex_make(
        polar.radius * naw_math_cos(polar.angle),
        polar.radius * naw_math_sin(polar.angle));
}

naw_complex_t naw_complex_add(naw_complex_t a, naw_complex_t b) {
    return naw_complex_make(a.real + b.real, a.imag + b.imag);
}

naw_complex_t naw_complex_sub(naw_complex_t a, naw_complex_t b) {
    return naw_complex_make(a.real - b.real, a.imag - b.imag);
}

naw_complex_t naw_complex_mul(naw_complex_t a, naw_complex_t b) {
    return naw_complex_make(
        a.real * b.real - a.imag * b.imag,
        a.real * b.imag + a.imag * b.real);
}

naw_complex_t naw_complex_div(naw_complex_t a, naw_complex_t b) {
    double denom = b.real * b.real + b.imag * b.imag;

    /* Деление на ноль: тихо возвращаем 0; верхний уровень (парсер/native) проверяет сам. */
    if (denom == 0.0) {
        return naw_complex_make(0.0, 0.0);
    }

    return naw_complex_make(
        (a.real * b.real + a.imag * b.imag) / denom,
        (a.imag * b.real - a.real * b.imag) / denom);
}

naw_complex_t naw_complex_neg(naw_complex_t value) {
    return naw_complex_make(-value.real, -value.imag);
}

naw_complex_t naw_complex_scale(naw_complex_t value, double factor) {
    return naw_complex_make(value.real * factor, value.imag * factor);
}

naw_complex_t naw_complex_divide_scalar(naw_complex_t value, double factor) {
    if (factor == 0.0) {
        return naw_complex_make(0.0, 0.0);
    }

    return naw_complex_make(value.real / factor, value.imag / factor);
}

double naw_complex_abs(naw_complex_t value) {
    return naw_math_sqrt(value.real * value.real + value.imag * value.imag);
}

double naw_complex_arg(naw_complex_t value) {
    return naw_math_atan2(value.imag, value.real);
}

naw_complex_t naw_complex_conjugate(naw_complex_t value) {
    return naw_complex_make(value.real, -value.imag);
}

naw_complex_t naw_complex_normalize(naw_complex_t value) {
    double length = naw_complex_abs(value);

    if (length == 0.0) {
        return naw_complex_make(0.0, 0.0);
    }

    return naw_complex_divide_scalar(value, length);
}

static naw_complex_t naw_complex_pow_int(naw_complex_t value, int exponent) {
    naw_complex_t result = naw_complex_from_real(1.0);
    int power = exponent;

    if (power < 0) {
        if (naw_complex_is_zero(value)) {
            return naw_complex_make(0.0, 0.0);
        }
        power = -power;
    }

    while (power > 0) {
        if (power & 1) {
            result = naw_complex_mul(result, value);
        }

        value = naw_complex_mul(value, value);
        power >>= 1;
    }

    if (exponent < 0) {
        return naw_complex_div(naw_complex_from_real(1.0), result);
    }

    return result;
}

naw_complex_t naw_complex_pow_real(naw_complex_t value, double power) {
    naw_polar_t polar;

    if (power == 0.0) {
        return naw_complex_from_real(1.0);
    }

    if (naw_complex_is_zero(value)) {
        if (power > 0.0) {
            return naw_complex_make(0.0, 0.0);
        }

        return naw_complex_make(0.0, 0.0);
    }

    if (naw_math_is_integer(power)) {
        return naw_complex_pow_int(value, (int)power);
    }

    /* z^p = r^p * exp(i * p * arg(z)); берём главное значение аргумента. */
    polar = naw_complex_to_polar(value);
    polar.radius = naw_math_pow(polar.radius, power);
    polar.angle *= power;
    return naw_complex_from_polar(polar);
}

/*
 * Комплексный квадратный корень в алгебраической форме:
 *   Re = sqrt((|z| + Re z)/2),  Im = sign(Im z) * sqrt((|z| - Re z)/2).
 * Выбирается ветвь с неотрицательной действительной частью.
 */
naw_complex_t naw_complex_sqrt(naw_complex_t value) {
    double magnitude;
    double real_term;
    double imag_term;
    double real_part;
    double imag_part;

    if (naw_complex_is_zero(value)) {
        return naw_complex_make(0.0, 0.0);
    }

    magnitude = naw_complex_abs(value);
    real_term = (magnitude + value.real) * 0.5;
    imag_term = (magnitude - value.real) * 0.5;

    if (real_term < 0.0) {
        real_term = 0.0;
    }

    if (imag_term < 0.0) {
        imag_term = 0.0;
    }

    real_part = naw_math_sqrt(real_term);
    imag_part = naw_math_sqrt(imag_term);

    if (value.imag < 0.0) {
        imag_part = -imag_part;
    }

    return naw_complex_make(real_part, imag_part);
}

/* sin(x+iy) = sin x cosh y + i cos x sinh y */
naw_complex_t naw_complex_sin(naw_complex_t value) {
    return naw_complex_make(
        naw_math_sin(value.real) * naw_math_cosh(value.imag),
        naw_math_cos(value.real) * naw_math_sinh(value.imag));
}

/* cos(x+iy) = cos x cosh y - i sin x sinh y */
naw_complex_t naw_complex_cos(naw_complex_t value) {
    return naw_complex_make(
        naw_math_cos(value.real) * naw_math_cosh(value.imag),
        -naw_math_sin(value.real) * naw_math_sinh(value.imag));
}

/*
 * Решение ax^2 + bx + c = 0 над C.
 * a=0, b≠0 -> линейное; a=b=0 -> либо тождество, либо противоречие.
 */
naw_quadratic_status_t naw_complex_solve_quadratic(
    naw_complex_t a,
    naw_complex_t b,
    naw_complex_t c,
    naw_quadratic_result_t* out) {
    naw_complex_t two_a;
    naw_complex_t discriminant;
    naw_complex_t sqrt_discriminant;
    naw_complex_t neg_b;

    if (out) {
        out->x1 = naw_complex_make(0.0, 0.0);
        out->x2 = naw_complex_make(0.0, 0.0);
        out->status = NAW_QUADRATIC_NONE;
    }

    if (naw_complex_is_zero(a)) {
        if (naw_complex_is_zero(b)) {
            if (out) {
                out->status = naw_complex_is_zero(c) ? NAW_QUADRATIC_INFINITE : NAW_QUADRATIC_NONE;
            }
            return naw_complex_is_zero(c) ? NAW_QUADRATIC_INFINITE : NAW_QUADRATIC_NONE;
        }

        if (out) {
            out->x1 = naw_complex_div(naw_complex_neg(c), b);
            out->x2 = out->x1;
            out->status = NAW_QUADRATIC_LINEAR;
        }

        return NAW_QUADRATIC_LINEAR;
    }

    two_a = naw_complex_scale(a, 2.0);
    discriminant = naw_complex_sub(naw_complex_mul(b, b), naw_complex_scale(naw_complex_mul(a, c), 4.0));
    sqrt_discriminant = naw_complex_sqrt(discriminant);
    neg_b = naw_complex_neg(b);

    if (out) {
        out->x1 = naw_complex_div(naw_complex_add(neg_b, sqrt_discriminant), two_a);
        out->x2 = naw_complex_div(naw_complex_sub(neg_b, sqrt_discriminant), two_a);
        out->status = NAW_QUADRATIC_OK;
    }

    return NAW_QUADRATIC_OK;
}

#if 0
static double naw_parse_function_call(naw_expr_parser_t* parser, const char* name, int length) {
    double first;
    double second;

    if (!naw_parser_match_char(parser, '(')) {
        parser->error = 1;
        return 0.0;
    }

    if (naw_identifier_equals(name, length, "pow")) {
        first = naw_parse_expression_internal(parser);
        if (!naw_parser_match_char(parser, ',')) {
            parser->error = 1;
            return 0.0;
        }

        second = naw_parse_expression_internal(parser);
        if (!naw_parser_match_char(parser, ')')) {
            parser->error = 1;
            return 0.0;
        }

        if ((first == 0.0 && second <= 0.0) ||
            (first < 0.0 && !naw_math_is_integer(second))) {
            parser->error = 1;
            return 0.0;
        }

        return naw_math_pow(first, second);
    }

    first = naw_parse_expression_internal(parser);
    if (!naw_parser_match_char(parser, ')')) {
        parser->error = 1;
        return 0.0;
    }

    if (naw_identifier_equals(name, length, "sqrt")) {
        if (first < 0.0) {
            parser->error = 1;
            return 0.0;
        }
        return naw_math_sqrt(first);
    }

    if (naw_identifier_equals(name, length, "sin")) {
        return naw_math_sin(first);
    }

    if (naw_identifier_equals(name, length, "cos")) {
        return naw_math_cos(first);
    }

    if (naw_identifier_equals(name, length, "abs")) {
        return naw_abs_double(first);
    }

    if (naw_identifier_equals(name, length, "exp")) {
        return naw_math_exp(first);
    }

    if (naw_identifier_equals(name, length, "log")) {
        if (first <= 0.0) {
            parser->error = 1;
            return 0.0;
        }
        return naw_math_log_positive(first);
    }

    if (naw_identifier_equals(name, length, "tan")) {
        double cosine = naw_math_cos(first);
        if (cosine == 0.0) {
            parser->error = 1;
            return 0.0;
        }
        return naw_math_sin(first) / cosine;
    }

    parser->error = 1;
    return 0.0;
}

static double naw_parse_primary_internal(naw_expr_parser_t* parser) {
    const char* name;
    int length;
    double value;

    parser->cursor = naw_skip_spaces(parser->cursor);

    if (*parser->cursor == '(') {
        parser->cursor++;
        value = naw_parse_expression_internal(parser);
        if (!naw_parser_match_char(parser, ')')) {
            parser->error = 1;
        }
        return value;
    }

    if (naw_parse_double(parser->cursor, &value, &parser->cursor)) {
        return value;
    }

    if (!naw_read_identifier(&parser->cursor, &name, &length)) {
        parser->error = 1;
        return 0.0;
    }

    parser->cursor = naw_skip_spaces(parser->cursor);
    if (naw_identifier_equals(name, length, "pi")) {
        return NAW_MATH_PI;
    }

    if (naw_identifier_equals(name, length, "e")) {
        return NAW_MATH_E;
    }

    if (*parser->cursor != '(') {
        parser->error = 1;
        return 0.0;
    }

    return naw_parse_function_call(parser, name, length);
}

static double naw_parse_power_internal(naw_expr_parser_t* parser) {
    double left = naw_parse_primary_internal(parser);

    parser->cursor = naw_skip_spaces(parser->cursor);
    if (!parser->error && *parser->cursor == '^') {
        double right;

        parser->cursor++;
        right = naw_parse_unary_internal(parser);
        if ((left == 0.0 && right <= 0.0) ||
            (left < 0.0 && !naw_math_is_integer(right))) {
            parser->error = 1;
            return 0.0;
        }
        left = naw_math_pow(left, right);
    }

    return left;
}

static double naw_parse_unary_internal(naw_expr_parser_t* parser) {
    parser->cursor = naw_skip_spaces(parser->cursor);

    if (*parser->cursor == '+') {
        parser->cursor++;
        return naw_parse_unary_internal(parser);
    }

    if (*parser->cursor == '-') {
        parser->cursor++;
        return -naw_parse_unary_internal(parser);
    }

    return naw_parse_power_internal(parser);
}

static double naw_parse_term_internal(naw_expr_parser_t* parser) {
    double value = naw_parse_unary_internal(parser);

    while (!parser->error) {
        parser->cursor = naw_skip_spaces(parser->cursor);

        if (*parser->cursor == '*') {
            double rhs;

            parser->cursor++;
            rhs = naw_parse_unary_internal(parser);
            value *= rhs;
        } else if (*parser->cursor == '/') {
            double rhs;

            parser->cursor++;
            rhs = naw_parse_unary_internal(parser);
            if (rhs == 0.0) {
                parser->error = 1;
                return 0.0;
            }
            value /= rhs;
        } else {
            break;
        }
    }

    return value;
}

static double naw_parse_expression_internal(naw_expr_parser_t* parser) {
    double value = naw_parse_term_internal(parser);

    while (!parser->error) {
        parser->cursor = naw_skip_spaces(parser->cursor);

        if (*parser->cursor == '+') {
            double rhs;

            parser->cursor++;
            rhs = naw_parse_term_internal(parser);
            value += rhs;
        } else if (*parser->cursor == '-') {
            double rhs;

            parser->cursor++;
            rhs = naw_parse_term_internal(parser);
            value -= rhs;
        } else {
            break;
        }
    }

    return value;
}

int naw_eval_expr(const char* expr, double* out) {
    naw_expr_parser_t parser;
    double value;

    if (!expr || !out) {
        return 0;
    }

    parser.cursor = expr;
    parser.error = 0;
    value = naw_parse_expression_internal(&parser);
    parser.cursor = naw_skip_spaces(parser.cursor);

    if (parser.error || *parser.cursor != '\0') {
        return 0;
    }

    *out = value;
    return 1;
}

double eval_expr(const char* expr) {
    double value = 0.0;

    if (!naw_eval_expr(expr, &value)) {
        return 0.0;
    }

    return value;
}
#endif

/* Кольцевой пул строк для native-ответов: указатели живут до следующего вытеснения слота. */
#define NAW_MATH_TEXT_BUFFER_COUNT 8
#define NAW_MATH_TEXT_BUFFER_SIZE 192

typedef struct {
    const char* cursor;
    int error;
} naw_complex_expr_parser_t;

static char naw_math_text_buffers[NAW_MATH_TEXT_BUFFER_COUNT][NAW_MATH_TEXT_BUFFER_SIZE];
static int naw_math_text_buffer_index = 0;

static char* naw_math_text_buffer(void) {
    char* buffer = naw_math_text_buffers[naw_math_text_buffer_index];

    naw_math_text_buffer_index++;
    if (naw_math_text_buffer_index >= NAW_MATH_TEXT_BUFFER_COUNT) {
        naw_math_text_buffer_index = 0;
    }

    buffer[0] = '\0';
    return buffer;
}

static void naw_text_append_char(char* out, unsigned int size, unsigned int* pos, char c) {
    if (*pos + 1 < size) {
        out[*pos] = c;
        (*pos)++;
    }
}

static void naw_text_append_str(char* out, unsigned int size, unsigned int* pos, const char* text) {
    unsigned int i = 0;

    while (text[i]) {
        naw_text_append_char(out, size, pos, text[i]);
        i++;
    }
}

static void naw_text_append_uint(char* out, unsigned int size, unsigned int* pos, unsigned int value) {
    char buf[16];
    unsigned int i = 0;

    if (value == 0) {
        naw_text_append_char(out, size, pos, '0');
        return;
    }

    while (value > 0 && i < sizeof(buf)) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (i > 0) {
        i--;
        naw_text_append_char(out, size, pos, buf[i]);
    }
}

static int naw_double_to_buffer(double value, char* out, unsigned int out_size) {
    unsigned int pos = 0;
    int negative = 0;
    unsigned int int_part;
    unsigned int frac_part;
    double abs_value = value;

    if (!out || out_size == 0) {
        return 0;
    }

    if (abs_value < 0.0) {
        negative = 1;
        abs_value = -abs_value;
    }

    /* Фиксированные 4 знака после точки; округление может перенести единицу в целую часть. */
    int_part = (unsigned int)abs_value;
    abs_value -= (double)int_part;
    frac_part = (unsigned int)(abs_value * 10000.0 + 0.5);

    if (frac_part >= 10000U) {
        int_part++;
        frac_part -= 10000U;
    }

    if (negative && (int_part != 0U || frac_part != 0U)) {
        naw_text_append_char(out, out_size, &pos, '-');
    }

    naw_text_append_uint(out, out_size, &pos, int_part);
    naw_text_append_char(out, out_size, &pos, '.');

    naw_text_append_char(out, out_size, &pos, (char)('0' + ((frac_part / 1000U) % 10U)));
    naw_text_append_char(out, out_size, &pos, (char)('0' + ((frac_part / 100U) % 10U)));
    naw_text_append_char(out, out_size, &pos, (char)('0' + ((frac_part / 10U) % 10U)));
    naw_text_append_char(out, out_size, &pos, (char)('0' + (frac_part % 10U)));

    out[pos] = '\0';
    return 1;
}

int naw_double_to_text(double value, char* out, unsigned int out_size) {
    return naw_double_to_buffer(value, out, out_size);
}

int naw_complex_to_text(naw_complex_t value, char* out, unsigned int out_size) {
    char real_buf[64];
    char imag_buf[64];
    unsigned int pos = 0;

    if (!out || out_size == 0) {
        return 0;
    }

    naw_double_to_buffer(value.real, real_buf, sizeof(real_buf));
    naw_double_to_buffer(value.imag < 0.0 ? -value.imag : value.imag, imag_buf, sizeof(imag_buf));

    naw_text_append_char(out, out_size, &pos, '(');
    naw_text_append_str(out, out_size, &pos, real_buf);
    naw_text_append_str(out, out_size, &pos, value.imag < 0.0 ? " - " : " + ");
    naw_text_append_str(out, out_size, &pos, imag_buf);
    naw_text_append_char(out, out_size, &pos, 'i');
    naw_text_append_char(out, out_size, &pos, ')');
    out[pos] = '\0';
    return 1;
}

static naw_complex_t naw_complex_make_zero(void) {
    return naw_complex_make(0.0, 0.0);
}

static int naw_complex_is_zeroish(naw_complex_t value) {
    return value.real == 0.0 && value.imag == 0.0;
}

static naw_complex_t naw_complex_exp_value(naw_complex_t value) {
    /* exp(x+iy) = e^x (cos y + i sin y) */
    double exp_real = naw_math_exp(value.real);
    return naw_complex_make(exp_real * naw_math_cos(value.imag), exp_real * naw_math_sin(value.imag));
}

static int naw_complex_log_value(naw_complex_t value, naw_complex_t* out) {
    if (naw_complex_is_zeroish(value)) {
        return 0;
    }

    /* Главная ветвь: ln|z| + i arg(z). */
    out->real = naw_math_log_positive(naw_complex_abs(value));
    out->imag = naw_complex_arg(value);
    return 1;
}

static int naw_complex_pow_value(naw_complex_t base, naw_complex_t exponent, naw_complex_t* out) {
    naw_complex_t log_base;

    if (naw_complex_is_zeroish(exponent)) {
        *out = naw_complex_make(1.0, 0.0);
        return 1;
    }

    if (naw_complex_is_zeroish(base)) {
        if (exponent.imag == 0.0 && exponent.real > 0.0) {
            *out = naw_complex_make_zero();
            return 1;
        }

        return 0;
    }

    if (exponent.imag == 0.0) {
        *out = naw_complex_pow_real(base, exponent.real);
        return 1;
    }

    /* z^w = exp(w * Log z) на главной ветви логарифма. */
    if (!naw_complex_log_value(base, &log_base)) {
        return 0;
    }

    *out = naw_complex_exp_value(naw_complex_mul(exponent, log_base));
    return 1;
}

/* Рекурсивный спуск: expression -> term -> unary -> power -> primary. */
static int naw_complex_parse_primary(naw_complex_expr_parser_t* parser, naw_complex_t* out);
static int naw_complex_parse_expression(naw_complex_expr_parser_t* parser, naw_complex_t* out);
static int naw_complex_parse_term(naw_complex_expr_parser_t* parser, naw_complex_t* out);
static int naw_complex_parse_unary(naw_complex_expr_parser_t* parser, naw_complex_t* out);
static int naw_complex_parse_power(naw_complex_expr_parser_t* parser, naw_complex_t* out);

static int naw_complex_parser_match_char(naw_complex_expr_parser_t* parser, char expected) {
    parser->cursor = naw_skip_spaces(parser->cursor);

    if (*parser->cursor != expected) {
        return 0;
    }

    parser->cursor++;
    return 1;
}

static int naw_complex_parse_identifier_call(naw_complex_expr_parser_t* parser, const char* name, int len, naw_complex_t* out) {
    naw_complex_t a;
    naw_complex_t b;
    naw_complex_t c;

    if (!naw_complex_parser_match_char(parser, '(')) {
        return 0;
    }

    if (naw_identifier_equals(name, len, "pow")) {
        if (!naw_complex_parse_expression(parser, &a) || !naw_complex_parser_match_char(parser, ',')) {
            return 0;
        }

        if (!naw_complex_parse_expression(parser, &b) || !naw_complex_parser_match_char(parser, ')')) {
            return 0;
        }

        return naw_complex_pow_value(a, b, out);
    }

    if (!naw_complex_parse_expression(parser, &a) || !naw_complex_parser_match_char(parser, ')')) {
        return 0;
    }

    if (naw_identifier_equals(name, len, "sqrt")) {
        *out = naw_complex_sqrt(a);
        return 1;
    }

    if (naw_identifier_equals(name, len, "sin")) {
        *out = naw_complex_sin(a);
        return 1;
    }

    if (naw_identifier_equals(name, len, "cos")) {
        *out = naw_complex_cos(a);
        return 1;
    }

    if (naw_identifier_equals(name, len, "tan")) {
        c = naw_complex_cos(a);
        if (naw_complex_is_zeroish(c)) {
            return 0;
        }
        *out = naw_complex_div(naw_complex_sin(a), c);
        return 1;
    }

    if (naw_identifier_equals(name, len, "exp")) {
        *out = naw_complex_exp_value(a);
        return 1;
    }

    if (naw_identifier_equals(name, len, "log")) {
        return naw_complex_log_value(a, out);
    }

    if (naw_identifier_equals(name, len, "abs")) {
        *out = naw_complex_make(naw_complex_abs(a), 0.0);
        return 1;
    }

    if (naw_identifier_equals(name, len, "arg")) {
        *out = naw_complex_make(naw_complex_arg(a), 0.0);
        return 1;
    }

    if (naw_identifier_equals(name, len, "conj")) {
        *out = naw_complex_conjugate(a);
        return 1;
    }

    if (naw_identifier_equals(name, len, "norm")) {
        *out = naw_complex_normalize(a);
        return 1;
    }

    if (naw_identifier_equals(name, len, "re")) {
        *out = naw_complex_make(a.real, 0.0);
        return 1;
    }

    if (naw_identifier_equals(name, len, "im")) {
        *out = naw_complex_make(a.imag, 0.0);
        return 1;
    }

    return 0;
}

static int naw_complex_parse_number_literal(naw_complex_expr_parser_t* parser, naw_complex_t* out) {
    double value;
    const char* end = 0;
    const char* start = parser->cursor;

    /* Голая 'i' / 'I' — мнимая единица; суффикс после числа даёт чисто мнимое (2i). */
    if (*start == 'i' || *start == 'I') {
        parser->cursor++;
        *out = naw_complex_make(0.0, 1.0);
        return 1;
    }

    if (!naw_parse_double(start, &value, &end)) {
        return 0;
    }

    parser->cursor = end;
    if (*parser->cursor == 'i' || *parser->cursor == 'I') {
        parser->cursor++;
        *out = naw_complex_make(0.0, value);
    } else {
        *out = naw_complex_make(value, 0.0);
    }

    return 1;
}

static int naw_complex_parse_primary(naw_complex_expr_parser_t* parser, naw_complex_t* out) {
    const char* name;
    int length;

    parser->cursor = naw_skip_spaces(parser->cursor);

    if (*parser->cursor == '(') {
        parser->cursor++;
        if (!naw_complex_parse_expression(parser, out) || !naw_complex_parser_match_char(parser, ')')) {
            return 0;
        }
        return 1;
    }

    if (naw_complex_parse_number_literal(parser, out)) {
        return 1;
    }

    if (!naw_read_identifier(&parser->cursor, &name, &length)) {
        return 0;
    }

    if (naw_identifier_equals(name, length, "pi")) {
        *out = naw_complex_make(NAW_MATH_PI, 0.0);
        return 1;
    }

    if (naw_identifier_equals(name, length, "e")) {
        *out = naw_complex_make(NAW_MATH_E, 0.0);
        return 1;
    }

    if (naw_identifier_equals(name, length, "i")) {
        *out = naw_complex_make(0.0, 1.0);
        return 1;
    }

    if (naw_complex_parse_identifier_call(parser, name, length, out)) {
        return 1;
    }

    return 0;
}

static int naw_complex_parse_power(naw_complex_expr_parser_t* parser, naw_complex_t* out) {
    naw_complex_t left;
    naw_complex_t right;

    if (!naw_complex_parse_primary(parser, &left)) {
        return 0;
    }

    /* '^' правоассоциативен через unary справа (как в calc: 2^-3). */
    parser->cursor = naw_skip_spaces(parser->cursor);
    if (*parser->cursor == '^') {
        parser->cursor++;
        if (!naw_complex_parse_unary(parser, &right)) {
            return 0;
        }

        if (!naw_complex_pow_value(left, right, out)) {
            return 0;
        }
        return 1;
    }

    *out = left;
    return 1;
}

static int naw_complex_parse_unary(naw_complex_expr_parser_t* parser, naw_complex_t* out) {
    parser->cursor = naw_skip_spaces(parser->cursor);

    if (*parser->cursor == '+') {
        parser->cursor++;
        return naw_complex_parse_unary(parser, out);
    }

    if (*parser->cursor == '-') {
        parser->cursor++;
        if (!naw_complex_parse_unary(parser, out)) {
            return 0;
        }
        out->real = -out->real;
        out->imag = -out->imag;
        return 1;
    }

    return naw_complex_parse_power(parser, out);
}

static int naw_complex_parse_term(naw_complex_expr_parser_t* parser, naw_complex_t* out) {
    naw_complex_t left;
    naw_complex_t right;

    if (!naw_complex_parse_unary(parser, &left)) {
        return 0;
    }

    for (;;) {
        parser->cursor = naw_skip_spaces(parser->cursor);

        if (*parser->cursor == '*') {
            parser->cursor++;
            if (!naw_complex_parse_unary(parser, &right)) {
                return 0;
            }
            left = naw_complex_mul(left, right);
        } else if (*parser->cursor == '/') {
            parser->cursor++;
            if (!naw_complex_parse_unary(parser, &right)) {
                return 0;
            }
            if (naw_complex_is_zeroish(right)) {
                return 0;
            }
            left = naw_complex_div(left, right);
        } else {
            break;
        }
    }

    *out = left;
    return 1;
}

static int naw_complex_parse_expression(naw_complex_expr_parser_t* parser, naw_complex_t* out) {
    naw_complex_t left;
    naw_complex_t right;

    if (!naw_complex_parse_term(parser, &left)) {
        return 0;
    }

    for (;;) {
        parser->cursor = naw_skip_spaces(parser->cursor);

        if (*parser->cursor == '+') {
            parser->cursor++;
            if (!naw_complex_parse_term(parser, &right)) {
                return 0;
            }
            left = naw_complex_add(left, right);
        } else if (*parser->cursor == '-') {
            parser->cursor++;
            if (!naw_complex_parse_term(parser, &right)) {
                return 0;
            }
            left = naw_complex_sub(left, right);
        } else {
            break;
        }
    }

    *out = left;
    return 1;
}

int naw_eval_complex_expr(const char* expr, naw_complex_t* out) {
    naw_complex_expr_parser_t parser;

    if (!expr || !out) {
        return 0;
    }

    parser.cursor = expr;
    parser.error = 0;

    if (!naw_complex_parse_expression(&parser, out)) {
        return 0;
    }

    /* Требуем полное потребление ввода — хвост после выражения это ошибка. */
    parser.cursor = naw_skip_spaces(parser.cursor);
    return parser.error == 0 && *parser.cursor == '\0';
}

static int naw_math_eval_arg(const char* text, naw_complex_t* value) {
    return naw_eval_complex_expr(text, value);
}

static char* naw_math_native_buffer(void) {
    return naw_math_text_buffer();
}

const char* naw_math_native_call(naw_math_native_id_t native_id, const char** args, int argc) {
    char* out = naw_math_native_buffer();
    naw_complex_t a;
    naw_complex_t b;
    naw_complex_t c;
    naw_complex_t result;

    if (!out) {
        return "math error";
    }

    /* Мост Lelya -> math: аргументы — строки-выражения, ответ — текст из кольцевого буфера. */
    switch (native_id) {
        case NAW_MATH_NATIVE_EVAL:
            if (argc < 1 || !naw_math_eval_arg(args[0], &result)) {
                return "math error";
            }
            naw_complex_to_text(result, out, NAW_MATH_TEXT_BUFFER_SIZE);
            return out;

        case NAW_MATH_NATIVE_ADD:
        case NAW_MATH_NATIVE_SUB:
        case NAW_MATH_NATIVE_MUL:
        case NAW_MATH_NATIVE_DIV:
        case NAW_MATH_NATIVE_POW:
            if (argc < 2 || !naw_math_eval_arg(args[0], &a) || !naw_math_eval_arg(args[1], &b)) {
                return "math error";
            }

            if (native_id == NAW_MATH_NATIVE_ADD) {
                result = naw_complex_add(a, b);
            } else if (native_id == NAW_MATH_NATIVE_SUB) {
                result = naw_complex_sub(a, b);
            } else if (native_id == NAW_MATH_NATIVE_MUL) {
                result = naw_complex_mul(a, b);
            } else if (native_id == NAW_MATH_NATIVE_DIV) {
                if (naw_complex_is_zeroish(b)) {
                    return "math error";
                }
                result = naw_complex_div(a, b);
            } else {
                if (!naw_complex_pow_value(a, b, &result)) {
                    return "math error";
                }
            }

            naw_complex_to_text(result, out, NAW_MATH_TEXT_BUFFER_SIZE);
            return out;

        case NAW_MATH_NATIVE_SQRT:
        case NAW_MATH_NATIVE_SIN:
        case NAW_MATH_NATIVE_COS:
        case NAW_MATH_NATIVE_TAN:
        case NAW_MATH_NATIVE_ABS:
        case NAW_MATH_NATIVE_ARG:
        case NAW_MATH_NATIVE_CONJ:
        case NAW_MATH_NATIVE_NORM:
            if (argc < 1 || !naw_math_eval_arg(args[0], &a)) {
                return "math error";
            }

            switch (native_id) {
                case NAW_MATH_NATIVE_SQRT:
                    result = naw_complex_sqrt(a);
                    naw_complex_to_text(result, out, NAW_MATH_TEXT_BUFFER_SIZE);
                    return out;
                case NAW_MATH_NATIVE_SIN:
                    result = naw_complex_sin(a);
                    naw_complex_to_text(result, out, NAW_MATH_TEXT_BUFFER_SIZE);
                    return out;
                case NAW_MATH_NATIVE_COS:
                    result = naw_complex_cos(a);
                    naw_complex_to_text(result, out, NAW_MATH_TEXT_BUFFER_SIZE);
                    return out;
                case NAW_MATH_NATIVE_TAN:
                    b = naw_complex_cos(a);
                    if (naw_complex_is_zeroish(b)) {
                        return "math error";
                    }
                    result = naw_complex_div(naw_complex_sin(a), b);
                    naw_complex_to_text(result, out, NAW_MATH_TEXT_BUFFER_SIZE);
                    return out;
                case NAW_MATH_NATIVE_ABS:
                    naw_double_to_buffer(naw_complex_abs(a), out, NAW_MATH_TEXT_BUFFER_SIZE);
                    return out;
                case NAW_MATH_NATIVE_ARG:
                    naw_double_to_buffer(naw_complex_arg(a), out, NAW_MATH_TEXT_BUFFER_SIZE);
                    return out;
                case NAW_MATH_NATIVE_CONJ:
                    result = naw_complex_conjugate(a);
                    naw_complex_to_text(result, out, NAW_MATH_TEXT_BUFFER_SIZE);
                    return out;
                case NAW_MATH_NATIVE_NORM:
                    result = naw_complex_normalize(a);
                    naw_complex_to_text(result, out, NAW_MATH_TEXT_BUFFER_SIZE);
                    return out;
                default:
                    break;
            }
            return "math error";

        case NAW_MATH_NATIVE_QUAD: {
            naw_quadratic_result_t qr;
            naw_quadratic_status_t status;
            char x1[128];
            char x2[128];

            if (argc < 3 ||
                !naw_math_eval_arg(args[0], &a) ||
                !naw_math_eval_arg(args[1], &b) ||
                !naw_math_eval_arg(args[2], &c)) {
                return "math error";
            }

            status = naw_complex_solve_quadratic(a, b, c, &qr);
            if (status == NAW_QUADRATIC_NONE) {
                return "quadratic equation has no solution";
            }
            if (status == NAW_QUADRATIC_INFINITE) {
                return "quadratic equation has infinitely many solutions";
            }

            naw_complex_to_text(qr.x1, x1, sizeof(x1));
            naw_complex_to_text(qr.x2, x2, sizeof(x2));
            if (status == NAW_QUADRATIC_LINEAR) {
                char* buffer = out;
                unsigned int pos = 0;
                naw_text_append_str(buffer, NAW_MATH_TEXT_BUFFER_SIZE, &pos, "linear solution: x = ");
                naw_text_append_str(buffer, NAW_MATH_TEXT_BUFFER_SIZE, &pos, x1);
                buffer[pos] = '\0';
                return buffer;
            }

            {
                unsigned int pos = 0;
                naw_text_append_str(out, NAW_MATH_TEXT_BUFFER_SIZE, &pos, "x1 = ");
                naw_text_append_str(out, NAW_MATH_TEXT_BUFFER_SIZE, &pos, x1);
                naw_text_append_str(out, NAW_MATH_TEXT_BUFFER_SIZE, &pos, "\nx2 = ");
                naw_text_append_str(out, NAW_MATH_TEXT_BUFFER_SIZE, &pos, x2);
                out[pos] = '\0';
            }
            return out;
        }

        default:
            break;
    }

    return "math error";
}

int naw_eval_expr(const char* expr, double* out) {
    naw_complex_t value;

    if (!expr || !out) {
        return 0;
    }

    if (!naw_eval_complex_expr(expr, &value)) {
        return 0;
    }

    /* Совместимость со старым API: комплексный ненулевой Im — не вещественный результат. */
    if (value.imag != 0.0) {
        return 0;
    }

    *out = value.real;
    return 1;
}

double eval_expr(const char* expr) {
    double value = 0.0;

    if (!naw_eval_expr(expr, &value)) {
        return 0.0;
    }

    return value;
}
