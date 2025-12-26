 

#include "apm.h"
#include "apps.h"
#include "script.h"
#include "../tty/tty.h"
#include "../drivers/vga.h"
#include "../fs/vfs.h"
#include "../core/exc.h"
#include "../core/user.h"
#include <string.h>
 
static apm_entry_t packages[MAX_PACKAGES];
static int package_count = 0;
static u32 next_package_id = 1;

 


 
static bool str_endswith(const char *s, const char *suffix) {
    int slen = strlen(s);
    int suflen = strlen(suffix);
    if (suflen > slen) return false;
    return strcmp(s + slen - suflen, suffix) == 0;
}

 
static const struct {
    const char *ext;
    apm_lang_t lang;
    const char *name;
    const char *desc;
} lang_table[] = {
    {".c",    LANG_C,       "C",        "C source file"},
    {".cpp",  LANG_CPP,     "C++",      "C++ source file"},
    {".cc",   LANG_CPP,     "C++",      "C++ source file"},
    {".cxx",  LANG_CPP,     "C++",      "C++ source file"},
    {".py",   LANG_PYTHON,  "Python",   "Python script"},
    {".asm",  LANG_ASM_X86, "ASM-x86",  "x86 assembly"},
    {".s",    LANG_ASM_X86, "ASM-x86",  "x86 assembly"},
    {".asm64",LANG_ASM_X64, "ASM-x64",  "x86-64 assembly"},
    {".rs",   LANG_RUST,    "Rust",     "Rust source file"},
    {".html", LANG_HTML,    "HTML",     "HTML document"},
    {".htm",  LANG_HTML,    "HTML",     "HTML document"},
    {".css",  LANG_CSS,     "CSS",      "CSS stylesheet"},
    {".js",   LANG_JS,      "JS",       "JavaScript"},
    {".go",   LANG_GOLANG,  "Go",       "Go source file"},
    {".exc",  LANG_EXC,     "EXC",      "ICE executable"},
    {".apm",  LANG_EXC,     "APM",      "APM package"},
    {0, LANG_UNKNOWN, 0, 0}
};

void apm_init(void) {
    package_count = 0;
    next_package_id = 1;
    
    for (int i = 0; i < MAX_PACKAGES; i++) {
        packages[i].installed = false;
    }
}

apm_lang_t apm_detect_lang(const char *filename) {
    for (int i = 0; lang_table[i].ext; i++) {
        if (str_endswith(filename, lang_table[i].ext)) {
            return lang_table[i].lang;
        }
    }
    return LANG_UNKNOWN;
}

const char* apm_lang_name(apm_lang_t lang) {
    for (int i = 0; lang_table[i].ext; i++) {
        if (lang_table[i].lang == lang) {
            return lang_table[i].name;
        }
    }
    return "Unknown";
}

int apm_install(const char *path) {
    if (!vfs_is_mounted()) {
        tty_puts("apm: No filesystem mounted.\n");
        return -1;
    }
    
     
    if (!str_endswith(path, ".apm")) {
        tty_puts("apm: Not a valid .apm package.\n");
        return -1;
    }
    
     
    vfs_file_t *f = vfs_open(path);
    if (!f) {
        tty_printf("apm: Package not found: %s\n", path);
        return -1;
    }
    
     
    apm_header_t header;
    int n = vfs_read(f, &header, sizeof(header));
    if (n < (int)sizeof(header)) {
        tty_puts("apm: Invalid package format.\n");
        vfs_close(f);
        return -1;
    }
    
     
    if (header.magic != APM_MAGIC) {
        tty_puts("apm: Invalid package magic.\n");
        vfs_close(f);
        return -1;
    }
    
     
    int slot = -1;
    for (int i = 0; i < MAX_PACKAGES; i++) {
        if (!packages[i].installed) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        tty_puts("apm: Package registry full.\n");
        vfs_close(f);
        return -1;
    }
    
     
    packages[slot].id = next_package_id++;
    strncpy(packages[slot].name, header.name, 32);
    strncpy(packages[slot].path, path, 64);
    packages[slot].lang = (apm_lang_t)header.lang;
    packages[slot].installed = true;
    packages[slot].size = header.code_size + header.data_size;
    
    package_count++;
    
    vfs_close(f);
    
    tty_printf("apm: Installed '%s' (%s)\n", header.name, apm_lang_name(packages[slot].lang));
    
    return 0;
}

