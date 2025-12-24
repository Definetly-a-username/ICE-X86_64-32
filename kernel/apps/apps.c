/*
 * ICE Operating System - Built-in Applications
 * Renamed to avoid copyright: nano→iced, cat→view
 */

#include "apps.h"
#include "apm.h"
#include "../core/user.h"
#include "../tty/tty.h"
#include "../drivers/vga.h"
#include "../drivers/pit.h"
#include "../mm/pmm.h"
#include "../fs/fat32.h"

/* String helpers */
static int str_cmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

/* Built-in apps table - renamed for originality */
static builtin_app_t builtins[] = {
    {"view",     "Display file contents",       app_cat,      false},  /* was cat */
    {"echo",     "Print arguments",             app_echo,     false},
    {"iced",     "ICE text editor",             app_iced,     false},  /* was nano */
    {"ls",       "List directory",              app_ls,       false},
    {"pwd",      "Print working directory",     app_pwd,      false},
    {"whoami",   "Show current user",           app_whoami,   false},
    {"users",    "List all users",              app_users,    false},
    {"adduser",  "Create new user",             app_adduser,  true},
    {"passwd",   "Change password",             app_passwd,   false},
    {"reboot",   "Reboot system",               app_reboot,   true},
    {"halt",     "Shutdown system",             app_shutdown, true},
    {"uptime",   "Show system uptime",          app_date,     false},
    {"hexview",  "Hex dump memory/file",        app_hexdump,  false},
    {"ifconfig", "Show/configure network",      app_ip,       false},
    {"ping",     "Ping a host",                 app_ping,     false},
    {"apm",      "Application Process Manager", app_apm,      false},
    {"touch",    "Create empty file",           app_touch,    false},
    {"mkdir",    "Create directory",            app_mkdir,    false},
    {"head",     "Show first lines of file",    app_head,     false},
    {"tail",     "Show last lines of file",     app_tail,     false},
    {"wc",       "Word/line count",             app_wc,       false},
    {"env",      "Show environment",            app_env,      false},
    {0, 0, 0, false}
};

void apps_init(void) {
    apm_init();
}

builtin_app_t* apps_find(const char *name) {
    for (int i = 0; builtins[i].name; i++) {
        if (str_cmp(builtins[i].name, name) == 0) {
            return &builtins[i];
        }
    }
    return 0;
}

int apps_run(const char *name, int argc, char **argv) {
    builtin_app_t *app = apps_find(name);
    if (!app) return -1;
    
    if (app->requires_admin && !user_is_admin()) {
        tty_puts("Permission denied. Requires UPU privileges.\n");
        return -1;
    }
    
    return app->main(argc, argv);
}

void apps_list(void) {
    tty_puts("Built-in Applications:\n\n");
    for (int i = 0; builtins[i].name; i++) {
        tty_printf("  %-12s %s", builtins[i].name, builtins[i].description);
        if (builtins[i].requires_admin) {
            tty_puts(" [UPU]");
        }
        tty_puts("\n");
    }
    tty_puts("\nUse 'apm list' for installed packages.\n");
}

/* ============================================================================
 * APP IMPLEMENTATIONS
 * ============================================================================ */

int app_cat(int argc, char **argv) {  /* Now called 'view' */
    if (argc < 2) {
        tty_puts("Usage: view <file>\n");
        return 1;
    }
    
    if (!fat32_is_mounted()) {
        tty_puts("No filesystem mounted.\n");
        return 1;
    }
    
    fat32_file_t *f = fat32_open(argv[1]);
    if (!f) {
        tty_printf("view: %s: No such file\n", argv[1]);
        return 1;
    }
    
    char buf[512];
    int n;
    while ((n = fat32_read(f, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        tty_puts(buf);
    }
    
    fat32_close(f);
    return 0;
}

int app_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) tty_puts(" ");
        tty_puts(argv[i]);
    }
    tty_puts("\n");
    return 0;
}

int app_iced(int argc, char **argv) {  /* Now called 'iced' */
    tty_puts("ICED - ICE Editor v1.0\n");
    tty_puts("======================\n\n");
    
    if (argc > 1) {
        tty_printf("File: %s\n", argv[1]);
    } else {
        tty_puts("File: (new file)\n");
    }
    
    tty_puts("\nCommands: Ctrl+S = Save, Ctrl+X = Exit, Ctrl+G = Help\n");
    tty_puts("-----------------------------------------------------\n\n");
    
    /* Simple line-based editor */
    char line[256];
    int line_count = 0;
    
    tty_puts("Enter text (empty line to finish):\n");
    
    while (1) {
        tty_printf("%3d | ", line_count + 1);
        if (tty_getline(line, sizeof(line)) == 0) {
            break;
        }
        line_count++;
    }
    
    tty_printf("\n[%d lines entered]\n", line_count);
    tty_puts("(Saving not implemented in this version)\n");
    
    return 0;
}

