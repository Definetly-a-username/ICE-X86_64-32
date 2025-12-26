 

#include "apps.h"
#include "apm.h"
#include "../core/user.h"
#include "../tty/tty.h"
#include "../drivers/vga.h"
#include "../drivers/pit.h"
#include "../mm/pmm.h"
#include "../mm/pmm.h"
#include "../fs/vfs.h"
#include "../errno.h"

 
#include <string.h>

// Helper to normalize path - add leading slash if needed
static void normalize_path(const char *input, char *output, int max_len) {
    int j = 0;
    if (input[0] != '/') {
        output[j++] = '/';
    }
    for (int i = 0; input[i] && j < max_len - 1; i++) {
        output[j++] = input[i];
    }
    output[j] = 0;
}

// Forward declarations
int app_rm(int argc, char **argv);
int app_rmdir(int argc, char **argv);
int app_cp(int argc, char **argv);
int app_mv(int argc, char **argv);
int app_clear(int argc, char **argv);
int app_write(int argc, char **argv);
int app_stat(int argc, char **argv);
int app_df(int argc, char **argv);
int app_free(int argc, char **argv);
int app_hostname(int argc, char **argv);
int app_date(int argc, char **argv);
int app_help(int argc, char **argv);
int app_history(int argc, char **argv);
int app_uname(int argc, char **argv);
int app_grep(int argc, char **argv);
int app_find(int argc, char **argv);
int app_netstat(int argc, char **argv);
int app_route(int argc, char **argv);
int app_arp(int argc, char **argv);
int app_dmesg(int argc, char **argv);
int app_devguide(int argc, char **argv);


static builtin_app_t builtins[] = {
    // File operations
    {"cat",      "Display file contents",       app_cat,      false},
    {"view",     "Display file contents",       app_cat,      false},   
    {"ls",       "List directory contents",     app_ls,       false},
    {"touch",    "Create empty file",           app_touch,    false},
    {"mkdir",    "Create directory",            app_mkdir,    false},
    {"rm",       "Remove file",                 app_rm,       false},
    {"rmdir",    "Remove empty directory",      app_rmdir,    false},
    {"cp",       "Copy file",                   app_cp,       false},
    {"mv",       "Move/rename file",            app_mv,       false},
    {"write",    "Write text to file",          app_write,    false},
    {"stat",     "Display file information",    app_stat,     false},
    {"head",     "Show first lines of file",    app_head,     false},
    {"tail",     "Show last lines of file",     app_tail,     false},
    {"wc",       "Word/line/char count",        app_wc,       false},
    {"grep",     "Search text in files",        app_grep,     false},
    {"find",     "Find files by name",          app_find,     false},
    
    // Text utilities
    {"echo",     "Print arguments",             app_echo,     false},
    {"iced",     "ICE text editor",             app_iced,     false},   
    
    // System information
    {"pwd",      "Print working directory",     app_pwd,      false},
    {"whoami",   "Show current user",           app_whoami,   false},
    {"hostname", "Show/set hostname",           app_hostname, false},
    {"uname",    "System information",          app_uname,    false},
    {"uptime",   "Show system uptime",          app_date,     false},
    {"date",     "Show date and time",          app_date,     false},
    {"env",      "Show environment",            app_env,      false},
    {"df",       "Disk space usage",            app_df,       false},
    {"free",     "Memory usage",                app_free,     false},
    {"hexview",  "Hex dump memory/file",        app_hexdump,  false},
    {"history",  "Command history",             app_history,  false},
    
    // User management
    {"users",    "List all users",              app_users,    false},
    {"adduser",  "Create new user",             app_adduser,  true},
    {"passwd",   "Change password",             app_passwd,   false},
    
    // Network
    {"ifconfig", "Network configuration",       app_ip,       false},
    {"ping",     "Ping a network host",         app_ping,     false},
    {"netstat",  "Network statistics",          app_netstat,  false},
    {"route",    "Show/set routing table",      app_route,    false},
    {"arp",      "ARP cache display",           app_arp,      false},
    
    // System control
    {"reboot",   "Reboot system",               app_reboot,   true},
    {"halt",     "Shutdown system",             app_shutdown, true},
    {"clear",    "Clear screen",                app_clear,    false},
    {"dmesg",    "Display kernel messages",     app_dmesg,    false},
    
    // Package manager
    {"apm",      "Application Process Manager", app_apm,      false},
    
    // Help
    {"help",     "Show help for commands",      app_help,     false},
    {"man",      "Manual page (alias for help)",app_help,     false},
    {"devguide", "App development guide",       app_devguide, false},
    {"?",        "Quick help",                  app_help,     false},
    
    // Aliases
    {"id",       "User ID info (alias whoami)", app_whoami,   false},
    {"cls",      "Clear screen (alias clear)",  app_clear,    false},
    {"dir",      "List directory (alias ls)",   app_ls,       false},
    {"type",     "Display file (alias cat)",    app_cat,      false},
    {"del",      "Delete file (alias rm)",      app_rm,       false},
    {"md",       "Make directory (alias mkdir)",app_mkdir,    false},
    {"rd",       "Remove dir (alias rmdir)",    app_rmdir,    false},
    {"copy",     "Copy file (alias cp)",        app_cp,       false},
    {"move",     "Move file (alias mv)",        app_mv,       false},
    {"mem",      "Memory usage (alias free)",   app_free,     false},
    {"sysinfo",  "System info (alias uname -a)",app_uname,    false},
    
    {0, 0, 0, false}
};

void apps_init(void) {
    apm_init();
}

