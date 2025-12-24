/*
 * ICE Operating System - System Info (Neofetch-style)
 */

#include "sysinfo.h"
#include "../drivers/vga.h"
#include "../drivers/pit.h"
#include "../mm/pmm.h"
#include "user.h"

/* ICE ASCII Logo */
static const char *ice_logo[] = {
    "  \033[36m██╗ ██████╗███████╗\033[0m",
    "  \033[36m██║██╔════╝██╔════╝\033[0m",
    "  \033[36m██║██║     █████╗  \033[0m",
    "  \033[36m██║██║     ██╔══╝  \033[0m",
    "  \033[36m██║╚██████╗███████╗\033[0m",
    "  \033[36m╚═╝ ╚═════╝╚══════╝\033[0m",
    0
};

/* Simple VGA color logo (no ANSI) */
static const char *ice_logo_vga[] = {
    "   ##  ####  #####",
    "   ##  ##    ##   ",
    "   ##  ##    #### ",
    "   ##  ##    ##   ",
    "   ##  ####  #####",
    0
};

void sysinfo_print(void) {
    u32 total_mem = pmm_get_total_memory();
    u32 free_mem = pmm_get_free_memory();
    u32 used_mem = total_mem - free_mem;
    
    u32 ticks = (u32)pit_get_ticks();
    u32 secs = ticks / 100;
    u32 mins = secs / 60;
    u32 hours = mins / 60;
    
    user_t *user = user_get_current();
    const char *username = user ? user->username : "guest";
    const char *usertype = (user && user->type == USER_TYPE_UPU) ? "UPU" : "PU";
    
    /* Print logo on left, info on right */
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    
    /* Line 1: Logo + User@Host */
    vga_puts("\n");
    vga_puts("   ##  ####  #####   ");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_printf("%s", username);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("@");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("ice");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("\n");
    
    /* Line 2: Logo + Separator */
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("   ##  ##    ##      ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("-----------------\n");
    
    /* Line 3: Logo + OS */
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("   ##  ##    ####    ");
    vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    vga_puts("OS: ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("ICE 1.0.0\n");
    
    /* Line 4: Logo + Kernel */
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("   ##  ##    ##      ");
    vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    vga_puts("Kernel: ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("MPM 1.0\n");
    
    /* Line 5: Logo + Uptime */
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("   ##  ####  #####   ");
    vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    vga_puts("Uptime: ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_printf("%d hours, %d mins\n", hours, mins % 60);
    
    /* Line 6: Shell */
    vga_puts("                     ");
    vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    vga_puts("Shell: ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("ice-shell\n");
    
    /* Line 7: Terminal */
    vga_puts("                     ");
    vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    vga_puts("Terminal: ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("tty0 (80x25)\n");
    
    /* Line 8: CPU */
    vga_puts("                     ");
    vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    vga_puts("CPU: ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("x86 (Protected Mode)\n");
    
    /* Line 9: Memory */
    vga_puts("                     ");
    vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    vga_puts("Memory: ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_printf("%d MB / %d MB\n", used_mem/1024/1024, total_mem/1024/1024);
    
    /* Line 10: User type */
    vga_puts("                     ");
    vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    vga_puts("User: ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_printf("%s (%s)\n", username, usertype);
    
    /* Color bar */
    vga_puts("\n                     ");
    vga_set_color(VGA_COLOR_BLACK, VGA_COLOR_BLACK);
    vga_puts("   ");
    vga_set_color(VGA_COLOR_RED, VGA_COLOR_RED);
    vga_puts("   ");
    vga_set_color(VGA_COLOR_GREEN, VGA_COLOR_GREEN);
    vga_puts("   ");
    vga_set_color(VGA_COLOR_BROWN, VGA_COLOR_BROWN);
    vga_puts("   ");
    vga_set_color(VGA_COLOR_BLUE, VGA_COLOR_BLUE);
    vga_puts("   ");
    vga_set_color(VGA_COLOR_MAGENTA, VGA_COLOR_MAGENTA);
    vga_puts("   ");
    vga_set_color(VGA_COLOR_CYAN, VGA_COLOR_CYAN);
    vga_puts("   ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_LIGHT_GREY);
    vga_puts("   ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("\n\n");
}

void sysinfo_print_login_banner(void) {
    vga_clear();
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    
    vga_puts("\n\n");
    vga_puts("        ██╗ ██████╗███████╗\n");
    vga_puts("        ██║██╔════╝██╔════╝\n");
    vga_puts("        ██║██║     █████╗  \n");
    vga_puts("        ██║██║     ██╔══╝  \n");
    vga_puts("        ██║╚██████╗███████╗\n");
    vga_puts("        ╚═╝ ╚═════╝╚══════╝\n");
    
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("\n");
    vga_puts("        ICE Operating System v1.0.0\n");
    vga_puts("        Main Process Manager Kernel\n");
    vga_puts("\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_puts("        UPU: root / PU: user\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("\n");
}