int apm_setup(const char *source_path) {
    tty_printf("apm: Setting up from source: %s\n", source_path);
    
     
    apm_lang_t lang = apm_detect_lang(source_path);
    
    if (lang == LANG_UNKNOWN) {
        tty_puts("apm: Unknown source language.\n");
        tty_puts("     Supported: .c .cpp .py .asm .rs .html .css .js .go\n");
        return -1;
    }
    
    tty_printf("apm: Detected language: %s\n", apm_lang_name(lang));
    
     
    const char *name = source_path;
    for (const char *p = source_path; *p; p++) {
        if (*p == '/') name = p + 1;
    }
    
    char app_name[32];
    int i;
    for (i = 0; i < 31 && name[i] && name[i] != '.'; i++) {
        app_name[i] = name[i];
    }
    app_name[i] = '\0';
    
    tty_printf("apm: Creating executable: %s.exc\n", app_name);
    
     
    switch (lang) {
        case LANG_C:
        case LANG_CPP:
            tty_puts("apm: Compiling C/C++ source...\n");
             
            break;
            
        case LANG_ASM_X86:
        case LANG_ASM_X64:
            tty_puts("apm: Assembling x86 source...\n");
            break;
            
        case LANG_PYTHON:
            tty_puts("apm: Creating Python wrapper...\n");
             
            break;
            
        case LANG_RUST:
            tty_puts("apm: Compiling Rust source...\n");
            break;
            
        case LANG_GOLANG:
            tty_puts("apm: Compiling Go source...\n");
            break;
            
        case LANG_HTML:
        case LANG_CSS:
        case LANG_JS:
            tty_puts("apm: Bundling web application...\n");
             
            break;
            
        default:
            tty_puts("apm: Unsupported language for compilation.\n");
            return -1;
    }
    
     
    int slot = -1;
    for (i = 0; i < MAX_PACKAGES; i++) {
        if (!packages[i].installed) {
            slot = i;
            break;
        }
    }
    
    if (slot >= 0) {
        packages[slot].id = next_package_id++;
        strncpy(packages[slot].name, app_name, 32);
        strncpy(packages[slot].path, source_path, 64);
        packages[slot].lang = lang;
        packages[slot].installed = true;
        packages[slot].size = 0;
        package_count++;
    }
    
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    tty_printf("apm: Successfully set up '%s'\n", app_name);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_printf("     Run with: apm run %s\n", app_name);
    
    return 0;
}

int apm_run(const char *name, int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    // First check if it's a registered package
    apm_entry_t *pkg = apm_get(name);
    
    if (pkg) {
        tty_printf("apm: Running '%s' (%s)...\n", pkg->name, apm_lang_name(pkg->lang));
        
        switch (pkg->lang) {
            case LANG_C:
            case LANG_CPP:
            case LANG_ASM_X86:
            case LANG_ASM_X64:
            case LANG_RUST:
            case LANG_GOLANG:
            case LANG_EXC:
                tty_puts("[Native execution not implemented]\n");
                break;
                
            case LANG_PYTHON:
            case LANG_JS:
                // Use script interpreter
                return script_run_file(pkg->path);
                
            case LANG_HTML:
            case LANG_CSS:
                tty_puts("[Web runtime not implemented]\n");
                break;
                
            default:
                // Try script interpreter
                return script_run_file(pkg->path);
        }
        return 0;
    }
    
    // Not a registered package - try to run as script file directly
    // Check if it looks like a path or has extension
    bool is_path = false;
    const char *p = name;
    while (*p) {
        if (*p == '/' || *p == '.') {
            is_path = true;
            break;
        }
        p++;
    }
    
    if (is_path) {
        return script_run_file(name);
    }
    
    // Try with common extensions
    char path[256];
    const char *exts[] = {".ice", ".sh", ".py", ".lua", ".js", ".bas", NULL};
    
    for (int i = 0; exts[i]; i++) {
        // Build path: /<name><ext>
        int j = 0;
        path[j++] = '/';
        p = name;
        while (*p && j < 250) path[j++] = *p++;
        const char *ext = exts[i];
        while (*ext && j < 255) path[j++] = *ext++;
        path[j] = 0;
        
        if (vfs_exists(path)) {
            return script_run_file(path);
        }
    }
    
    tty_printf("apm: Cannot find '%s'\n", name);
    return -1;
}