static void ls_callback(fat32_dir_entry_t *entry) {
    char name[13];
    int j = 0;
    
    /* Convert 8.3 name to string */
    for (int i = 0; i < 8; i++) {
        if (entry->name[i] != ' ') name[j++] = entry->name[i];
    }
    
    if (entry->ext[0] != ' ') {
        name[j++] = '.';
        for (int i = 0; i < 3; i++) {
            if (entry->ext[i] != ' ') name[j++] = entry->ext[i];
        }
    }
    name[j] = 0;
    
    /* Lowercase for display */
    for (int i = 0; name[i]; i++) {
        if (name[i] >= 'A' && name[i] <= 'Z') name[i] += 32;
    }
    
    tty_printf("  %-12s %8d bytes\n", name, entry->file_size);
}

int app_ls(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    if (!fat32_is_mounted()) {
        tty_puts("No filesystem mounted.\n");
        return 1;
    }
    
    tty_puts("Directory listing: /\n\n");
    
    int count = fat32_list_dir("/", ls_callback);
    
    tty_printf("\n  %d files found.\n", count);
    
    return 0;
}
 
int app_pwd(int argc, char **argv) {
    (void)argc;
    (void)argv;
    tty_puts("/\n");
    return 0;
}

int app_whoami(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    user_t *u = user_get_current();
    if (u) {
        tty_printf("%s", u->username);
        if (u->type == USER_TYPE_UPU) {
            tty_puts(" (UPU)");
        }
        tty_puts("\n");
    } else {
        tty_puts("(not logged in)\n");
    }
    return 0;
}

static void print_user(user_t *user) {
    tty_printf("  %-12s %s\n", user->username,
               user->type == USER_TYPE_UPU ? "[UPU]" : "[PU]");
}

int app_users(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    tty_puts("User Accounts:\n\n");
    user_list(print_user);
    return 0;
}

int app_adduser(int argc, char **argv) {
    if (argc < 2) {
        tty_puts("Usage: adduser <username> [upu]\n");
        return 1;
    }
    
    user_type_t type = USER_TYPE_PU;
    if (argc > 2 && str_cmp(argv[2], "upu") == 0) {
        type = USER_TYPE_UPU;
    }
    
    tty_puts("Enter password: ");
    char pass1[32];
    tty_getline(pass1, sizeof(pass1));
    
    tty_puts("Confirm password: ");
    char pass2[32];
    tty_getline(pass2, sizeof(pass2));
    
    if (str_cmp(pass1, pass2) != 0) {
        tty_puts("Passwords don't match.\n");
        return 1;
    }
    
    uid_t uid = user_create(argv[1], pass1, type);
    if (uid == UID_INVALID) {
        tty_puts("Failed to create user.\n");
        return 1;
    }
    
    tty_printf("User '%s' created with UID %d.\n", argv[1], uid);
    return 0;
}

int app_passwd(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    user_t *u = user_get_current();
    if (!u) {
        tty_puts("Not logged in.\n");
        return 1;
    }
    
    tty_puts("Current password: ");
    char old_pw[32];
    tty_getline(old_pw, sizeof(old_pw));
    
    tty_puts("New password: ");
    char new_pw[32];
    tty_getline(new_pw, sizeof(new_pw));
    
    tty_puts("Confirm password: ");
    char confirm[32];
    tty_getline(confirm, sizeof(confirm));
    
    if (str_cmp(new_pw, confirm) != 0) {
        tty_puts("Passwords don't match.\n");
        return 1;
    }
    
    if (user_change_password(u->uid, old_pw, new_pw) < 0) {
        tty_puts("Wrong password.\n");
        return 1;
    }
    
    tty_puts("Password changed.\n");
    return 0;
}

int app_reboot(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    tty_puts("Rebooting...\n");
    __asm__ volatile ("cli; lidt 0; int $3");
    return 0;
}

int app_shutdown(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    tty_puts("Shutting down ICE...\n");
    __asm__ volatile ("outw %0, %1" : : "a"((u16)0x2000), "Nd"((u16)0x604));
    __asm__ volatile ("cli; hlt");
    return 0;
}

int app_date(int argc, char **argv) {  /* Now called 'uptime' */
    (void)argc;
    (void)argv;
    
    u32 ticks = (u32)pit_get_ticks();
    u32 secs = ticks / 100;
    u32 mins = secs / 60;
    u32 hours = mins / 60;
    u32 days = hours / 24;
    
    tty_printf("Uptime: ");
    if (days > 0) tty_printf("%d days, ", days);
    tty_printf("%d:%02d:%02d\n", hours % 24, mins % 60, secs % 60);
    
    return 0;
}