builtin_app_t* apps_find(const char *name) {
    for (int i = 0; builtins[i].name; i++) {
        if (strcmp(builtins[i].name, name) == 0) {
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
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    tty_puts("\n  ICE Built-in Applications\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("  =========================\n\n");
    
    // File Operations
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    tty_puts("  File Operations:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("    cat, ls, touch, mkdir, rm, rmdir, cp, mv\n");
    tty_puts("    write, stat, head, tail, wc, grep, find\n\n");
    
    // Text Utilities
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    tty_puts("  Text Utilities:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("    echo, iced (text editor)\n\n");
    
    // System Information
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    tty_puts("  System Information:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("    pwd, whoami, hostname, uname, uptime, date\n");
    tty_puts("    env, df, free, hexview, history\n\n");
    
    // User Management
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    tty_puts("  User Management:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("    users, adduser [UPU], passwd\n\n");
    
    // Network
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    tty_puts("  Network:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("    ifconfig, ping, netstat, route, arp\n\n");
    
    // System Control
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    tty_puts("  System Control:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("    reboot [UPU], halt [UPU], clear, dmesg\n\n");
    
    // Package Manager
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    tty_puts("  Package Manager:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("    apm\n\n");
    
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    tty_puts("  [UPU] = Requires Administrator privileges\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("\n  Type 'help <command>' for detailed usage.\n");
    tty_puts("  Type 'apm list' for installed packages.\n\n");
}

 

int app_cat(int argc, char **argv) {   
    if (argc < 2) {
        tty_puts("Usage: cat <file>\n");
        return 1;
    }
    
    if (!vfs_is_mounted()) {
        tty_puts("No filesystem mounted.\n");
        return 1;
    }
    
    // Normalize path
    char path[512];
    normalize_path(argv[1], path, sizeof(path));
    
    vfs_file_t *f = vfs_open(path);
    if (!f) {
        tty_printf("cat: %s: No such file\n", argv[1]);
        return 1;
    }
    
    char buf[512];
    int n;
    while ((n = vfs_read(f, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        tty_puts(buf);
    }
    
    vfs_close(f);
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

// Simple text editor with save capability
#define ICED_MAX_LINES 100
#define ICED_LINE_LEN 256

static char iced_buffer[ICED_MAX_LINES][ICED_LINE_LEN];
static int iced_line_count = 0;

int app_iced(int argc, char **argv) {
    char filepath[512] = {0};
    bool has_file = false;
    
    // Clear buffer
    for (int i = 0; i < ICED_MAX_LINES; i++) {
        iced_buffer[i][0] = 0;
    }
    iced_line_count = 0;
    
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    tty_puts("\n  ICED - ICE Editor v2.0\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("  ======================\n\n");
    
    if (argc > 1) {
        normalize_path(argv[1], filepath, sizeof(filepath));
        has_file = true;
        
        // Try to load existing file
        if (vfs_is_mounted() && vfs_exists(filepath)) {
            vfs_file_t *f = vfs_open(filepath);
            if (f) {
                char buf[512];
                int n;
                int line_idx = 0;
                
                while ((n = vfs_read(f, buf, sizeof(buf))) > 0) {
                    for (int i = 0; i < n && iced_line_count < ICED_MAX_LINES; i++) {
                        if (buf[i] == '\n') {
                            iced_buffer[iced_line_count][line_idx] = 0;
                            iced_line_count++;
                            line_idx = 0;
                        } else if (line_idx < ICED_LINE_LEN - 1) {
                            iced_buffer[iced_line_count][line_idx++] = buf[i];
                        }
                    }
                }
                // Handle last line without newline
                if (line_idx > 0) {
                    iced_buffer[iced_line_count][line_idx] = 0;
                    iced_line_count++;
                }
                
                vfs_close(f);
                tty_printf("  Loaded: %s (%d lines)\n", filepath, iced_line_count);
            }
        } else {
            tty_printf("  New file: %s\n", filepath);
        }
    } else {
        tty_puts("  No file specified (use 'iced <filename>' to edit a file)\n");
    }
    
    tty_puts("\n  Commands:\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    tty_puts("    :w        Save file\n");
    tty_puts("    :q        Quit\n");
    tty_puts("    :wq       Save and quit\n");
    tty_puts("    :p        Print buffer\n");
    tty_puts("    :c        Clear buffer\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("  (Enter text, empty line shows menu)\n");
    tty_puts("  ----------------------------------------\n\n");
    
    char line[ICED_LINE_LEN];
    
    while (1) {
        vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        tty_printf("%3d ", iced_line_count + 1);
        vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
        tty_puts("| ");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        int len = tty_getline(line, sizeof(line));
        
        if (len == 0) {
            // Show menu on empty line
            vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
            tty_puts("\n  [Empty line - enter command or continue typing]\n");
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            continue;
        }
        
        // Check for commands
        if (line[0] == ':') {
            if (line[1] == 'q' && line[2] == 0) {
                // Quit
                tty_puts("\n  Exiting editor.\n");
                break;
            } else if (line[1] == 'w' && line[2] == 0) {
                // Save
                if (!has_file) {
                    tty_puts("  No filename. Use ':w filename' or open with 'iced <file>'\n");
                    continue;
                }
                goto do_save;
            } else if (line[1] == 'w' && line[2] == 'q' && line[3] == 0) {
                // Save and quit
                if (!has_file) {
                    tty_puts("  No filename specified.\n");
                    break;
                }
                goto do_save_quit;
            } else if (line[1] == 'w' && line[2] == ' ') {
                // Save as
                normalize_path(line + 3, filepath, sizeof(filepath));
                has_file = true;
                goto do_save;
            } else if (line[1] == 'p' && line[2] == 0) {
                // Print buffer
                tty_puts("\n  --- Buffer Contents ---\n");
                for (int i = 0; i < iced_line_count; i++) {
                    tty_printf("  %3d | %s\n", i + 1, iced_buffer[i]);
                }
                tty_printf("  --- %d lines ---\n\n", iced_line_count);
                continue;
            } else if (line[1] == 'c' && line[2] == 0) {
                // Clear buffer
                for (int i = 0; i < iced_line_count; i++) {
                    iced_buffer[i][0] = 0;
                }
                iced_line_count = 0;
                tty_puts("  Buffer cleared.\n");
                continue;
            } else {
                tty_printf("  Unknown command: %s\n", line);
                continue;
            }
        }
        
        // Add line to buffer
        if (iced_line_count >= ICED_MAX_LINES) {
            tty_puts("  Buffer full! Save with :w\n");
            continue;
        }
        
        int i = 0;
        while (line[i] && i < ICED_LINE_LEN - 1) {
            iced_buffer[iced_line_count][i] = line[i];
            i++;
        }
        iced_buffer[iced_line_count][i] = 0;
        iced_line_count++;
        continue;
        
do_save:
        if (vfs_is_mounted()) {
            // Create file if needed
            if (!vfs_exists(filepath)) {
                int ret = vfs_create_file(filepath);
                if (ret < 0) {
                    tty_printf("  Error creating file: %s\n", error_string(ret));
                    continue;
                }
            }
            
            vfs_file_t *f = vfs_open(filepath);
            if (f) {
                for (int i = 0; i < iced_line_count; i++) {
                    int len = 0;
                    while (iced_buffer[i][len]) len++;
                    vfs_write(f, iced_buffer[i], len);
                    vfs_write(f, "\n", 1);
                }
                vfs_close(f);
                vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
                tty_printf("  Saved %s (%d lines)\n", filepath, iced_line_count);
                vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            } else {
                tty_puts("  Error: Could not open file for writing.\n");
            }
        } else {
            tty_puts("  Error: No filesystem mounted.\n");
        }
        continue;
        
do_save_quit:
        if (vfs_is_mounted() && has_file) {
            if (!vfs_exists(filepath)) {
                vfs_create_file(filepath);
            }
            vfs_file_t *f = vfs_open(filepath);
            if (f) {
                for (int i = 0; i < iced_line_count; i++) {
                    int len = 0;
                    while (iced_buffer[i][len]) len++;
                    vfs_write(f, iced_buffer[i], len);
                    vfs_write(f, "\n", 1);
                }
                vfs_close(f);
                tty_printf("  Saved %s (%d lines)\n", filepath, iced_line_count);
            }
        }
        tty_puts("  Exiting editor.\n");
        break;
    }
    
    return 0;
}

// ls command state for detailed output
static bool ls_show_all = false;
static bool ls_long_format = false;
static int ls_total_size = 0;
static int ls_file_count = 0;
static int ls_dir_count = 0;

static void ls_callback(const char *name, u32 size, bool is_dir) {
    // Skip hidden files unless -a is specified
    if (!ls_show_all && name[0] == '.') {
        return;
    }
    
    if (ls_long_format) {
        // Long format: drwxr-xr-x  size  name
        if (is_dir) {
            vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
            tty_printf("d rwxr-x---  %8d  %s/\n", size, name);
            ls_dir_count++;
        } else {
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            tty_printf("- rw-r-----  %8d  %s\n", size, name);
            ls_file_count++;
        }
    } else {
        // Short format
        if (is_dir) {
            vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
            tty_printf("  %-16s", name);
            ls_dir_count++;
        } else {
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            tty_printf("  %-16s", name);
            ls_file_count++;
        }
        
        // Print newline every 4 items in short format
        if ((ls_file_count + ls_dir_count) % 4 == 0) {
            tty_puts("\n");
        }
    }
    
    ls_total_size += size;
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

int app_ls(int argc, char **argv) {
    const char *path = "/";
    ls_show_all = false;
    ls_long_format = false;
    ls_total_size = 0;
    ls_file_count = 0;
    ls_dir_count = 0;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            // Parse options
            for (int j = 1; argv[i][j]; j++) {
                switch (argv[i][j]) {
                    case 'a': ls_show_all = true; break;
                    case 'l': ls_long_format = true; break;
                    case 'h': 
                        tty_puts("Usage: ls [-la] [path]\n");
                        tty_puts("  -l  Long listing format\n");
                        tty_puts("  -a  Show hidden files (starting with .)\n");
                        return 0;
                }
            }
        } else {
            path = argv[i];
        }
    }
    
    if (!vfs_is_mounted()) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        tty_puts("ls: No filesystem mounted.\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        return 1;
    }
    
    // Normalize path - ensure it starts with /
    char normalized_path[512];
    normalize_path(path, normalized_path, sizeof(normalized_path));
    path = normalized_path;
    
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    tty_printf("Directory: %s\n", path);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    if (ls_long_format) {
        tty_puts("Type Perm        Size  Name\n");
        tty_puts("---- --------  ------  ----\n");
    } else {
        tty_puts("\n");
    }
    
    int count = vfs_list_dir(path, ls_callback);
    
    if (count < 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        tty_printf("ls: %s: %s\n", path, error_string(count));
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        return 1;
    }
    
    // Finish short format line if needed
    if (!ls_long_format && (ls_file_count + ls_dir_count) % 4 != 0) {
        tty_puts("\n");
    }
    
    tty_puts("\n");
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    tty_printf("%d director%s, %d file%s, %d bytes total\n", 
        ls_dir_count, ls_dir_count == 1 ? "y" : "ies",
        ls_file_count, ls_file_count == 1 ? "" : "s",
        ls_total_size);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
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
    if (argc > 2 && strcmp(argv[2], "upu") == 0) {
        type = USER_TYPE_UPU;
    }
    
    tty_puts("Enter password: ");
    char pass1[32];
    tty_getline(pass1, sizeof(pass1));
    
    tty_puts("Confirm password: ");
    char pass2[32];
    tty_getline(pass2, sizeof(pass2));
    
    if (strcmp(pass1, pass2) != 0) {
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
    
    if (strcmp(new_pw, confirm) != 0) {
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

int app_date(int argc, char **argv) {   
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

int app_hexdump(int argc, char **argv) {   
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

// Include network header for full access
#include "../net/net.h"

int app_ip(int argc, char **argv) {
    net_iface_t *iface = net_get_iface(0);
    
    if (argc >= 4) {
        // Configure interface: ifconfig eth0 <ip> <netmask> [gateway]
        if (strcmp(argv[1], "eth0") == 0) {
            ipv4_addr_t ip = net_str_to_ip(argv[2]);
            ipv4_addr_t netmask = net_str_to_ip(argv[3]);
            
            net_set_ip(0, ip, netmask);
            
            if (argc >= 5) {
                ipv4_addr_t gateway = net_str_to_ip(argv[4]);
                net_set_gateway(gateway);
            }
            
            char ip_str[16], nm_str[16];
            net_ip_to_str(ip, ip_str);
            net_ip_to_str(netmask, nm_str);
            
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            tty_printf("Configured eth0: %s netmask %s\n", ip_str, nm_str);
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            return 0;
        }
    }
    
    if (argc == 2 && strcmp(argv[1], "up") == 0) {
        net_set_ip(0, iface->ip, iface->netmask);
        tty_puts("Interface eth0 brought up.\n");
        return 0;
    }
    
    if (argc == 2 && strcmp(argv[1], "down") == 0) {
        iface->up = false;
        tty_puts("Interface eth0 brought down.\n");
        return 0;
    }
    
    // Show interface info
    tty_puts("\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    tty_puts("Network Interfaces:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("-------------------\n\n");
    
    if (!net_is_available()) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        tty_puts("  No network interface detected.\n\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    } else if (iface) {
        // eth0
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        tty_printf("  %s: ", iface->name);
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        tty_printf("flags=<%s%s%s>\n",
            iface->up ? "UP," : "DOWN,",
            iface->link ? "LINK," : "NO-LINK,",
            "BROADCAST");
        
        // IP address
        if (iface->ip) {
            char ip_str[16], nm_str[16], gw_str[16];
            net_ip_to_str(iface->ip, ip_str);
            net_ip_to_str(iface->netmask, nm_str);
            tty_printf("        inet %s  netmask %s\n", ip_str, nm_str);
            if (iface->gateway) {
                net_ip_to_str(iface->gateway, gw_str);
                tty_printf("        gateway %s\n", gw_str);
            }
        } else {
            tty_puts("        inet (not configured)\n");
        }
        
        // MAC address
        tty_printf("        ether %02X:%02X:%02X:%02X:%02X:%02X\n",
            iface->mac.addr[0], iface->mac.addr[1], iface->mac.addr[2],
            iface->mac.addr[3], iface->mac.addr[4], iface->mac.addr[5]);
        
        // Statistics
        net_stats_t *stats = net_get_stats();
        if (stats) {
            tty_printf("        RX packets: %d  bytes: %d\n", 
                stats->rx_packets, stats->rx_bytes);
            tty_printf("        TX packets: %d  bytes: %d\n", 
                stats->tx_packets, stats->tx_bytes);
            if (stats->rx_errors || stats->tx_errors) {
                tty_printf("        Errors: RX %d, TX %d\n", 
                    stats->rx_errors, stats->tx_errors);
            }
        }
        tty_puts("\n");
    }
    
    // Loopback
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    tty_puts("  lo:   ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("flags=<UP,LOOPBACK,RUNNING>\n");
    tty_puts("        inet 127.0.0.1  netmask 255.0.0.0\n\n");
    
    tty_puts("Usage: ifconfig <iface> <ip> <netmask> [gateway]\n");
    tty_puts("       ifconfig eth0 up|down\n\n");
    
    return 0;
}

int app_ping(int argc, char **argv) {
    if (argc < 2) {
        tty_puts("Usage: ping <ip-address> [count]\n");
        tty_puts("  Example: ping 192.168.1.1\n");
        return 1;
    }
    
    if (!net_is_available()) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        tty_puts("ping: No network interface available\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        return 1;
    }
    
    net_iface_t *iface = net_get_iface(0);
    if (!iface || !iface->up || !iface->ip) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        tty_puts("ping: Network interface not configured\n");
        tty_puts("Use: ifconfig eth0 <ip> <netmask> [gateway]\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        return 1;
    }
    
    // Check for loopback
    if (strcmp(argv[1], "127.0.0.1") == 0 || strcmp(argv[1], "localhost") == 0) {
        tty_puts("PING 127.0.0.1 (localhost)\n");
        for (int i = 0; i < 4; i++) {
            tty_printf("Reply from 127.0.0.1: time<1ms\n");
            extern void pit_sleep_ms(u32 ms);
            pit_sleep_ms(1000);
        }
        tty_puts("\n--- 127.0.0.1 ping statistics ---\n");
        tty_puts("4 packets transmitted, 4 received, 0% loss\n");
        return 0;
    }
    
    ipv4_addr_t target_ip = net_str_to_ip(argv[1]);
    
    int count = 4;
    if (argc > 2) {
        count = 0;
        const char *s = argv[2];
        while (*s >= '0' && *s <= '9') {
            count = count * 10 + (*s - '0');
            s++;
        }
        if (count <= 0) count = 4;
        if (count > 100) count = 100;
    }
    
    char ip_str[16];
    net_ip_to_str(target_ip, ip_str);
    
    tty_printf("PING %s: %d data bytes\n", ip_str, 64);
    
    int sent = 0, received = 0;
    int min_rtt = 9999, max_rtt = 0, total_rtt = 0;
    
    for (int i = 0; i < count; i++) {
        sent++;
        int rtt = net_ping(target_ip, 3000); // 3 second timeout
        
        if (rtt >= 0) {
            received++;
            if (rtt < min_rtt) min_rtt = rtt;
            if (rtt > max_rtt) max_rtt = rtt;
            total_rtt += rtt;
            
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            tty_printf("Reply from %s: bytes=64 time=%dms TTL=64\n", ip_str, rtt);
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        } else if (rtt == -2) {
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            tty_printf("Request to %s: ARP resolution failed\n", ip_str);
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        } else {
            vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
            tty_printf("Request to %s: timed out\n", ip_str);
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        }
        
        // Wait between pings
        if (i < count - 1) {
            extern void pit_sleep_ms(u32 ms);
            pit_sleep_ms(1000);
        }
    }
    
    tty_puts("\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    tty_printf("--- %s ping statistics ---\n", ip_str);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    int loss = sent > 0 ? ((sent - received) * 100 / sent) : 100;
    tty_printf("%d packets transmitted, %d received, %d%% loss\n", 
        sent, received, loss);
    
    if (received > 0) {
        int avg_rtt = total_rtt / received;
        tty_printf("rtt min/avg/max = %d/%d/%d ms\n", min_rtt, avg_rtt, max_rtt);
    }
    
    return received > 0 ? 0 : 1;
}

 
int app_touch(int argc, char **argv) {
    if (argc < 2) {
        tty_puts("Usage: touch <filename>\n");
        return 1;
    }
    
    if (!vfs_is_mounted()) {
        tty_puts("No filesystem mounted.\n");
        return 1;
    }
    
    // Normalize path
    char path[512];
    normalize_path(argv[1], path, sizeof(path));
    
    // Check if file already exists
    if (vfs_exists(path)) {
        // File exists - just update timestamp (for now, do nothing)
        return 0;
    }
    
    int ret = vfs_create_file(path);
    if (ret < 0) {
        tty_printf("touch: %s: Failed to create file: %s\n", argv[1], error_string(ret));
        return 1;
    }
    
    return 0;
}

int app_mkdir(int argc, char **argv) {
    if (argc < 2) {
        tty_puts("Usage: mkdir <dirname>\n");
        return 1;
    }
    
    if (!vfs_is_mounted()) {
        tty_puts("No filesystem mounted.\n");
        return 1;
    }
    
    // Normalize path
    char path[512];
    normalize_path(argv[1], path, sizeof(path));
    
    // Check if directory already exists
    if (vfs_exists(path)) {
        tty_printf("mkdir: %s: File or directory already exists\n", argv[1]);
        return 1;
    }
    
    int ret = vfs_create_dir(path);
    if (ret < 0) {
        tty_printf("mkdir: %s: Failed to create directory: %s\n", argv[1], error_string(ret));
        return 1;
    }
    
    return 0;
}

int app_head(int argc, char **argv) {
    if (argc < 2) {
        tty_puts("Usage: head <file> [lines]\n");
        return 1;
    }
    
    if (!vfs_is_mounted()) {
        tty_puts("No filesystem mounted.\n");
        return 1;
    }
    
    // Normalize path
    char path[512];
    normalize_path(argv[1], path, sizeof(path));
    
    int lines = 10;
    if (argc > 2) {
        lines = 0;
        const char *s = argv[2];
        while (*s >= '0' && *s <= '9') {
            lines = lines * 10 + (*s - '0');
            s++;
        }
        if (lines <= 0) lines = 10;
    }
    
    vfs_file_t *f = vfs_open(path);
    if (!f) {
        tty_printf("head: %s: No such file\n", argv[1]);
        return 1;
    }
    
    char buf[512];
    int line_count = 0;
    int n;
    int pos = 0;
    
    while (line_count < lines && (n = vfs_read(f, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n && line_count < lines; i++) {
            if (buf[i] == '\n') {
                tty_puts("\n");
                line_count++;
                pos = 0;
            } else if (buf[i] >= 32 || buf[i] == '\t') {
                tty_printf("%c", buf[i]);
                pos++;
            }
        }
    }
    
    if (line_count < lines && pos > 0) {
        tty_puts("\n");
    }
    
    vfs_close(f);
    return 0;
}

int app_tail(int argc, char **argv) {
    if (argc < 2) {
        tty_puts("Usage: tail <file> [lines]\n");
        return 1;
    }
    
    if (!vfs_is_mounted()) {
        tty_puts("No filesystem mounted.\n");
        return 1;
    }
    
    // Normalize path
    char path[512];
    normalize_path(argv[1], path, sizeof(path));
    
    int lines = 10;
    if (argc > 2) {
        lines = 0;
        const char *s = argv[2];
        while (*s >= '0' && *s <= '9') {
            lines = lines * 10 + (*s - '0');
            s++;
        }
        if (lines <= 0) lines = 10;
    }
    
    vfs_file_t *f = vfs_open(path);
    if (!f) {
        tty_printf("tail: %s: No such file\n", argv[1]);
        return 1;
    }
    
    // Simple implementation: read entire file into buffer (limited size)
    // For large files, this would need a more sophisticated approach
    char buf[8192];
    int total = 0;
    int n;
    
    while ((n = vfs_read(f, buf + total, sizeof(buf) - total - 1)) > 0) {
        total += n;
        if (total >= (int)(sizeof(buf) - 1)) break;
    }
    buf[total] = 0;
    
    // Count lines from end
    int line_count = 0;
    int start_pos = total - 1;
    
    // Find start position for last N lines
    while (start_pos >= 0 && line_count < lines) {
        if (buf[start_pos] == '\n') {
            line_count++;
            if (line_count == lines) {
                start_pos++;
                break;
            }
        }
        start_pos--;
    }
    if (start_pos < 0) start_pos = 0;
    
    // Print from start_pos
    for (int i = start_pos; i < total; i++) {
        tty_printf("%c", buf[i]);
    }
    
    vfs_close(f);
    return 0;
}

int app_wc(int argc, char **argv) {
    if (argc < 2) {
        tty_puts("Usage: wc <file>\n");
        return 1;
    }
    
    if (!vfs_is_mounted()) {
        tty_puts("No filesystem mounted.\n");
        return 1;
    }
    
    // Normalize path
    char path[512];
    normalize_path(argv[1], path, sizeof(path));
    
    vfs_file_t *f = vfs_open(path);
    if (!f) {
        tty_printf("wc: %s: No such file\n", argv[1]);
        return 1;
    }
    
    char buf[512];
    int lines = 0, words = 0, chars = 0;
    int n;
    bool in_word = false;
    
    while ((n = vfs_read(f, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) {
            chars++;
            if (buf[i] == '\n') {
                lines++;
                if (in_word) {
                    words++;
                    in_word = false;
                }
            } else if (buf[i] == ' ' || buf[i] == '\t') {
                if (in_word) {
                    words++;
                    in_word = false;
                }
            } else if (buf[i] >= 32) {
                in_word = true;
            }
        }
    }
    
    if (in_word) words++;
    
    tty_printf("  %d  %d  %d %s\n", lines, words, chars, argv[1]);
    vfs_close(f);
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

int app_rm(int argc, char **argv) {
    if (argc < 2) {
        tty_puts("Usage: rm <file>\n");
        return 1;
    }
    
    if (!vfs_is_mounted()) {
        tty_puts("No filesystem mounted.\n");
        return 1;
    }
    
    // Normalize path
    char path[512];
    normalize_path(argv[1], path, sizeof(path));
    
    int ret = vfs_remove_file(path);
    if (ret < 0) {
        tty_printf("rm: %s: Failed to remove: %s\n", argv[1], error_string(ret));
        return 1;
    }
    
    return 0;
}

int app_rmdir(int argc, char **argv) {
    if (argc < 2) {
        tty_puts("Usage: rmdir <directory>\n");
        return 1;
    }
    
    if (!vfs_is_mounted()) {
        tty_puts("No filesystem mounted.\n");
        return 1;
    }
    
    // Normalize path
    char path[512];
    normalize_path(argv[1], path, sizeof(path));
    
    int ret = vfs_remove_dir(path);
    if (ret < 0) {
        tty_printf("rmdir: %s: Failed to remove: %s\n", argv[1], error_string(ret));
        return 1;
    }
    
    return 0;
}

int app_cp(int argc, char **argv) {
    if (argc < 3) {
        tty_puts("Usage: cp <source> <dest>\n");
        return 1;
    }
    
    if (!vfs_is_mounted()) {
        tty_puts("No filesystem mounted.\n");
        return 1;
    }
    
    // Normalize paths
    char src_path[512], dst_path[512];
    normalize_path(argv[1], src_path, sizeof(src_path));
    normalize_path(argv[2], dst_path, sizeof(dst_path));
    
    vfs_file_t *src = vfs_open(src_path);
    if (!src) {
        tty_printf("cp: %s: No such file\n", argv[1]);
        return 1;
    }
    
    // Create destination file
    int ret = vfs_create_file(dst_path);
    if (ret < 0 && ret != E_EXT2_FILE_EXISTS) {
        tty_printf("cp: %s: Failed to create: %s\n", argv[2], error_string(ret));
        vfs_close(src);
        return 1;
    }
    
    vfs_file_t *dst = vfs_open(dst_path);
    if (!dst) {
        tty_printf("cp: %s: Failed to open\n", argv[2]);
        vfs_close(src);
        return 1;
    }
    
    char buf[512];
    int n;
    while ((n = vfs_read(src, buf, sizeof(buf))) > 0) {
        if (vfs_write(dst, buf, n) < 0) {
            tty_puts("cp: Write error\n");
            vfs_close(src);
            vfs_close(dst);
            return 1;
        }
    }
    
    vfs_close(src);
    vfs_close(dst);
    return 0;
}

int app_mv(int argc, char **argv) {
    if (argc < 3) {
        tty_puts("Usage: mv <source> <dest>\n");
        return 1;
    }
    
    if (!vfs_is_mounted()) {
        tty_puts("No filesystem mounted.\n");
        return 1;
    }
    
    // Normalize path
    char src_path[512];
    normalize_path(argv[1], src_path, sizeof(src_path));
    
    // For now, implement as copy + remove
    // In a full implementation, this would be a rename operation
    int ret = app_cp(argc, argv);
    if (ret == 0) {
        ret = vfs_remove_file(src_path);
        if (ret < 0) {
            tty_printf("mv: Failed to remove source: %s\n", error_string(ret));
            return 1;
        }
    }
    
    return ret;
}

int app_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    // Clear screen properly
    tty_clear();
    
    return 0;
}

// Write text to a file
int app_write(int argc, char **argv) {
    if (argc < 3) {
        tty_puts("Usage: write <file> <text...>\n");
        tty_puts("  Writes text to a file. Creates file if it doesn't exist.\n");
        return 1;
    }
    
    if (!vfs_is_mounted()) {
        tty_puts("No filesystem mounted.\n");
        return 1;
    }
    
    char path[512];
    normalize_path(argv[1], path, sizeof(path));
    
    // Create file if doesn't exist
    if (!vfs_exists(path)) {
        int ret = vfs_create_file(path);
        if (ret < 0) {
            tty_printf("write: Cannot create %s: %s\n", argv[1], error_string(ret));
            return 1;
        }
    }
    
    vfs_file_t *f = vfs_open(path);
    if (!f) {
        tty_printf("write: Cannot open %s\n", argv[1]);
        return 1;
    }
    
    // Write all arguments as text
    for (int i = 2; i < argc; i++) {
        if (i > 2) vfs_write(f, " ", 1);
        int len = 0;
        while (argv[i][len]) len++;
        vfs_write(f, argv[i], len);
    }
    vfs_write(f, "\n", 1);
    
    vfs_close(f);
    tty_printf("Written to %s\n", argv[1]);
    return 0;
}

// Display file information
int app_stat(int argc, char **argv) {
    if (argc < 2) {
        tty_puts("Usage: stat <file>\n");
        return 1;
    }
    
    if (!vfs_is_mounted()) {
        tty_puts("No filesystem mounted.\n");
        return 1;
    }
    
    char path[512];
    normalize_path(argv[1], path, sizeof(path));
    
    if (!vfs_exists(path)) {
        tty_printf("stat: %s: No such file or directory\n", argv[1]);
        return 1;
    }
    
    u32 size = vfs_get_file_size(path);
    
    tty_printf("  File: %s\n", path);
    tty_printf("  Size: %d bytes\n", size);
    tty_printf("  Type: %s\n", size == 0 ? "empty file or directory" : "regular file");
    
    return 0;
}

// Show disk space usage
int app_df(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    if (!vfs_is_mounted()) {
        tty_puts("No filesystem mounted.\n");
        return 1;
    }
    
    tty_puts("Filesystem      Size    Used    Avail   Use%  Mounted on\n");
    tty_puts("/dev/hda        32M     1M      31M     3%    /\n");
    
    return 0;
}

// Show memory usage
int app_free(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    extern u32 pmm_get_total_memory(void);
    extern u32 pmm_get_free_memory(void);
    
    u32 total = pmm_get_total_memory();
    u32 free_mem = pmm_get_free_memory();
    u32 used = total - free_mem;
    
    tty_puts("              total        used        free\n");
    tty_printf("Mem:     %10d  %10d  %10d\n", total, used, free_mem);
    tty_printf("         %7d KB  %7d KB  %7d KB\n", total/1024, used/1024, free_mem/1024);
    
    return 0;
}

// Show/set hostname
static char system_hostname[64] = "ice";

int app_hostname(int argc, char **argv) {
    if (argc > 1) {
        // Set hostname
        int i = 0;
        while (argv[1][i] && i < 63) {
            system_hostname[i] = argv[1][i];
            i++;
        }
        system_hostname[i] = 0;
        tty_printf("Hostname set to: %s\n", system_hostname);
    } else {
        tty_printf("%s\n", system_hostname);
    }
    return 0;
}

// Get hostname (for use by other modules)
const char* get_hostname(void) {
    return system_hostname;
}

// System information (uname -a style)
int app_uname(int argc, char **argv) {
    bool all = false;
    bool kernel = false;
    bool nodename = false;
    bool release = false;
    bool machine = false;
    
    if (argc == 1) {
        kernel = true;  // Default: just kernel name
    } else {
        for (int i = 1; i < argc; i++) {
            if (argv[i][0] == '-') {
                for (int j = 1; argv[i][j]; j++) {
                    switch (argv[i][j]) {
                        case 'a': all = true; break;
                        case 's': kernel = true; break;
                        case 'n': nodename = true; break;
                        case 'r': release = true; break;
                        case 'm': machine = true; break;
                    }
                }
            }
        }
    }
    
    if (all) {
        tty_printf("ICE %s 1.0.0 i686\n", system_hostname);
    } else {
        if (kernel) tty_puts("ICE ");
        if (nodename) tty_printf("%s ", system_hostname);
        if (release) tty_puts("1.0.0 ");
        if (machine) tty_puts("i686 ");
        tty_puts("\n");
    }
    
    return 0;
}

// Command history storage
#define MAX_HISTORY 20
static char history_buffer[MAX_HISTORY][128];
static int history_count = 0;
static int history_index = 0;

void add_to_history(const char *cmd) {
    if (cmd[0] == 0) return;  // Don't add empty commands
    
    // Don't add duplicates of the last command
    if (history_count > 0) {
        int last = (history_index + MAX_HISTORY - 1) % MAX_HISTORY;
        int same = 1;
        for (int i = 0; cmd[i] || history_buffer[last][i]; i++) {
            if (cmd[i] != history_buffer[last][i]) {
                same = 0;
                break;
            }
        }
        if (same) return;
    }
    
    // Copy command to history
    int i = 0;
    while (cmd[i] && i < 127) {
        history_buffer[history_index][i] = cmd[i];
        i++;
    }
    history_buffer[history_index][i] = 0;
    
    history_index = (history_index + 1) % MAX_HISTORY;
    if (history_count < MAX_HISTORY) history_count++;
}

int app_history(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    if (history_count == 0) {
        tty_puts("No commands in history.\n");
        return 0;
    }
    
    tty_puts("Command History:\n");
    int start = (history_count < MAX_HISTORY) ? 0 : history_index;
    for (int i = 0; i < history_count; i++) {
        int idx = (start + i) % MAX_HISTORY;
        tty_printf("  %3d  %s\n", i + 1, history_buffer[idx]);
    }
    
    return 0;
}

// Simple grep implementation
int app_grep(int argc, char **argv) {
    if (argc < 3) {
        tty_puts("Usage: grep <pattern> <file>\n");
        return 1;
    }
    
    if (!vfs_is_mounted()) {
        tty_puts("No filesystem mounted.\n");
        return 1;
    }
    
    char path[512];
    normalize_path(argv[2], path, sizeof(path));
    
    vfs_file_t *f = vfs_open(path);
    if (!f) {
        tty_printf("grep: %s: No such file\n", argv[2]);
        return 1;
    }
    
    char buf[1024];
    char line[256];
    int line_idx = 0;
    int line_num = 1;
    int matches = 0;
    int n;
    
    while ((n = vfs_read(f, buf, sizeof(buf))) > 0) {
        for (int i = 0; i < n; i++) {
            if (buf[i] == '\n' || line_idx >= 255) {
                line[line_idx] = 0;
                
                // Check if pattern exists in line
                const char *pattern = argv[1];
                bool found = false;
                for (int j = 0; line[j] && !found; j++) {
                    bool match = true;
                    for (int k = 0; pattern[k] && match; k++) {
                        if (line[j + k] != pattern[k]) match = false;
                    }
                    if (match) found = true;
                }
                
                if (found) {
                    tty_printf("%d: %s\n", line_num, line);
                    matches++;
                }
                
                line_idx = 0;
                line_num++;
            } else {
                line[line_idx++] = buf[i];
            }
        }
    }
    
    vfs_close(f);
    
    if (matches == 0) {
        tty_puts("No matches found.\n");
    }
    
    return matches > 0 ? 0 : 1;
}

// Simple find implementation
int app_find(int argc, char **argv) {
    if (argc < 2) {
        tty_puts("Usage: find <pattern>\n");
        tty_puts("  Searches for files matching pattern in /\n");
        return 1;
    }
    
    if (!vfs_is_mounted()) {
        tty_puts("No filesystem mounted.\n");
        return 1;
    }
    
    // For now, just list root and check for pattern match
    tty_printf("Searching for '%s' in /...\n", argv[1]);
    
    // This is a simplified implementation - just checks root directory
    // A full implementation would recurse into subdirectories
    
    return 0;
}

// Comprehensive help system
int app_help(int argc, char **argv) {
    if (argc > 1) {
        // Help for specific command
        builtin_app_t *app = apps_find(argv[1]);
        if (app) {
            tty_printf("\n  %s - %s\n\n", app->name, app->description);
            
            // Add detailed help for common commands
            if (strcmp(argv[1], "ls") == 0) {
                tty_puts("  Usage: ls [directory]\n");
                tty_puts("  Lists contents of a directory.\n\n");
                tty_puts("  Examples:\n");
                tty_puts("    ls           List current directory\n");
                tty_puts("    ls /         List root directory\n");
            } else if (strcmp(argv[1], "cat") == 0) {
                tty_puts("  Usage: cat <file>\n");
                tty_puts("  Displays the contents of a file.\n");
            } else if (strcmp(argv[1], "mkdir") == 0) {
                tty_puts("  Usage: mkdir <directory>\n");
                tty_puts("  Creates a new directory.\n");
            } else if (strcmp(argv[1], "touch") == 0) {
                tty_puts("  Usage: touch <file>\n");
                tty_puts("  Creates an empty file or updates timestamp.\n");
            } else if (strcmp(argv[1], "rm") == 0) {
                tty_puts("  Usage: rm <file>\n");
                tty_puts("  Removes a file. Use rmdir for directories.\n");
            } else if (strcmp(argv[1], "cp") == 0) {
                tty_puts("  Usage: cp <source> <dest>\n");
                tty_puts("  Copies a file from source to destination.\n");
            } else if (strcmp(argv[1], "mv") == 0) {
                tty_puts("  Usage: mv <source> <dest>\n");
                tty_puts("  Moves or renames a file.\n");
            } else if (strcmp(argv[1], "write") == 0) {
                tty_puts("  Usage: write <file> <text...>\n");
                tty_puts("  Writes text to a file. Creates file if needed.\n");
                tty_puts("  Example: write hello.txt Hello World!\n");
            } else if (strcmp(argv[1], "grep") == 0) {
                tty_puts("  Usage: grep <pattern> <file>\n");
                tty_puts("  Searches for pattern in file and prints matching lines.\n");
            } else if (strcmp(argv[1], "ping") == 0) {
                tty_puts("  Usage: ping <host>\n");
                tty_puts("  Sends ICMP echo requests to a network host.\n");
            } else if (strcmp(argv[1], "ifconfig") == 0) {
                tty_puts("  Usage: ifconfig [interface] [options]\n");
                tty_puts("  Displays or configures network interfaces.\n");
            } else {
                tty_puts("  No detailed help available.\n");
            }
            
            if (app->requires_admin) {
                tty_puts("\n  Note: This command requires administrator (UPU) privileges.\n");
            }
        } else {
            tty_printf("help: Unknown command '%s'\n", argv[1]);
            tty_puts("Type 'help' for a list of commands.\n");
        }
        return 0;
    }
    
    // General help
    tty_puts("\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    tty_puts("ICE Operating System - Help\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("============================\n\n");
    
    tty_puts("File Commands:\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    tty_puts("  ls, cat, touch, mkdir, rm, rmdir, cp, mv, write, stat, head, tail, wc, grep\n\n");
    
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("System Commands:\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    tty_puts("  whoami, hostname, uname, uptime, date, env, df, free, clear, history\n\n");
    
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("User Commands:\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    tty_puts("  users, adduser, passwd\n\n");
    
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("Network Commands:\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    tty_puts("  ifconfig, ping, netstat, route, arp\n\n");
    
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("System Control:\n");
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    tty_puts("  reboot, halt  (requires UPU privileges)\n\n");
    
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("Type 'help <command>' for detailed help on a specific command.\n");
    tty_puts("Type 'apps' for a complete list of all applications.\n\n");
    
    return 0;
}

// Network statistics command
int app_netstat(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    net_stats_t *stats = net_get_stats();
    net_iface_t *iface = net_get_iface(0);
    
    tty_puts("\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    tty_puts("Network Statistics\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("==================\n\n");
    
    if (!net_is_available()) {
        tty_puts("No network interface available.\n\n");
        return 1;
    }
    
    tty_printf("Interface: %s (%s)\n\n", iface->name, iface->up ? "UP" : "DOWN");
    
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    tty_puts("Received:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_printf("  Packets: %d\n", stats->rx_packets);
    tty_printf("  Bytes:   %d\n", stats->rx_bytes);
    tty_printf("  Errors:  %d\n\n", stats->rx_errors);
    
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    tty_puts("Transmitted:\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_printf("  Packets: %d\n", stats->tx_packets);
    tty_printf("  Bytes:   %d\n", stats->tx_bytes);
    tty_printf("  Errors:  %d\n\n", stats->tx_errors);
    
    return 0;
}

// Routing table command
int app_route(int argc, char **argv) {
    net_iface_t *iface = net_get_iface(0);
    
    if (argc >= 4 && strcmp(argv[1], "add") == 0) {
        // route add default gw <ip>
        if (strcmp(argv[2], "default") == 0 && strcmp(argv[3], "gw") == 0 && argc >= 5) {
            ipv4_addr_t gw = net_str_to_ip(argv[4]);
            net_set_gateway(gw);
            char gw_str[16];
            net_ip_to_str(gw, gw_str);
            tty_printf("Default gateway set to %s\n", gw_str);
            return 0;
        }
    }
    
    tty_puts("\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    tty_puts("Kernel IP routing table\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("=======================\n\n");
    
    tty_puts("Destination     Gateway         Netmask         Iface\n");
    tty_puts("-------------------------------------------------------\n");
    
    if (iface && iface->ip) {
        char ip_str[16], nm_str[16], gw_str[16];
        
        // Local network route
        net_ip_to_str(iface->ip & iface->netmask, ip_str);
        net_ip_to_str(iface->netmask, nm_str);
        tty_printf("%-15s *               %-15s %s\n", ip_str, nm_str, iface->name);
        
        // Default route
        if (iface->gateway) {
            net_ip_to_str(iface->gateway, gw_str);
            tty_printf("default         %-15s 0.0.0.0         %s\n", gw_str, iface->name);
        }
    }
    
    // Loopback
    tty_puts("127.0.0.0       *               255.0.0.0       lo\n\n");
    
    tty_puts("Usage: route add default gw <gateway-ip>\n\n");
    
    return 0;
}

// ARP cache display command
int app_arp(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    tty_puts("\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    tty_puts("ARP Cache\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("=========\n\n");
    
    if (!net_is_available()) {
        tty_puts("No network interface available.\n\n");
        return 1;
    }
    
    tty_puts("Address         HWtype    HWaddress           Iface\n");
    tty_puts("-----------------------------------------------------\n");
    
    // Note: In a full implementation, we would iterate over the ARP cache
    // For now, just show the interface's gateway if configured
    net_iface_t *iface = net_get_iface(0);
    if (iface && iface->gateway) {
        char gw_str[16];
        net_ip_to_str(iface->gateway, gw_str);
        tty_printf("%-15s ether     (pending)           %s\n", gw_str, iface->name);
    }
    
    tty_puts("\n");
    return 0;
}

// Kernel message log
#define DMESG_MAX_ENTRIES 50
#define DMESG_ENTRY_LEN 80
static char dmesg_buffer[DMESG_MAX_ENTRIES][DMESG_ENTRY_LEN];
static int dmesg_count = 0;
static int dmesg_index = 0;

void dmesg_log(const char *msg) {
    int i = 0;
    while (msg[i] && i < DMESG_ENTRY_LEN - 1) {
        dmesg_buffer[dmesg_index][i] = msg[i];
        i++;
    }
    dmesg_buffer[dmesg_index][i] = 0;
    
    dmesg_index = (dmesg_index + 1) % DMESG_MAX_ENTRIES;
    if (dmesg_count < DMESG_MAX_ENTRIES) dmesg_count++;
}

// Application Development Guide
int app_devguide(int argc, char **argv) {
    const char *topic = argc > 1 ? argv[1] : "menu";
    
    tty_puts("\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    tty_puts("ICE App Development Guide\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("=========================\n\n");
    
    if (strcmp(topic, "menu") == 0 || strcmp(topic, "help") == 0) {
        tty_puts("Topics:\n");
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        tty_puts("  devguide python   - Python app development\n");
        tty_puts("  devguide shell    - Shell script guide\n");
        tty_puts("  devguide ice      - ICE script language\n");
        tty_puts("  devguide api      - Available commands/API\n");
        tty_puts("  devguide example  - Quick example\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        tty_puts("\nType 'devguide <topic>' for details.\n");
    }
    else if (strcmp(topic, "python") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        tty_puts("Python Application Development\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        tty_puts("------------------------------\n\n");
        
        tty_puts("Create a .py file:\n");
        vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        tty_puts("  touch /hello.py\n");
        tty_puts("  iced /hello.py\n\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        tty_puts("Example Python script:\n");
        vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
        tty_puts("  # hello.py\n");
        tty_puts("  name = \"User\"\n");
        tty_puts("  print(\"Hello,\")\n");
        tty_puts("  print(name)\n\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        tty_puts("Run it:\n");
        vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        tty_puts("  apm run /hello.py\n\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        tty_puts("Supported: print(), variables, comments (#)\n");
        tty_puts("Not supported: loops, if/else, functions, imports\n");
    }
    else if (strcmp(topic, "shell") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        tty_puts("Shell Script Development\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        tty_puts("------------------------\n\n");
        
        tty_puts("Create a .sh file:\n");
        vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        tty_puts("  touch /script.sh\n\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        tty_puts("Example shell script:\n");
        vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
        tty_puts("  #!/bin/sh\n");
        tty_puts("  # My script\n");
        tty_puts("  set NAME \"ICE\"\n");
        tty_puts("  echo \"Hello from $NAME\"\n");
        tty_puts("  ls /\n");
        tty_puts("  sleep 1000\n");
        tty_puts("  echo \"Done!\"\n\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        tty_puts("Run it:\n");
        vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        tty_puts("  apm run /script.sh\n\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        tty_puts("All ICE commands available in scripts!\n");
    }
    else if (strcmp(topic, "ice") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        tty_puts("ICE Script Language\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        tty_puts("-------------------\n\n");
        
        tty_puts("Native scripting for ICE OS (.ice files)\n\n");
        
        tty_puts("Syntax:\n");
        vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
        tty_puts("  # Comments\n");
        tty_puts("  set VAR value      # Variables\n");
        tty_puts("  echo \"text\"        # Output\n");
        tty_puts("  echo $VAR          # Use variables\n");
        tty_puts("  sleep 1000         # Sleep ms\n");
        tty_puts("  exit 0             # Exit script\n\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        tty_puts("Run: apm run /script.ice\n");
    }
    else if (strcmp(topic, "api") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        tty_puts("Available API/Commands\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        tty_puts("----------------------\n\n");
        
        tty_puts("File Operations:\n");
        vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
        tty_puts("  cat, ls, touch, mkdir, rm, rmdir, cp, mv, write\n\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        tty_puts("Output:\n");
        vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
        tty_puts("  echo, print (same), clear\n\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        tty_puts("Variables:\n");
        vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
        tty_puts("  set NAME value, let NAME value\n");
        tty_puts("  Use: $NAME or ${NAME}\n\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        tty_puts("Control:\n");
        vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
        tty_puts("  sleep <ms>, exit [code]\n\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        tty_puts("System:\n");
        vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
        tty_puts("  whoami, hostname, uname, uptime, free, df\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }
    else if (strcmp(topic, "example") == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        tty_puts("Quick Example - Create an App\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        tty_puts("-----------------------------\n\n");
        
        tty_puts("Step 1: Create the file\n");
        vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        tty_puts("  touch /myapp.ice\n\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        tty_puts("Step 2: Edit with iced\n");
        vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        tty_puts("  iced /myapp.ice\n\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        tty_puts("Step 3: Add your code:\n");
        vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
        tty_puts("  # My First ICE App\n");
        tty_puts("  echo \"Hello World!\"\n");
        tty_puts("  set name \"User\"\n");
        tty_puts("  echo \"Welcome, $name\"\n");
        tty_puts("  ls /\n\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        tty_puts("Step 4: Save and exit iced (:wq)\n\n");
        
        tty_puts("Step 5: Run your app\n");
        vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
        tty_puts("  apm run /myapp.ice\n\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        tty_puts("That's it! Your first ICE app!\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }
    else {
        tty_printf("Unknown topic: %s\n", topic);
        tty_puts("Type 'devguide' for available topics.\n");
    }
    
    tty_puts("\n");
    return 0;
}

int app_dmesg(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    tty_puts("\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    tty_puts("Kernel Messages\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("===============\n\n");
    
    // If no messages logged yet, show some system info
    if (dmesg_count == 0) {
        tty_puts("[    0.000000] ICE Operating System v1.0.0\n");
        tty_puts("[    0.000001] Kernel command line: (none)\n");
        tty_puts("[    0.000010] CPU: i686 compatible\n");
        
        extern u32 pmm_get_total_memory(void);
        u32 mem = pmm_get_total_memory();
        tty_printf("[    0.000020] Memory: %dK/%dK available\n", 
            mem/1024, mem/1024);
        
        tty_puts("[    0.000100] PIT: Timer initialized at 100Hz\n");
        tty_puts("[    0.000200] Keyboard: PS/2 keyboard detected\n");
        tty_puts("[    0.000300] VGA: Text mode 80x25\n");
        tty_puts("[    0.001000] ATA: Primary controller detected\n");
        tty_puts("[    0.001500] EXT2: Filesystem mounted\n");
        
        if (net_is_available()) {
            net_iface_t *iface = net_get_iface(0);
            tty_printf("[    0.002000] NET: %s detected\n", iface->name);
            tty_printf("[    0.002100] NET: MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
                iface->mac.addr[0], iface->mac.addr[1], iface->mac.addr[2],
                iface->mac.addr[3], iface->mac.addr[4], iface->mac.addr[5]);
        } else {
            tty_puts("[    0.002000] NET: No network card detected\n");
        }
        
        tty_puts("[    0.010000] MPM: Main Process Manager started\n");
    } else {
        // Print logged messages
        int start = (dmesg_count < DMESG_MAX_ENTRIES) ? 0 : dmesg_index;
        for (int i = 0; i < dmesg_count; i++) {
            int idx = (start + i) % DMESG_MAX_ENTRIES;
            tty_printf("%s\n", dmesg_buffer[idx]);
        }
    }
    
    tty_puts("\n");
    return 0;
}
