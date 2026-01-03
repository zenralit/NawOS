
#ifndef NAWSTRING_H
#define NAWSTRING_H

char* strncpy(char* dest, const char* src, unsigned int n);
int strcmp(const char* s1, const char* s2);
unsigned int strlen(const char* str);
char* strstr(const char* haystack, const char* needle);
void itoa(int value, char* str);

#endif
