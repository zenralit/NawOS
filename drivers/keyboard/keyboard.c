#include "keyboard.h"
#include "drivers/screen/screen.h"
#include "drivers/ports/ports.h"
#include "fs/nawfs.h"
#include "drivers/disk/disk.h"
#include "lib/math.h"
#include <stddef.h>
#include "drivers/net/net.h"
#include "drivers/net/ip.h"
#include "nawlang/parser.h"

#define INPUT_BUFFER_SIZE 256
#define MAX_INPUT 128
#define PORT_KBD_DATA 0x60
#define EDITOR_BUFFER_SIZE 512
#define EDITOR_TEXT_START_ROW 3
#define EXTENDED_SCANCODE_PREFIX 0xE0
#define EXTENDED_KEY_UP 0x48
#define EXTENDED_KEY_DOWN 0x50
#define EXTENDED_KEY_LEFT 0x4B
#define EXTENDED_KEY_RIGHT 0x4D

char input_buffer[INPUT_BUFFER_SIZE];
int input_pos = 0;
uint8_t key_pressed[128] = {0};
int shift_pressed = 0;
uint8_t key_down[256] = {0};
static uint8_t extended_key_down[128] = {0};
static uint8_t extended_scancode_prefix = 0;

// -------- keyboard map -------- //
static const char scancode_map[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
    'z','x','c','v','b','n','m',',','.','/', 0, '*',
    0, ' ', 0,
};
static const char scancode_shift_map[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,
    'A','S','D','F','G','H','J','K','L',':','"','~', 0, '|',
    'Z','X','C','V','B','N','M','<','>','?', 0, '*',
    0, ' ', 0,
};
// -------- keyboard map -------- //

static void print_ipv4_value(const uint8_t ip[4]) {
    for (int i = 0; i < 4; i++) {
        print_dec(ip[i]);
        if (i < 3) {
            print(".");
        }
    }
}

static void reset_keyboard_state() {
    for (int i = 0; i < 256; i++) {
        key_down[i] = 0;
    }

    for (int i = 0; i < 128; i++) {
        extended_key_down[i] = 0;
    }

    shift_pressed = 0;
    extended_scancode_prefix = 0;
}

static void editor_step_visual_position(char c, int* row, int* col) {
    if (c == '\n') {
        (*row)++;
        *col = 0;
        return;
    }

    (*col)++;
    if (*col >= SCREEN_COLS) {
        (*row)++;
        *col = 0;
    }
}

static void editor_get_visual_position(const char* buffer, int cursor, int* row, int* col) {
    *row = EDITOR_TEXT_START_ROW;
    *col = 0;

    for (int i = 0; i < cursor; i++) {
        editor_step_visual_position(buffer[i], row, col);
    }
}

static int editor_index_from_visual_position(const char* buffer, int len, int target_row, int target_col) {
    int row = EDITOR_TEXT_START_ROW;
    int col = 0;
    int best_index = -1;

    for (int i = 0; i <= len; i++) {
        if (row == target_row) {
            best_index = i;
            if (col >= target_col) {
                return i;
            }
        } else if (row > target_row) {
            break;
        }

        if (i == len) {
            break;
        }

        editor_step_visual_position(buffer[i], &row, &col);
    }

    return best_index;
}

static int editor_insert_char(char* buffer, int* len, int* cursor, char c) {
    if (*len >= EDITOR_BUFFER_SIZE - 1) {
        return 0;
    }

    for (int i = *len; i >= *cursor; i--) {
        buffer[i + 1] = buffer[i];
    }

    buffer[*cursor] = c;
    (*len)++;
    (*cursor)++;
    return 1;
}

static int editor_delete_char(char* buffer, int* len, int* cursor) {
    if (*cursor <= 0) {
        return 0;
    }

    for (int i = *cursor - 1; i < *len; i++) {
        buffer[i] = buffer[i + 1];
    }

    (*cursor)--;
    (*len)--;
    return 1;
}

