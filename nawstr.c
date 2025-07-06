// string.c
#include "nawstring.h"

char* strncpy(char* dest, const char* src, unsigned int n) {
    unsigned int i;
    for (i = 0; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

unsigned int strlen(const char* str) {
    unsigned int len = 0;
    while (str[len]) len++;
    return len;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n) return (char*)haystack;
    }
    return 0;
}

void itoa(int value, char* str) {
    char* p = str;
    char* p1, *p2;
    int digits = 0;

    if (value == 0) {
        *p++ = '0';
        *p = '\0';
        return;
    }

    if (value < 0) {
        *p++ = '-';
        value = -value;
    }

    p1 = p;

    while (value) {
        *p++ = '0' + (value % 10);
        value /= 10;
        digits++;
    }
    *p = '\0';

    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
}