void apm_list(void) {
    tty_puts("Installed Packages:\n\n");
    
    if (package_count == 0) {
        tty_puts("  (no packages installed)\n");
        return;
    }
    
    tty_puts("  ID   NAME                 LANG       SIZE\n");
    tty_puts("  --   ----                 ----       ----\n");
    
    for (int i = 0; i < MAX_PACKAGES; i++) {
        if (packages[i].installed) {
            tty_printf("  %-4d %-20s %-10s %d bytes\n",
                packages[i].id,
                packages[i].name,
                apm_lang_name(packages[i].lang),
                packages[i].size);
        }
    }
    
    tty_printf("\nTotal: %d packages\n", package_count);
}

int apm_remove(const char *name) {
    if (!user_is_admin()) {
        tty_puts("apm: Permission denied. Requires UPU.\n");
        return -1;
    }
    
    for (int i = 0; i < MAX_PACKAGES; i++) {
        if (packages[i].installed && strcmp(packages[i].name, name) == 0) {
            packages[i].installed = false;
            package_count--;
            tty_printf("apm: Removed '%s'\n", name);
            return 0;
        }
    }
    
    tty_printf("apm: Package '%s' not found.\n", name);
    return -1;
}

apm_entry_t* apm_get(const char *name) {
    for (int i = 0; i < MAX_PACKAGES; i++) {
        if (packages[i].installed && strcmp(packages[i].name, name) == 0) {
            return &packages[i];
        }
    }
    return 0;
}

 
int app_apm(int argc, char **argv) {
    if (argc < 2) {
        vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        tty_puts("APM - Application Process Manager\n\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        tty_puts("Usage: apm <command> [args]\n\n");
        tty_puts("Commands:\n");
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        tty_puts("  run <script>         Run a script file (.sh, .py, .ice, etc.)\n");
        tty_puts("  setup <source>       Build from source file\n");
        tty_puts("  install <file.apm>   Install APM package\n");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        tty_puts("  list                 List installed packages\n");
        tty_puts("  remove <app>         Remove package [UPU]\n");
        tty_puts("  info <app>           Show package info\n");
        tty_puts("  langs                List supported languages\n\n");
        tty_puts("Supported script types: .ice .sh .py .js .lua .bas .rb .pl\n");
        return 0;
    }
    
    if (strcmp(argv[1], "install") == 0) {
        if (argc < 3) {
            tty_puts("Usage: apm install <package.apm>\n");
            return 1;
        }
        return apm_install(argv[2]);
    }
    else if (strcmp(argv[1], "setup") == 0) {
        if (argc < 3) {
            tty_puts("Usage: apm setup <source_file>\n");
            return 1;
        }
        return apm_setup(argv[2]);
    }
    else if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            tty_puts("Usage: apm run <app_name> [args]\n");
            return 1;
        }
        return apm_run(argv[2], argc - 2, argv + 2);
    }
    else if (strcmp(argv[1], "list") == 0) {
        apm_list();
        return 0;
    }
    else if (strcmp(argv[1], "remove") == 0) {
        if (argc < 3) {
            tty_puts("Usage: apm remove <app_name>\n");
            return 1;
        }
        return apm_remove(argv[2]);
    }
    else if (strcmp(argv[1], "info") == 0) {
        if (argc < 3) {
            tty_puts("Usage: apm info <app_name>\n");
            return 1;
        }
        apm_entry_t *pkg = apm_get(argv[2]);
        if (!pkg) {
            tty_printf("apm: Package '%s' not found.\n", argv[2]);
            return 1;
        }
        tty_puts("Package Information:\n");
        tty_printf("  Name:     %s\n", pkg->name);
        tty_printf("  ID:       %d\n", pkg->id);
        tty_printf("  Language: %s\n", apm_lang_name(pkg->lang));
        tty_printf("  Path:     %s\n", pkg->path);
        tty_printf("  Size:     %d bytes\n", pkg->size);
        return 0;
    }
    else if (strcmp(argv[1], "langs") == 0) {
        tty_puts("Supported Languages:\n\n");
        tty_puts("  Extension  Language    Description\n");
        tty_puts("  ---------  --------    -----------\n");
        for (int i = 0; lang_table[i].ext; i++) {
            tty_printf("  %-10s %-11s %s\n",
                lang_table[i].ext, lang_table[i].name, lang_table[i].desc);
        }
        return 0;
    }
    else {
        tty_printf("apm: Unknown command '%s'\n", argv[1]);
        return 1;
    }
}
