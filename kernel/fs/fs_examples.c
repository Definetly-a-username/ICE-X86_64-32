/**
 * Filesystem Example Usage Functions
 * 
 * Demonstrates how to use the EXT2/EXT4 filesystem drivers
 * for common operations like reading files, writing files,
 * and listing directories.
 */

#include "vfs.h"
#include "ext2.h"
#include "ext4.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"
#include "../errno.h"

// Simple strlen implementation
static u32 strlen(const char *s) {
    u32 len = 0;
    while (s[len]) len++;
    return len;
}

/**
 * Example: Read a file and print its contents
 * @param path Path to file
 * @return 0 on success, negative error code on failure
 */
int example_read_file(const char *path) {
    vfs_file_t *file = vfs_open(path);
    if (!file) {
        vga_printf("Failed to open file: %s\n", path);
        return E_NOT_FOUND;
    }
    
    // Get file size
    u32 size = vfs_get_file_size(path);
    if (size == 0) {
        vfs_close(file);
        vga_puts("File is empty\n");
        return E_OK;
    }
    
    // Allocate buffer
    char *buffer = (char*)0x100000; // Use a fixed address for kernel space
    if (size > 4096) {
        size = 4096; // Limit to 4KB for example
    }
    
    // Read file
    int bytes_read = vfs_read(file, buffer, size);
    if (bytes_read < 0) {
        vga_printf("Failed to read file: %s\n", error_string(bytes_read));
        vfs_close(file);
        return bytes_read;
    }
    
    // Print contents
    vga_printf("File contents (%d bytes):\n", bytes_read);
    for (u32 i = 0; i < bytes_read; i++) {
        if (buffer[i] >= 32 && buffer[i] < 127) {
            vga_putc(buffer[i]);
        } else if (buffer[i] == '\n') {
            vga_putc('\n');
        } else {
            vga_putc('.');
        }
    }
    vga_putc('\n');
    
    vfs_close(file);
    return E_OK;
}

/**
 * Example: Write data to a file
 * @param path Path to file
 * @param data Data to write
 * @param size Size of data
 * @return 0 on success, negative error code on failure
 */
int example_write_file(const char *path, const void *data, u32 size) {
    // Check if file exists, create if not
    if (!vfs_exists(path)) {
        int ret = vfs_create_file(path);
        if (ret < 0) {
            vga_printf("Failed to create file: %s\n", error_string(ret));
            return ret;
        }
    }
    
    // Open file
    vfs_file_t *file = vfs_open(path);
    if (!file) {
        vga_printf("Failed to open file: %s\n", path);
        return E_NOT_FOUND;
    }
    
    // Write data
    int bytes_written = vfs_write(file, data, size);
    if (bytes_written < 0) {
        vga_printf("Failed to write file: %s\n", error_string(bytes_written));
        vfs_close(file);
        return bytes_written;
    }
    
    vga_printf("Wrote %d bytes to %s\n", bytes_written, path);
    
    vfs_close(file);
    return E_OK;
}

/**
 * Example: List directory contents
 * @param path Path to directory
 * @return 0 on success, negative error code on failure
 */
// Callback function for directory listing
static void list_dir_callback(const char *name, u32 size, bool is_dir) {
    if (is_dir) {
        vga_printf("  [DIR]  %s\n", name);
    } else {
        vga_printf("  [FILE] %s (%d bytes)\n", name, size);
    }
}

int example_list_directory(const char *path) {
    vga_printf("Listing directory: %s\n", path);
    
    int count = vfs_list_dir(path, list_dir_callback);
    
    if (count < 0) {
        vga_printf("Failed to list directory: %s\n", error_string(count));
        return count;
    }
    
    vga_printf("Total entries: %d\n", count);
    return E_OK;
}

/**
 * Example: Create a test file with sample data
 * @return 0 on success, negative error code on failure
 */
int example_create_test_file(void) {
    const char *test_path = "/test.txt";
    const char *test_data = "Hello, World!\nThis is a test file.\n";
    u32 test_size = strlen(test_data);
    
    vga_puts("Creating test file...\n");
    
    int ret = example_write_file(test_path, test_data, test_size);
    if (ret < 0) {
        return ret;
    }
    
    vga_puts("Test file created successfully!\n");
    return E_OK;
}

/**
 * Example: Read and display a file
 * @param path Path to file
 * @return 0 on success, negative error code on failure
 */
int example_display_file(const char *path) {
    vga_printf("Displaying file: %s\n", path);
    return example_read_file(path);
}

/**
 * Example: Create a directory
 * @param path Path to directory
 * @return 0 on success, negative error code on failure
 */
int example_create_directory(const char *path) {
    vga_printf("Creating directory: %s\n", path);
    
    int ret = vfs_create_dir(path);
    if (ret < 0) {
        vga_printf("Failed to create directory: %s\n", error_string(ret));
        return ret;
    }
    
    vga_puts("Directory created successfully!\n");
    return E_OK;
}

/**
 * Example: Complete filesystem test
 * Tests reading, writing, and directory operations
 * @return 0 on success, negative error code on failure
 */
int example_filesystem_test(void) {
    vga_puts("\n=== Filesystem Test ===\n");
    
    // Test 1: List root directory
    vga_puts("\n1. Listing root directory:\n");
    example_list_directory("/");
    
    // Test 2: Create a test file
    vga_puts("\n2. Creating test file:\n");
    if (example_create_test_file() < 0) {
        return -1;
    }
    
    // Test 3: Read the test file
    vga_puts("\n3. Reading test file:\n");
    if (example_read_file("/test.txt") < 0) {
        return -1;
    }
    
    // Test 4: Create a directory
    vga_puts("\n4. Creating test directory:\n");
    if (example_create_directory("/testdir") < 0) {
        return -1;
    }
    
    // Test 5: List root again to see new entries
    vga_puts("\n5. Listing root directory again:\n");
    example_list_directory("/");
    
    vga_puts("\n=== Filesystem Test Complete ===\n");
    return E_OK;
}

