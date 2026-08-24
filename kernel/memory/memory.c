/* memcpy/memset/memcmp — freestanding-замена libc для драйверов и сети. */
#include "kernel/memory/memory.h"

void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;

    while (n--) {
        *d++ = *s++;
    }

    return dest;
}

void* memset(void* dest, int val, size_t n) {
    unsigned char* d = dest;

    while (n--) {
        *d++ = (unsigned char)val;
    }

    return dest;
}

int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = s1;
    const unsigned char* p2 = s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }

    return 0;
}
