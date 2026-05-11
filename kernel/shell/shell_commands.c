#include "kernel/shell/shell_commands.h"
#include "apps/calc/calc.h"
#include "apps/editor/editor.h"
#include "apps/nawlang/parser.h"
#include "drivers/ata/ata.h"
#include "drivers/ports/ports.h"
#include "fs/nawfs.h"
#include "kernel/input/keyboard_driver.h"
#include "kernel/shell/shell_parser.h"
#include "kernel/terminal/terminal.h"
#include "lib/nawstring.h"
#include "lib/nawutil.h"
#include "net/ip.h"
#include "net/net.h"

static void shell_print_ipv4(const uint8_t ip[4]) {
    for (int i = 0; i < 4; i++) {
        terminal_write_dec(ip[i]);
        if (i < 3) {
            terminal_write(".");
        }
    }
}

static void shell_reboot_system() {
    uint8_t good = 0x02;

    while (good & 0x02) {
        good = inb(0x64);
    }

    outb(0x64, 0xFE);
}

static void shell_dump_scancodes() {
    uint8_t local_key_down[128] = {0};

    while (1) {
        uint16_t sc = keyboard_driver_read_scancode();

        if (sc == 0) {
            continue;
        }

        if (sc == 0xE0) {
            uint8_t next = (uint8_t)keyboard_driver_read_scancode();
            uint16_t full = (uint16_t)(0xE000 | next);

            terminal_write_hex(full);
            terminal_write("\n");
            if (next == 0x01) {
                break;
            }
            continue;
        }

        if (sc & 0x80) {
            uint8_t key = (uint8_t)(sc & 0x7F);
            local_key_down[key] = 0;
            continue;
        }

        if (local_key_down[sc]) {
            continue;
        }

        local_key_down[sc] = 1;
        terminal_write_hex(sc);
        terminal_write("\n");

        if (sc == 1) {
            break;
        }
    }
}

static void shell_print_help() {
    terminal_write(" commands:\n");
    terminal_write("help - Show this message\n");
    terminal_write("clear - Clear screen\n");
    terminal_write("reboot - Reboot system\n");
    terminal_write("echo <text> - Print text to the screen\n");
    terminal_write("ls - Show files\n");
    terminal_write("cf <name.ext> - Create file\n");
    terminal_write("wr <text> - <name.ext> - Write to file\n");
    terminal_write("cat <name.ext> - Show file content\n");
    terminal_write("rm <name.ext> - Delete file\n");
    terminal_write("readsec <num> - Read disk sector\n");
    terminal_write("edit <name.ext> - text editor\n");
    terminal_write("calc <math exp> - solve a mathematical expression\n");
    terminal_write("dhcp - Request a DHCP lease\n");
    terminal_write("ipconfig - Show network configuration\n");
    terminal_write("netmsg <text> - Broadcast UDP text\n");
}

