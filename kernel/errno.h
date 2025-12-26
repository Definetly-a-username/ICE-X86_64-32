#ifndef ICE_ERRNO_H
#define ICE_ERRNO_H

// Success
#define E_OK 0

// --- Generic System Errors (1 99) ---
#define E_GENERIC        -1
#define E_INVALID_ARG    -2
#define E_NO_MEM         -3
#define E_IO             -4
#define E_NOT_FOUND      -5
#define E_ACCESS         -6
#define E_BUSY           -7
#define E_EXISTS         -8
#define E_IS_DIR         -9
#define E_NOT_DIR        -10

// --- ATA / Disk Errors (100 199) ---
#define E_ATA_NO_DEV     -100
#define E_ATA_READ_ERR   -101
#define E_ATA_WRITE_ERR  -102
#define E_ATA_TIMEOUT    -103
#define E_ATA_INVALID    -104

// --- Ext2 Filesystem Errors (200 299) ---
#define E_EXT2_NOT_MOUNTED     -200
#define E_EXT2_BAD_MAGIC       -201
#define E_EXT2_READ_BLOCK      -202
#define E_EXT2_WRITE_BLOCK     -203
#define E_EXT2_NO_INODE        -204
#define E_EXT2_NO_BLOCK        -205
#define E_EXT2_SB_READ         -206
#define E_EXT2_SB_WRITE        -207
#define E_EXT2_BG_READ         -208
#define E_EXT2_BG_WRITE        -209
#define E_EXT2_DIR_FULL        -210
#define E_EXT2_FILE_EXISTS     -211
#define E_EXT2_FILE_NOT_FOUND  -212
#define E_EXT2_BAD_PATH        -213
#define E_EXT2_INODE_BITMAP    -214
#define E_EXT2_BLOCK_BITMAP    -215
#define E_EXT2_ROOT_READ       -216
#define E_EXT2_BAD_TYPE        -217

// --- GUI System Errors (300 399) ---
#define E_GUI_NOT_INIT         -300
#define E_GUI_FB_INIT          -301
#define E_GUI_FB_INVALID       -302
#define E_GUI_FB_NO_MEM        -303
#define E_GUI_WM_INIT          -304
#define E_GUI_DESKTOP_INIT     -305
#define E_GUI_MOUSE_INIT       -306
#define E_GUI_RENDER_INIT      -307
#define E_GUI_TIMEOUT          -308
#define E_GUI_INVALID_STATE    -309

// --- Framebuffer Errors (400 499) ---
#define E_FB_NOT_INIT          -400
#define E_FB_INVALID_ADDR      -401
#define E_FB_INVALID_SIZE      -402
#define E_FB_INVALID_BPP       -403
#define E_FB_VBE_FAILED        -404
#define E_FB_MULTIBOOT_FAILED  -405
#define E_FB_FALLBACK_FAILED   -406

// --- Window Manager Errors (500 599) ---
#define E_WM_NOT_INIT          -500
#define E_WM_MAX_WINDOWS       -501
#define E_WM_INVALID_WINDOW    -502
#define E_WM_INVALID_POS       -503
#define E_WM_INVALID_SIZE      -504

// --- Desktop Errors (600 699) ---
#define E_DESKTOP_NOT_INIT     -600
#define E_DESKTOP_MAX_ICONS    -601
#define E_DESKTOP_INVALID_ICON -602

// --- Render Errors (700 799) ---
#define E_RENDER_INVALID_CLIP  -700
#define E_RENDER_INVALID_COLOR -701
#define E_RENDER_BUFFER_NULL   -702

// --- Mouse Errors (800 899) ---
#define E_MOUSE_NOT_INIT       -800
#define E_MOUSE_INIT_FAILED    -801
#define E_MOUSE_TIMEOUT        -802
#define E_MOUSE_INVALID_PORT   -803

// --- Keyboard Errors (900 999) ---
#define E_KB_NOT_INIT          -900
#define E_KB_INIT_FAILED       -901
#define E_KB_BUFFER_FULL       -902

const char *error_string(int err);

#endif
