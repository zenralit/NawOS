#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include "bytecode.h"
#include "fs/nawfs.h"
#include "drivers/screen/screen.h"
#include "drivers/keyboard/keyboard.h"
#include "lib/nawstring.h"

void nawlang_run(const char* filename) {
    char name[9] = {0};
    char ext[4]  = {0};

    char* dot = find_char(filename, '.');
    if (!dot) {
        print("Bad filename\n");
        return;
    }

    int len = dot - filename;
    if (len > 8) len = 8;
    strncpy(name, filename, len);
    strncpy(ext, dot + 1, 3);

    const char* src = fs_read(name, ext);
    if (!src) {
        print("File not found\n");
        return;
    }

    Instruction code[MAX_CODE];
    naw_compile(src, code);
    vm_run(code);
}
