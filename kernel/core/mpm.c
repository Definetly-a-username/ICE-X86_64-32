 

#include "mpm.h"
#include "bootval.h"
#include "user.h"
#include "sysinfo.h"
#include "exc.h"
#include "../tty/tty.h"
#include "../tty/console.h"
#include "../drivers/vga.h"
#include "../drivers/pit.h"
#include "../drivers/keyboard.h"
#include "../mm/pmm.h"
#include "../apps/apps.h"
#include "../net/net.h"
#include "../gui/gui.h"
#include "../frost/frost.h"

#define ICE_VERSION "1.0.0"
#define MAX_INPUT 128
#define MAX_ARGS 16
#define MAX_PROCESSES 64

 
static process_t process_table[MAX_PROCESSES];
static int process_count = 0;
static ice_pid_t next_pid = 1;

 
static u64 boot_ticks = 0;

 
static int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

 
static int atoi(const char *s) {
    int n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return n;
}

 
static int parse_args(char *input, char **argv) {
    int argc = 0;
    char *p = input;
    
    while (*p && argc < MAX_ARGS - 1) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        
        argv[argc++] = p;
        
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    
    argv[argc] = 0;
    return argc;
}

 

static bool do_login(void) {
    char username[32];
    char password[32];
    
    for (int attempts = 0; attempts < 3; attempts++) {
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        tty_puts("login: ");
        tty_getline(username, sizeof(username));
        
        tty_puts("password: ");
         
        tty_getline(password, sizeof(password));
        
        uid_t uid = user_login(username, password);
        if (uid != UID_INVALID) {
            user_t *u = user_get_current();
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            tty_printf("\nWelcome, %s!\n", u->username);
            if (u->type == USER_TYPE_UPU) {
                tty_puts("(Upper Process User - Administrator)\n");
            }
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            return true;
        }
        
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        tty_puts("Login incorrect.\n\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }
    
    tty_puts("Too many failed attempts.\n");
    return false;
}

 

static void cmd_help(void) {
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    tty_puts("ICE Shell Commands:\n\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    tty_puts("  pm          Process Manager\n");
    tty_puts("  gpm         General Process Manager\n");
    tty_puts("  apps        List built-in applications\n");
    tty_puts("  frost/gui   Start Frost GUI layer\n");
    tty_puts("  netauto     Auto-configure network\n");
    tty_puts("  logout      Logout current user\n");
    tty_puts("  help        Show this help\n");
    tty_puts("  clear       Clear screen\n\n");
    
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    tty_puts("Type 'apps' to see available programs.\n");
    tty_puts("Type 'apm run <script>' to run scripts.\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

static void cmd_pm(int argc, char **argv) {
    if (argc < 2) {
        if (process_count == 0) {
            tty_puts("No running processes.\n");
            return;
        }
        
        tty_puts("PID       EXEC_ID   STATE     NAME\n");
        tty_puts("---       -------   -----     ----\n");
        
        for (int i = 0; i < process_count; i++) {
            if (process_table[i].state != PROC_STATE_OFF) {
                tty_printf("%-9d #%08X %-9s %s\n",
                    process_table[i].pid,
                    process_table[i].exec_id,
                    process_table[i].state == PROC_STATE_ON ? "ON" : "PAUSED",
                    process_table[i].name);
            }
        }
        return;
    }
    
    if (strcmp(argv[1], "exit") == 0) {
        tty_puts("Shutting down ICE...\n");
        __asm__ volatile ("cli; hlt");
    }
    else if (strcmp(argv[1], "kp") == 0) {
        if (argc < 3) {
            tty_puts("Usage: pm kp <pid>\n");
            return;
        }
        int pid = atoi(argv[2]);
        tty_printf("Process %d terminated.\n", pid);
    }
    else if (strcmp(argv[1], "rp") == 0) {
        if (argc < 3) {
            tty_puts("Usage: pm rp <pid>\n");
            return;
        }
        int pid = atoi(argv[2]);
        tty_printf("Process %d restarted.\n", pid);
    }
    else if (strcmp(argv[1], "val") == 0) {
        bootval_validate();
    }
    else {
        tty_printf("Unknown pm command: %s\n", argv[1]);
    }
}

static void cmd_gpm(int argc, char **argv) {
    if (argc < 2) {
        tty_puts("Usage: gpm <command>\n");
        tty_puts("Commands: info, version, uptime, mem, clear, col, net\n");
        return;
    }
    
    if (strcmp(argv[1], "info") == 0) {
        sysinfo_print();
    }
    else if (strcmp(argv[1], "version") == 0) {
        tty_puts("ICE Operating System v");
        tty_puts(ICE_VERSION);
        tty_puts("\nMPM (Main Process Manager) Kernel\n");
    }
    else if (strcmp(argv[1], "uptime") == 0) {
        u32 ticks = (u32)(pit_get_ticks() - boot_ticks);
        u32 secs = ticks / 100;
        u32 mins = secs / 60;
        u32 hours = mins / 60;
        
        tty_printf("Uptime: %d:%02d:%02d\n", hours, mins % 60, secs % 60);
    }
    else if (strcmp(argv[1], "mem") == 0) {
        u32 total = pmm_get_total_memory();
        u32 free = pmm_get_free_memory();
        u32 used = total - free;
        
        tty_puts("Memory Information:\n");
        tty_printf("  Total: %d KB (%d MB)\n", total/1024, total/1024/1024);
        tty_printf("  Used:  %d KB (%d MB)\n", used/1024, used/1024/1024);
        tty_printf("  Free:  %d KB (%d MB)\n", free/1024, free/1024/1024);
    }
    else if (strcmp(argv[1], "clear") == 0) {
        tty_clear();
    }
    else if (strcmp(argv[1], "col") == 0) {
        if (argc < 3) {
            tty_puts("Usage: gpm col <1-4>\n");
            return;
        }
        int scheme = atoi(argv[2]);
        if (scheme < 1 || scheme > 4) {
            tty_puts("Color scheme must be 1-4\n");
            return;
        }
        tty_set_color_scheme(scheme);
        tty_printf("Color scheme set to %d\n", scheme);
    }
    else if (strcmp(argv[1], "net") == 0) {
        net_iface_t *iface = net_get_iface(0);
        if (!net_is_available()) {
            tty_puts("No network interface detected.\n");
        } else {
            tty_puts("Network Interface:\n");
            tty_printf("  Name:   %s\n", iface->name);
            tty_printf("  MAC:    %02X:%02X:%02X:%02X:%02X:%02X\n",
                iface->mac.addr[0], iface->mac.addr[1], iface->mac.addr[2],
                iface->mac.addr[3], iface->mac.addr[4], iface->mac.addr[5]);
            tty_printf("  Status: %s\n", iface->up ? "UP" : "DOWN");
        }
    }
    else {
        tty_printf("Unknown gpm command: %s\n", argv[1]);
    }
}

 

void mpm_init(void) {
    boot_ticks = pit_get_ticks();
    process_count = 0;
    next_pid = 1;
    
     
    user_init();
    apps_init();
    console_init();
    exc_init();
    net_init();
}

void mpm_shell(void) {
    char input[MAX_INPUT];
    char *argv[MAX_ARGS];
    
    // Initialize GUI system
    gui_init();
    
    // Show boot menu to choose GUI or TTY mode
    gui_mode_t mode = gui_boot_menu();
    
    // Print login banner
    sysinfo_print_login_banner();
    
    // Attempt login
    while (!do_login()) {
        pit_sleep_ms(2000);
        sysinfo_print_login_banner();
    }
    
    // If GUI mode selected, run Frost GUI layer
    if (mode == GUI_MODE_GRAPHIC) {
        tty_puts("\nStarting Frost GUI layer...\n");
        pit_sleep_ms(500);
        frost_run();
        // When Frost exits, continue to TTY
        tty_puts("\nReturned to text mode.\n");
    }
    
    tty_puts("\nType 'help' for commands, 'apps' for programs.\n\n");
    
     
    while (1) {
        // Enhanced prompt with better colors and formatting
        user_t *u = user_get_current();
        if (u) {
            // Username with color coding
            if (u->type == USER_TYPE_UPU) {
                vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            } else {
                vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            }
            vga_printf("%s", u->username);
            
            // Separator
            vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
            vga_puts("@");
            
            // Hostname
            vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
            vga_puts("ice");
            
            // Current directory (simplified - always / for now)
            vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
            vga_puts(":");
            vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
            vga_puts("/");
            
            // Prompt symbol
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            if (u->type == USER_TYPE_UPU) {
                vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
                vga_puts("# ");
            } else {
                vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
                vga_puts("$ ");
            }
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        } else {
            // Fallback prompt
            vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
            vga_puts("ice");
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            vga_puts("> ");
        }
        
        if (tty_getline(input, sizeof(input)) == 0) {
            continue;
        }
        
        int argc = parse_args(input, argv);
        if (argc == 0) continue;
        
        // Add to command history
        add_to_history(input);
        
         
        if (apps_find(argv[0])) {
            apps_run(argv[0], argc, argv);
            continue;
        }
        
         
        if (strcmp(argv[0], "help") == 0) {
            cmd_help();
        }
        else if (strcmp(argv[0], "clear") == 0) {
            tty_clear();
        }
        else if (strcmp(argv[0], "pm") == 0) {
            cmd_pm(argc, argv);
        }
        else if (strcmp(argv[0], "gpm") == 0) {
            cmd_gpm(argc, argv);
        }
        else if (strcmp(argv[0], "apps") == 0) {
            apps_list();
        }
        else if (strcmp(argv[0], "logout") == 0) {
            user_logout();
            tty_puts("Logged out.\n\n");
            sysinfo_print_login_banner();
            while (!do_login()) {
                pit_sleep_ms(2000);
                sysinfo_print_login_banner();
            }
            tty_puts("\n");
        }
        else if (strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "halt") == 0) {
            tty_puts("Shutting down ICE...\n");
            __asm__ volatile ("cli; hlt");
        }
        else if (strcmp(argv[0], "gui") == 0 || strcmp(argv[0], "frost") == 0) {
            tty_puts("Starting Frost GUI...\n");
            frost_run();
            tty_puts("Returned to TTY mode.\n");
        }
        else if (strcmp(argv[0], "netauto") == 0 || strcmp(argv[0], "dhcp") == 0) {
            // Auto-configure network
            if (!net_is_available()) {
                vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
                tty_puts("No network interface available.\n");
                vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            } else {
                vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
                tty_puts("Auto-configuring network...\n");
                vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
                
                // Set a default IP (DHCP would be more complex)
                // Using link-local auto-configuration (169.254.x.x)
                net_set_ip(0, IP(169, 254, 1, 100), IP(255, 255, 0, 0));
                net_set_gateway(IP(169, 254, 1, 1));
                
                vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
                tty_puts("Network configured:\n");
                tty_puts("  IP: 169.254.1.100\n");
                tty_puts("  Netmask: 255.255.0.0\n");
                tty_puts("  Gateway: 169.254.1.1\n");
                vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            }
        }
        else {
            tty_printf("Unknown command: %s\n", argv[0]);
            tty_puts("Type 'help' for commands, 'apps' for programs.\n");
        }
    }
}

u32 mpm_get_uptime(void) {
    return ((u32)(pit_get_ticks() - boot_ticks)) / 100;
}

int mpm_get_process_count(void) {
    return process_count;
}

void mpm_get_memory_info(u32 *total, u32 *used, u32 *free) {
    *total = pmm_get_total_memory();
    *free = pmm_get_free_memory();
    *used = *total - *free;
}