static void editor_render(const char* name, const char* ext, const char* buffer, int len, int cursor, const char* status_message) {
    int cursor_row;
    int cursor_col;

    screen_begin_batch();
    clear_screen();
    print("Editing: ");
    print(name);
    print(".");
    print(ext);
    print("\n");
    print("F2 = save, ESC = exit\n");
    if (status_message && status_message[0]) {
        print(status_message);
    }
    print("\n");

    for (int i = 0; i < len; i++) {
        put_char(buffer[i]);
    }

    editor_get_visual_position(buffer, cursor, &cursor_row, &cursor_col);
    screen_set_cursor_absolute(cursor_row, cursor_col);
    screen_end_batch();
}


void process_command(const char* input) {
    if (strcmp(input, "help") == 0) {
        print(" commands:\n");
        print("help - Show this message\n");
        print("clear - Clear screen\n");
        print("reboot - Reboot system\n");
        print("echo <text> - Print text to the screen\n");
        print("ls - Show files\n");
        print("cf <name.ext> - Create file\n");
        print("wr <text> - <name.ext> - Write to file\n");
        print("cat <name.ext> - Show file content\n");
        print("rm <name.ext> - Delete file\n");
        print("readsec <num> - Read disk sector\n");
        print("edit <name.ext> - text editor\n");
        print("calc <math exp> - solve a mathematical expression\n");
        print("dhcp - Request a DHCP lease\n");
        print("ipconfig - Show network configuration\n");
        print("netmsg <text> - Broadcast UDP text\n");
    } else if(strcmp(input, "")==0){
        print("\n");
    }
     else if (strncmp(input, "run ", 4) == 0) {
    const char* filename = input + 4;
    if (*filename == '\0') {
        print("Usage: run <filename>\n");
    } else {
        nawlang_run(filename);
    }
    }
    else if (strcmp(input, "clear") == 0) {
        clear_screen();
    } else if (strcmp(input, "reboot") == 0) {
        reboot();
    } else if (strncmp(input, "echo ", 5) == 0) {
        print(input + 5);
        print("\n");
    } else if (strcmp(input, "ls") == 0) {
        fs_list();
    } else if (strcmp(input, "swaga") == 0){
        print("swaga prisutstvuet");
    } else if (strncmp(input, "cf ", 3) == 0) {
    const char* nameext = input + 3;
    const char* dot = find_char(nameext, '.');
    if (dot && dot > nameext) {
        char name[9] = {0};
        char ext[4] = {0};
        size_t namelen = dot - nameext;
        if (namelen > 8) namelen = 8;
            memcpy(name, nameext, namelen);
            strncpy(ext, dot + 1, 3);
            ext[3] = '\0';

        if (strlen(ext) == 0 || strlen(name) == 0) {
            print("Invalid filename. Use name.ext\n");
        } else if (fs_create(name, ext) == 0) {
            print("File created\n");
        } else {
            print("Failed to create file\n");
        }
    } else {
        print("Invalid filename. Use name.ext\n");
    }
   } else if (strncmp(input, "wr ", 3) == 0) {
    const char* sep = strstr(input + 3, " - ");
    if (sep) {
        char text[128] = {0};
        char file[16] = {0};
        size_t textlen = sep - (input + 3);
        if (textlen > 127) textlen = 127;
        memcpy(text, input + 3, textlen);
        strncpy(file, sep + 3, 15);
        file[15] = '\0';

        const char* dot = find_char(file, '.');

        if (dot && dot > file) {
            char name[9] = {0};
            char ext[4] = {0};
        size_t namelen = dot - file;
            if (namelen > 8) namelen = 8;
                memcpy(name, file, namelen);
                strncpy(ext, dot + 1, 3);
                ext[3] = '\0';

            if (strlen(ext) == 0 || strlen(name) == 0) {
                print("Invalid filename. Use name.ext\n");
            } else if (fs_write(name, ext, text) == 0) {
                print("File written\n");
            } else {
                print("Failed to write file\n");
            }
        } else {
            print("Invalid filename. Use name.ext\n");
        }
    } else {
        print("Usage: wr <text> - <file>\n");
    }
    } else if (strncmp(input, "cat ", 4) == 0) {
        const char* file = input + 4;
        const char* dot = find_char(file, '.');

    if (dot && dot > file) {
        char name[9] = { 0 };
        char ext[4] = { 0 };

    size_t namelen = dot - file;
        if (namelen > 8) namelen = 8;

        for (size_t i = 0; i < namelen; ++i) {
            name[i] = file[i];
        }

    const char* ext_start = dot + 1;
        size_t extlen = strlen(ext_start);
        if (extlen > 3) extlen = 3;
        for (size_t i = 0; i < extlen; ++i) {
            ext[i] = ext_start[i];
        }

        const char* data = fs_read(name, ext);
        if (data) {
            print(data);
            print("\n");
        } else {
            print("File not found\n");
        }
    } else {
        print("Invalid filename. Use name.ext\n");
    }
} else if (strncmp(input, "rm ", 3) == 0) {
    const char* file = input + 3;
    const char* dot = find_char(file, '.');
    if (dot && dot > file) {
        char name[9] = {0};
        char ext[4] = {0};
        size_t namelen = dot - file;
        if (namelen > 8) namelen = 8;
        memcpy(name, file, namelen);
        strncpy(ext, dot + 1, 3);
        ext[3] = '\0';

        if (strlen(ext) == 0 || strlen(name) == 0) {
            print("Invalid filename. Use name.ext\n");
        } else if (fs_delete(name, ext) == 0) {
            print("File deleted\n");
        } else {
            print("Failed to delete file\n");
        }
    } else {
        print("Invalid filename. Use name.ext\n");
    }
    } else if (strncmp(input, "readsec ", 8) == 0) {
        int sec = atoi(input + 8);
        uint8_t buffer[512];
        if (disk_read_sector(sec, buffer) == 0) {
            for (int i = 0; i < 512; i++) {
                char hex[3];
                uint8_to_hex(buffer[i], hex);
                print(hex);
                print(" ");
                if ((i + 1) % 16 == 0) print("\n");
            }
        } else {
            print("Disk read error\n");
        }
    }else if (strncmp(input, "edit ", 5) == 0) {
    const char* file = input + 5;
    const char* dot = find_char(file, '.');
    if (dot && dot > file) {
        char name[9] = {0};
        char ext[4] = {0};
        size_t namelen = dot - file;
        if (namelen > 8) namelen = 8;
        memcpy(name, file, namelen);
        strncpy(ext, dot + 1, 3);
        ext[3] = '\0';

        start_text_editor(name, ext);
    } else {
        print("Invalid filename. Use name.ext\n");
    }
    }
    else if (strncmp(input, "calc", 4) == 0) {
    if (strncmp(input, "calc", 4) == 0) {
        const char* expr = input + 4;
        while (*expr == ' ') expr++;
        if (*expr == '\0') {
            print("calculator is not implemented yet.\n");
            // TODO need added full calculator
            // tyt budet calc
        } else {         
            double result = eval_expr(expr);
            print(expr); print(" = ");
            print_double(result); 
            print("\n");
        }
    }
    // S W A G A - swaga,  уменя её навалом 
    } else if (strncmp(input, "lelya", 5) == 0) {

        uint8_t key_down[128] = {0};  

        while (1) { 
            uint16_t sc = get_scancode();

            if (sc == 0) continue;

            if (sc == 0xE0) {
                uint8_t next = get_scancode();
                uint16_t full = (0xE000 | next);
                print_hex(full);
                print("\n");
                if (next == 0x01) break; 
                continue;
            }

            if (sc & 0x80) {
                uint8_t key = sc & 0x7F;
                key_down[key] = 0;
                continue;
            }

            if (key_down[sc]) continue; 
            key_down[sc] = 1;

            print_hex(sc);
            print("\n");

            if (sc == 1) break; 
        }
        }
        else if (strcmp(input, "dhcp") == 0) {
        net_request_dhcp();
    } else if (strncmp(input, "netmsg ", 7) == 0) {
        if (input[7] == '\0') {
            print("Usage: netmsg <text>\n");
        } else {
            net_send_text_broadcast(input + 7);
            print("UDP broadcast sent\n");
        }
    } else if (strcmp(input, "ipconfig") == 0) {
        print("MAC: ");
        for (int i = 0; i < 6; i++) {
            print_hex(net_info.mac[i]);
            if (i < 5) print(":");
        }
        print("\nIP Address: ");
        print_ipv4_value(naw_ip_address);
        print("\nGateway: ");
        print_ipv4_value(net_info.gateway);
        print("\nSubnet: ");
        print_ipv4_value(net_info.subnet);
        print("\nDHCP Server: ");
        print_ipv4_value(net_info.dhcp_server);
        print("\nStatus: ");
        print(net_is_configured() ? "configured" : "pending");
        print("\n");
    } else {
        print("Unknown command. Type 'help' for help.\n");
    }
    print("\n> ");
    input_pos = 0;
    input_buffer[0] = 0;
}
// --------- функции --------- //

