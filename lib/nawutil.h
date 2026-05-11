#ifndef LIB_NAWUTIL_H
#define LIB_NAWUTIL_H

#include <stdint.h>

int naw_atoi(const char* str);
char* naw_find_char(const char* str, char ch);
void naw_uint8_to_hex(uint8_t val, char* out);

#endif
