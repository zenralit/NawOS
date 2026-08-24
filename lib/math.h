#ifndef LIB_MATH_H
#define LIB_MATH_H

/*
 * Математика NawOS без libm: комплексные числа как базовый тип.
 * Вещественные выражения считаются как комплексные с нулевой мнимой частью.
 */

#define NAW_MATH_PI 3.14159265358979323846
#define NAW_MATH_TWO_PI (NAW_MATH_PI * 2.0)
#define NAW_MATH_HALF_PI (NAW_MATH_PI * 0.5)
#define NAW_MATH_E 2.71828182845904523536

typedef struct {
    double real;
    double imag;
} naw_complex_t;

typedef struct {
    double radius;
    double angle;
} naw_polar_t;

/* Статус решения ax^2+bx+c=0, включая вырожденные случаи a=0 / a=b=0. */
typedef enum {
    NAW_QUADRATIC_OK = 0,
    NAW_QUADRATIC_LINEAR,
    NAW_QUADRATIC_INFINITE,
    NAW_QUADRATIC_NONE
} naw_quadratic_status_t;

typedef struct {
    naw_complex_t x1;
    naw_complex_t x2;
    naw_quadratic_status_t status;
} naw_quadratic_result_t;

/* Идентификаторы native-вызовов Lelya; совпадают с аргументом OP_NATIVE_CALL. */
typedef enum {
    NAW_MATH_NATIVE_EVAL = 0,
    NAW_MATH_NATIVE_ADD,
    NAW_MATH_NATIVE_SUB,
    NAW_MATH_NATIVE_MUL,
    NAW_MATH_NATIVE_DIV,
    NAW_MATH_NATIVE_POW,
    NAW_MATH_NATIVE_SQRT,
    NAW_MATH_NATIVE_SIN,
    NAW_MATH_NATIVE_COS,
    NAW_MATH_NATIVE_TAN,
    NAW_MATH_NATIVE_ABS,
    NAW_MATH_NATIVE_ARG,
    NAW_MATH_NATIVE_CONJ,
    NAW_MATH_NATIVE_NORM,
    NAW_MATH_NATIVE_QUAD
} naw_math_native_id_t;

int naw_parse_double(const char* text, double* out, const char** endptr);
/* Вещественный eval: ошибка, если результат имеет ненулевую мнимую часть. */
int naw_eval_expr(const char* expr, double* out);
double eval_expr(const char* expr);
int naw_eval_complex_expr(const char* expr, naw_complex_t* out);
int naw_double_to_text(double value, char* out, unsigned int out_size);
int naw_complex_to_text(naw_complex_t value, char* out, unsigned int out_size);
/* Аргументы — текстовые выражения; результат — статическая строка (кольцевой буфер). */
const char* naw_math_native_call(naw_math_native_id_t native_id, const char** args, int argc);

naw_complex_t naw_complex_make(double real, double imag);
naw_complex_t naw_complex_from_real(double real);
naw_polar_t naw_complex_to_polar(naw_complex_t value);
naw_complex_t naw_complex_from_polar(naw_polar_t polar);
naw_complex_t naw_complex_add(naw_complex_t a, naw_complex_t b);
naw_complex_t naw_complex_sub(naw_complex_t a, naw_complex_t b);
naw_complex_t naw_complex_mul(naw_complex_t a, naw_complex_t b);
naw_complex_t naw_complex_div(naw_complex_t a, naw_complex_t b);
naw_complex_t naw_complex_neg(naw_complex_t value);
naw_complex_t naw_complex_scale(naw_complex_t value, double factor);
naw_complex_t naw_complex_divide_scalar(naw_complex_t value, double factor);
double naw_complex_abs(naw_complex_t value);
double naw_complex_arg(naw_complex_t value);
naw_complex_t naw_complex_conjugate(naw_complex_t value);
naw_complex_t naw_complex_normalize(naw_complex_t value);
naw_complex_t naw_complex_pow_real(naw_complex_t value, double power);
naw_complex_t naw_complex_sqrt(naw_complex_t value);
naw_complex_t naw_complex_sin(naw_complex_t value);
naw_complex_t naw_complex_cos(naw_complex_t value);
naw_quadratic_status_t naw_complex_solve_quadratic(
    naw_complex_t a,
    naw_complex_t b,
    naw_complex_t c,
    naw_quadratic_result_t* out);

#endif