void handle_input(char c) {
    if (c == '\b' && input_pos > 0) {
        input_pos--;
        input_buffer[input_pos] = 0;
        print_backspace();
    } else if (c == '\n') {
        print("\n");
        input_buffer[input_pos] = 0;
        process_command(input_buffer);
    } else if (input_pos < INPUT_BUFFER_SIZE - 1) {
        input_buffer[input_pos++] = c;
        char str[2] = {c, 0};
        print(str);
    }
}

void keyboard_handle_scancode(uint16_t sc) {
    if (sc == 0) return;

    if (sc > 0xFF) {
        uint8_t ext = sc & 0xFF;
        uint8_t key = ext & 0x7F;

        if (ext & 0x80) {
            extended_key_down[key] = 0;
            return;
        }

        if (extended_key_down[key]) {
            return;
        }
        extended_key_down[key] = 1;

        if (ext == EXTENDED_KEY_UP) {
            screen_scroll_page_up();
        } else if (ext == EXTENDED_KEY_DOWN) {
            screen_scroll_page_down();
        }
        return;
    }

    if (sc == 42 || sc == 54) {
        shift_pressed = 1;
        return;
    } else if (sc == (42 | 0x80) || sc == (54 | 0x80)) {
        shift_pressed = 0;
        return;
    }

    
    if (sc & 0x80) {
        key_down[sc & 0x7F] = 0;
        return;
    }

    if (key_down[sc] == 0) {
        key_down[sc] = 1;
        uint8_t base = sc & 0x7F;
        char c = shift_pressed ? scancode_shift_map[base] : scancode_map[base];

        if (c) {
            handle_input(c);
        }
    }
}


