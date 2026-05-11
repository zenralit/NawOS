#include "lib/nawutil.h"

int naw_atoi(const char* str) {
    int sign = 1;
    int result = 0;

    while (*str == ' ') {
        str++;
    }

    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }

    return sign * result;
}

char* naw_find_char(const char* str, char ch) {
    while (*str) {
        if (*str == ch) {
            return (char*)str;
        }
        str++;
    }

    return 0;
}

void naw_uint8_to_hex(uint8_t val, char* out) {
    const char* hex = "0123456789ABCDEF";

    out[0] = hex[(val >> 4) & 0x0F];
    out[1] = hex[val & 0x0F];
    out[2] = 0;
}
