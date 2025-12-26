#include "errno.h"

const char *error_string(int err) {
    switch (err) {
        case E_OK: return "Success";
        case E_GENERIC: return "Generic error";
        case E_INVALID_ARG: return "Invalid argument";
        case E_NO_MEM: return "Out of memory";
        case E_IO: return "I/O error";
        case E_NOT_FOUND: return "Not found";
        case E_ACCESS: return "Access denied";
        case E_BUSY: return "Resource busy";
        case E_EXISTS: return "Already exists";
        case E_IS_DIR: return "Is a directory";
        case E_NOT_DIR: return "Not a directory";
        
        case E_ATA_NO_DEV: return "ATA: No device";
        case E_ATA_READ_ERR: return "ATA: Read error";
        case E_ATA_WRITE_ERR: return "ATA: Write error";
        
        case E_EXT2_NOT_MOUNTED: return "Ext2: Not mounted";
        case E_EXT2_BAD_MAGIC: return "Ext2: Bad magic";
        case E_EXT2_READ_BLOCK: return "Ext2: Block read failed";
        case E_EXT2_WRITE_BLOCK: return "Ext2: Block write failed";
        case E_EXT2_NO_INODE: return "Ext2: No free inodes";
        case E_EXT2_NO_BLOCK: return "Ext2: No free blocks";
        case E_EXT2_SB_READ: return "Ext2: Superblock read failed";
        case E_EXT2_DIR_FULL: return "Ext2: Directory full";
        case E_EXT2_FILE_EXISTS: return "Ext2: File exists";
        case E_EXT2_FILE_NOT_FOUND: return "Ext2: File not found";
        
        case E_GUI_NOT_INIT: return "GUI: Not initialized";
        case E_GUI_FB_INIT: return "GUI: Framebuffer init failed";
        case E_GUI_FB_INVALID: return "GUI: Invalid framebuffer";
        case E_GUI_FB_NO_MEM: return "GUI: No framebuffer memory";
        case E_GUI_WM_INIT: return "GUI: Window manager init failed";
        case E_GUI_DESKTOP_INIT: return "GUI: Desktop init failed";
        case E_GUI_MOUSE_INIT: return "GUI: Mouse init failed";
        case E_GUI_RENDER_INIT: return "GUI: Render init failed";
        case E_GUI_TIMEOUT: return "GUI: Operation timeout";
        case E_GUI_INVALID_STATE: return "GUI: Invalid state";
        
        case E_FB_NOT_INIT: return "Framebuffer: Not initialized";
        case E_FB_INVALID_ADDR: return "Framebuffer: Invalid address";
        case E_FB_INVALID_SIZE: return "Framebuffer: Invalid size";
        case E_FB_INVALID_BPP: return "Framebuffer: Invalid BPP";
        case E_FB_VBE_FAILED: return "Framebuffer: VBE failed";
        case E_FB_MULTIBOOT_FAILED: return "Framebuffer: Multiboot failed";
        case E_FB_FALLBACK_FAILED: return "Framebuffer: Fallback failed";
        
        case E_WM_NOT_INIT: return "Window Manager: Not initialized";
        case E_WM_MAX_WINDOWS: return "Window Manager: Max windows reached";
        case E_WM_INVALID_WINDOW: return "Window Manager: Invalid window";
        case E_WM_INVALID_POS: return "Window Manager: Invalid position";
        case E_WM_INVALID_SIZE: return "Window Manager: Invalid size";
        
        case E_DESKTOP_NOT_INIT: return "Desktop: Not initialized";
        case E_DESKTOP_MAX_ICONS: return "Desktop: Max icons reached";
        case E_DESKTOP_INVALID_ICON: return "Desktop: Invalid icon";
        
        case E_RENDER_INVALID_CLIP: return "Render: Invalid clip";
        case E_RENDER_INVALID_COLOR: return "Render: Invalid color";
        case E_RENDER_BUFFER_NULL: return "Render: Buffer null";
        
        case E_MOUSE_NOT_INIT: return "Mouse: Not initialized";
        case E_MOUSE_INIT_FAILED: return "Mouse: Init failed";
        case E_MOUSE_TIMEOUT: return "Mouse: Timeout";
        case E_MOUSE_INVALID_PORT: return "Mouse: Invalid port";
        
        case E_KB_NOT_INIT: return "Keyboard: Not initialized";
        case E_KB_INIT_FAILED: return "Keyboard: Init failed";
        case E_KB_BUFFER_FULL: return "Keyboard: Buffer full";
        
        default: return "Unknown error";
    }
}