void shell_execute_command(const char* input) {
    if (strcmp(input, "help") == 0) {
        shell_print_help();
    } else if (strcmp(input, "") == 0) {
        terminal_write("\n");
    } else if (strncmp(input, "run ", 4) == 0) {
        const char* filename = input + 4;

        if (*filename == '\0') {
            terminal_write("Usage: run <filename>\n");
        } else {
            nawlang_run(filename);
        }
    } else if (strcmp(input, "clear") == 0) {
        terminal_clear();
    } else if (strcmp(input, "reboot") == 0) {
        shell_reboot_system();
    } else if (strncmp(input, "echo ", 5) == 0) {
        terminal_write(input + 5);
        terminal_write("\n");
    } else if (strcmp(input, "ls") == 0) {
        fs_list();
    } else if (strcmp(input, "swaga") == 0) {
        terminal_write("swaga prisutstvuet");
    } else if (strncmp(input, "cf ", 3) == 0) {
        char name[9];
        char ext[4];

        if (!shell_parse_filename(input + 3, name, ext)) {
            terminal_write("Invalid filename. Use name.ext\n");
        } else if (fs_create(name, ext) == 0) {
            terminal_write("File created\n");
        } else {
            terminal_write("Failed to create file\n");
        }
    } else if (strncmp(input, "wr ", 3) == 0) {
        char text[128];
        char file[16];
        char name[9];
        char ext[4];

        if (!shell_parse_write_request(input + 3, text, sizeof(text), file, sizeof(file))) {
            terminal_write("Usage: wr <text> - <file>\n");
        } else if (!shell_parse_filename(file, name, ext)) {
            terminal_write("Invalid filename. Use name.ext\n");
        } else if (fs_write(name, ext, text) == 0) {
            terminal_write("File written\n");
        } else {
            terminal_write("Failed to write file\n");
        }
    } else if (strncmp(input, "cat ", 4) == 0) {
        char name[9];
        char ext[4];
        const char* data;

        if (!shell_parse_filename(input + 4, name, ext)) {
            terminal_write("Invalid filename. Use name.ext\n");
        } else {
            data = fs_read(name, ext);
            if (data) {
                terminal_write(data);
                terminal_write("\n");
            } else {
                terminal_write("File not found\n");
            }
        }
    } else if (strncmp(input, "rm ", 3) == 0) {
        char name[9];
        char ext[4];

        if (!shell_parse_filename(input + 3, name, ext)) {
            terminal_write("Invalid filename. Use name.ext\n");
        } else if (fs_delete(name, ext) == 0) {
            terminal_write("File deleted\n");
        } else {
            terminal_write("Failed to delete file\n");
        }
    } else if (strncmp(input, "readsec ", 8) == 0) {
        int sec = naw_atoi(input + 8);
        uint8_t buffer[512];

        if (disk_read_sector(sec, buffer) == 0) {
            for (int i = 0; i < 512; i++) {
                char hex[3];

                naw_uint8_to_hex(buffer[i], hex);
                terminal_write(hex);
                terminal_write(" ");
                if ((i + 1) % 16 == 0) {
                    terminal_write("\n");
                }
            }
        } else {
            terminal_write("Disk read error\n");
        }
    } else if (strncmp(input, "edit ", 5) == 0) {
        char name[9];
        char ext[4];

        if (!shell_parse_filename(input + 5, name, ext)) {
            terminal_write("Invalid filename. Use name.ext\n");
        } else {
            editor_start(name, ext);
        }
    } else if (strncmp(input, "calc", 4) == 0) {
        calc_run_expression(input + 4);
    } else if (strncmp(input, "lelya", 5) == 0) {
        shell_dump_scancodes();
    } else if (strcmp(input, "dhcp") == 0) {
        net_request_dhcp();
    } else if (strncmp(input, "netmsg ", 7) == 0) {
        if (input[7] == '\0') {
            terminal_write("Usage: netmsg <text>\n");
        } else {
            net_send_text_broadcast(input + 7);
            terminal_write("UDP broadcast sent\n");
        }
    } else if (strcmp(input, "ipconfig") == 0) {
        terminal_write("MAC: ");
        for (int i = 0; i < 6; i++) {
            terminal_write_hex(net_info.mac[i]);
            if (i < 5) {
                terminal_write(":");
            }
        }
        terminal_write("\nIP Address: ");
        shell_print_ipv4(naw_ip_address);
        terminal_write("\nGateway: ");
        shell_print_ipv4(net_info.gateway);
        terminal_write("\nSubnet: ");
        shell_print_ipv4(net_info.subnet);
        terminal_write("\nDHCP Server: ");
        shell_print_ipv4(net_info.dhcp_server);
        terminal_write("\nStatus: ");
        terminal_write(net_is_configured() ? "configured" : "pending");
        terminal_write("\n");
    } else {
        terminal_write("Unknown command. Type 'help' for help.\n");
    }
}