void keyboard_init() {
    uint8_t mask = inb(0x21);
    outb(0x21, mask & ~0x02);
}

char* find_char(const char* str, char ch) {
    while (*str) {
        if (*str == ch) return (char*)str;
        str++;
    }
    return NULL;
}


void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    while (n--) *d++ = *s++;
    return dest;
}

void uint8_to_hex(uint8_t val, char* out) {
    const char* hex = "0123456789ABCDEF";
    out[0] = hex[(val >> 4) & 0xF];
    out[1] = hex[val & 0xF];
    out[2] = 0;
}

int atoi(const char* str) {
    int res = 0;
    while (*str >= '0' && *str <= '9') {
        res = res * 10 + (*str - '0');
        str++;
    }
  return res;
}

void keyboard_handle_interrupt() {  
    uint8_t scancode = port_byte_in(0x60);
    uint16_t full_scancode;

    if (scancode == EXTENDED_SCANCODE_PREFIX) {
        extended_scancode_prefix = 1;
        port_byte_out(0x20, 0x20);
        return;
    }

    if (extended_scancode_prefix) {
        full_scancode = 0xE000 | scancode;
        extended_scancode_prefix = 0;
    } else {
        full_scancode = scancode;
    }

    keyboard_handle_scancode(full_scancode);

    port_byte_out(0x20, 0x20); 
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++; s2++;
    }
  return *(unsigned char*)s1 - *(unsigned char*)s2;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        if ((*haystack == *needle) && (strncmp(haystack, needle, strlen(needle)) == 0)) {
            return (char*)haystack;
        }
    }
  return NULL;
}

void reboot() {
    uint8_t good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);
}

int strncmp(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i]) {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
        if (s1[i] == '\0') {
            return 0;
        }
    }
  return 0;
}

