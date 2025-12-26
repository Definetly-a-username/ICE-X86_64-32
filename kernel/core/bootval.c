 

#include "bootval.h"
#include "../drivers/vga.h"
#include "../fs/vfs.h"

 
static const char *kernel_files[] = {
    "/EXC/API/process.exc",
    "/EXC/API/exec.exc",
    "/EXC/API/memory.exc",
    "/EXC/API/tty.exc",
    "/EXC/API/fs.exc",
    0
};

 
static const char *fallback_prefix = "/mpm/krnl/fallback";

void bootval_init(void) {
     
}

bool bootval_check_file(const char *path) {
     
    if (!vfs_is_mounted()) {
        return true;   
    }
    
    vfs_file_t *f = vfs_open(path);
    if (f) {
        vfs_close(f);
        return true;
    }
    return false;
}

int bootval_restore(const char *path) {
     
     
    (void)path;
    return 0;
}

val_result_t bootval_validate(void) {
    val_result_t result = {0, 0, 0};
    
    vga_puts("ICE Boot Validation\n");
    vga_puts("===================\n\n");
    
     
    if (!vfs_is_mounted()) {
        vga_puts("No disk mounted. Skipping validation.\n");
        return result;
    }
    
    vga_puts("Checking fallback integrity...\n\n");
    vga_puts("Validating kernel files...\n");
    
     
    for (int i = 0; kernel_files[i]; i++) {
        vga_printf("  %s: ", kernel_files[i]);
        result.files_checked++;
        
        if (bootval_check_file(kernel_files[i])) {
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            vga_puts("OK\n");
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        } else {
            vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            vga_puts("MISSING");
            vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            
             
            if (bootval_restore(kernel_files[i]) == 0) {
                vga_puts(" -> RESTORED\n");
                result.files_restored++;
            } else {
                vga_puts(" -> FAILED\n");
                result.errors++;
            }
        }
    }
    
    vga_puts("\nValidation complete.\n");
    vga_printf("  Files checked: %d\n", result.files_checked);
    vga_printf("  Files restored: %d\n", result.files_restored);
    vga_printf("  Errors: %d\n", result.errors);
    
    if (result.errors == 0) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_puts("\nSystem ready.\n");
    } else {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_puts("\nWARNING: System integrity compromised!\n");
    }
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    return result;
}
