#ifndef KERNEL_SHELL_SHELL_PARSER_H
#define KERNEL_SHELL_SHELL_PARSER_H

int shell_parse_filename(const char* filename, char* name, char* ext);
int shell_parse_write_request(const char* input, char* text, int text_size, char* file, int file_size);

#endif
