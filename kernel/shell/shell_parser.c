#include "kernel/shell/shell_parser.h"
#include "kernel/memory/memory.h"
#include "lib/nawstring.h"
#include "lib/nawutil.h"

int shell_parse_filename(const char* filename, char* name, char* ext) {
    const char* dot = naw_find_char(filename, '.');
    size_t name_len;

    if (!dot || dot <= filename) {
        return 0;
    }

    /* Имена NawFS: до 8 символов имени и 3 расширения (как в каталоге на диске). */
    name_len = (size_t)(dot - filename);
    if (name_len > 8) {
        name_len = 8;
    }

    memset(name, 0, 9);
    memset(ext, 0, 4);
    memcpy(name, filename, name_len);
    strncpy(ext, dot + 1, 3);
    ext[3] = '\0';

    return strlen(name) > 0 && strlen(ext) > 0;
}

int shell_parse_write_request(const char* input, char* text, int text_size, char* file, int file_size) {
    const char* sep = strstr(input, " - ");
    size_t text_len;

    if (!sep) {
        return 0;
    }

    text_len = (size_t)(sep - input);
    if (text_len >= (size_t)text_size) {
        text_len = (size_t)text_size - 1;
    }

    memset(text, 0, (size_t)text_size);
    memset(file, 0, (size_t)file_size);
    memcpy(text, input, text_len);
    strncpy(file, sep + 3, (unsigned int)(file_size - 1));
    file[file_size - 1] = '\0';
    return 1;
}