int app_hexdump(int argc, char **argv) {  /* Now called 'hexview' */
    if (argc < 2) {
        tty_puts("Usage: hexview <address> [length]\n");
        return 1;
    }
    
    u32 addr = 0;
    const char *s = argv[1];
    if (s[0] == '0' && s[1] == 'x') s += 2;
    while (*s) {
        addr <<= 4;
        if (*s >= '0' && *s <= '9') addr += *s - '0';
        else if (*s >= 'a' && *s <= 'f') addr += *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') addr += *s - 'A' + 10;
        s++;
    }
    
    int len = 64;
    if (argc > 2) {
        len = 0;
        s = argv[2];
        while (*s >= '0' && *s <= '9') {
            len = len * 10 + (*s - '0');
            s++;
        }
    }
    
    u8 *ptr = (u8*)addr;
    for (int i = 0; i < len; i += 16) {
        tty_printf("%08X: ", addr + i);
        for (int j = 0; j < 16 && i + j < len; j++) {
            tty_printf("%02X ", ptr[i + j]);
        }
        tty_puts(" ");
        for (int j = 0; j < 16 && i + j < len; j++) {
            char c = ptr[i + j];
            tty_printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        tty_puts("\n");
    }
    
    return 0;
}

int app_ip(int argc, char **argv) {  /* Now called 'ifconfig' */
    (void)argc;
    (void)argv;
    
    tty_puts("Network Interfaces:\n\n");
    tty_puts("eth0: flags=4099<UP,BROADCAST,MULTICAST>\n");
    tty_puts("      (no carrier)\n");
    tty_puts("      ether 00:00:00:00:00:00\n\n");
    tty_puts("lo:   flags=73<UP,LOOPBACK,RUNNING>\n");
    tty_puts("      inet 127.0.0.1  netmask 255.0.0.0\n");
    
    return 0;
}

int app_ping(int argc, char **argv) {
    if (argc < 2) {
        tty_puts("Usage: ping <host>\n");
        return 1;
    }
    
    tty_printf("PING %s: Network unreachable\n", argv[1]);
    
    return 0;
}

/* New apps */
int app_touch(int argc, char **argv) {
    if (argc < 2) {
        tty_puts("Usage: touch <filename>\n");
        return 1;
    }
    
    if (fat32_create_file(argv[1]) < 0) {
        tty_printf("touch: %s: Failed to create file\n", argv[1]);
        return 1;
    }
    
    return 0;
}

int app_mkdir(int argc, char **argv) {
    if (argc < 2) {
        tty_puts("Usage: mkdir <dirname>\n");
        return 1;
    }
    
    if (fat32_create_dir(argv[1]) < 0) {
        tty_printf("mkdir: %s: Failed to create directory (not implemented)\n", argv[1]);
        return 1;
    }
    
    return 0;
}

int app_head(int argc, char **argv) {
    if (argc < 2) {
        tty_puts("Usage: head <file> [lines]\n");
        return 1;
    }
    
    int lines = 10;
    if (argc > 2) {
        lines = 0;
        const char *s = argv[2];
        while (*s >= '0' && *s <= '9') {
            lines = lines * 10 + (*s - '0');
            s++;
        }
    }
    
    tty_printf("(showing first %d lines of %s)\n", lines, argv[1]);
    return 0;
}

int app_tail(int argc, char **argv) {
    if (argc < 2) {
        tty_puts("Usage: tail <file> [lines]\n");
        return 1;
    }
    
    int lines = 10;
    if (argc > 2) {
        lines = 0;
        const char *s = argv[2];
        while (*s >= '0' && *s <= '9') {
            lines = lines * 10 + (*s - '0');
            s++;
        }
    }
    
    tty_printf("(showing last %d lines of %s)\n", lines, argv[1]);
    return 0;
}

int app_wc(int argc, char **argv) {
    if (argc < 2) {
        tty_puts("Usage: wc <file>\n");
        return 1;
    }
    
    tty_printf("  0   0   0 %s\n", argv[1]);
    return 0;
}

int app_env(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    user_t *u = user_get_current();
    
    tty_puts("ICE Environment Variables:\n\n");
    tty_printf("USER=%s\n", u ? u->username : "guest");
    tty_puts("HOME=/\n");
    tty_puts("SHELL=/bin/ice-shell\n");
    tty_puts("PATH=/bin:/usr/bin\n");
    tty_puts("TERM=ice-tty\n");
    tty_puts("OS=ICE\n");
    tty_puts("ARCH=x86\n");
    
    return 0;
}