void start_text_editor(const char* name, const char* ext) {
    char buffer[EDITOR_BUFFER_SIZE] = {0};
    const char* status_message = "";
    int len = 0;
    int cursor = 0;
    int loaded;
    uint8_t local_key_down[128] = {0};
    uint8_t local_extended_key_down[128] = {0};
    int local_shift_pressed = 0;

    loaded = fs_read_to_buffer(name, ext, buffer, sizeof(buffer));
    if (loaded >= 0) {
        len = loaded;
        cursor = len;
    } else {
        status_message = "New file";
    }

    reset_keyboard_state();
    editor_render(name, ext, buffer, len, cursor, status_message);

    while (1) {
        uint16_t full_sc = get_scancode();
        uint8_t sc;

        if (full_sc == 0) {
            continue;
        }

        if (full_sc > 0xFF) {
            uint8_t ext_sc = full_sc & 0xFF;
            uint8_t key = ext_sc & 0x7F;

            if (ext_sc & 0x80) {
                local_extended_key_down[key] = 0;
                continue;
            }

            if (local_extended_key_down[key]) {
                continue;
            }
            local_extended_key_down[key] = 1;

            if (ext_sc == EXTENDED_KEY_LEFT && cursor > 0) {
                cursor--;
            } else if (ext_sc == EXTENDED_KEY_RIGHT && cursor < len) {
                cursor++;
            } else if (ext_sc == EXTENDED_KEY_UP) {
                int row;
                int col;
                int next_cursor;

                editor_get_visual_position(buffer, cursor, &row, &col);
                next_cursor = editor_index_from_visual_position(buffer, len, row - 1, col);
                if (next_cursor >= 0) {
                    cursor = next_cursor;
                }
            } else if (ext_sc == EXTENDED_KEY_DOWN) {
                int row;
                int col;
                int next_cursor;

                editor_get_visual_position(buffer, cursor, &row, &col);
                next_cursor = editor_index_from_visual_position(buffer, len, row + 1, col);
                if (next_cursor >= 0) {
                    cursor = next_cursor;
                }
            }

            editor_render(name, ext, buffer, len, cursor, status_message);
            continue;
        }

        sc = full_sc & 0xFF;

        if (sc & 0x80) {
            uint8_t key = sc & 0x7F;
            if (key < 128) {
                local_key_down[key] = 0;
            }

            if (key == 42 || key == 54) {
                local_shift_pressed = 0;
            }
            continue;
        }

        if (sc == 42 || sc == 54) {
            local_shift_pressed = 1;
            local_key_down[sc] = 1;
            continue;
        }

        if (sc < 128 && local_key_down[sc]) {
            continue;
        }
        if (sc < 128) {
            local_key_down[sc] = 1;
        }

        if (sc == 60) {
            if (fs_write(name, ext, buffer) == 0) {
                status_message = "File saved";
            } else {
                status_message = "Save failed";
            }
            editor_render(name, ext, buffer, len, cursor, status_message);
            continue;
        }

        if (sc == 1) {
            break;
        }

        if (sc == 28) {
            if (editor_insert_char(buffer, &len, &cursor, '\n')) {
                status_message = "";
            } else {
                status_message = "Buffer full";
            }
            editor_render(name, ext, buffer, len, cursor, status_message);
            continue;
        }

        {
            char c = local_shift_pressed ? scancode_shift_map[sc] : scancode_map[sc];

            if (!c) {
                continue;
            }

            if (c == '\b') {
                editor_delete_char(buffer, &len, &cursor);
                status_message = "";
                editor_render(name, ext, buffer, len, cursor, status_message);
                continue;
            }

            if (c == '\t') {
                int inserted = 0;

                while (inserted < 4 && editor_insert_char(buffer, &len, &cursor, ' ')) {
                    inserted++;
                }

                status_message = (inserted == 4) ? "" : "Buffer full";
                editor_render(name, ext, buffer, len, cursor, status_message);
                continue;
            }

            if (c >= 32 && c <= 126) {
                if (editor_insert_char(buffer, &len, &cursor, c)) {
                    status_message = "";
                } else {
                    status_message = "Buffer full";
                }
                editor_render(name, ext, buffer, len, cursor, status_message);
            }
        }
    }

    reset_keyboard_state();
    print("\nExited editor\n");
}
