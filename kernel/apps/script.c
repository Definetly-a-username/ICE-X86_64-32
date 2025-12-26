/**
 * ICE Script Interpreter Implementation
 * 
 * Provides simple interpretation for multiple scripting languages.
 * This is a basic implementation focused on shell-like scripting.
 */

#include "script.h"
#include "apps.h"
#include "../tty/tty.h"
#include "../fs/vfs.h"
#include "../drivers/vga.h"
#include <string.h>
static bool str_starts_with(const char *str, const char *prefix) {
    size_t len = strlen(prefix);
    return strncmp(str, prefix, len) == 0;
}

static bool str_ends_with(const char *str, const char *suffix) {
    size_t str_len = strlen(str);
    size_t suf_len = strlen(suffix);
    
    if (suf_len > str_len) return false;
    
    return strcmp(str + str_len - suf_len, suffix) == 0;
}

// Language detection
script_type_t script_detect_type(const char *filename) {
    if (str_ends_with(filename, ".ice")) return SCRIPT_ICE;
    if (str_ends_with(filename, ".sh")) return SCRIPT_SHELL;
    if (str_ends_with(filename, ".py")) return SCRIPT_PYTHON;
    if (str_ends_with(filename, ".js")) return SCRIPT_JAVASCRIPT;
    if (str_ends_with(filename, ".lua")) return SCRIPT_LUA;
    if (str_ends_with(filename, ".bas")) return SCRIPT_BASIC;
    if (str_ends_with(filename, ".4th") || str_ends_with(filename, ".forth")) return SCRIPT_FORTH;
    if (str_ends_with(filename, ".lisp") || str_ends_with(filename, ".scm")) return SCRIPT_LISP;
    if (str_ends_with(filename, ".rb")) return SCRIPT_RUBY;
    if (str_ends_with(filename, ".pl")) return SCRIPT_PERL;
    if (str_ends_with(filename, ".tcl")) return SCRIPT_TCL;
    if (str_ends_with(filename, ".awk")) return SCRIPT_AWK;
    if (str_ends_with(filename, ".sed")) return SCRIPT_SED;
    if (str_ends_with(filename, ".bat") || str_ends_with(filename, ".cmd")) return SCRIPT_BATCH;
    if (str_ends_with(filename, ".conf") || str_ends_with(filename, ".ini") || 
        str_ends_with(filename, ".cfg")) return SCRIPT_CONFIG;
    
    return SCRIPT_UNKNOWN;
}

const char* script_type_name(script_type_t type) {
    switch (type) {
        case SCRIPT_ICE: return "ICE Script";
        case SCRIPT_SHELL: return "Shell Script";
        case SCRIPT_PYTHON: return "Python";
        case SCRIPT_JAVASCRIPT: return "JavaScript";
        case SCRIPT_LUA: return "Lua";
        case SCRIPT_BASIC: return "BASIC";
        case SCRIPT_FORTH: return "Forth";
        case SCRIPT_LISP: return "Lisp";
        case SCRIPT_RUBY: return "Ruby";
        case SCRIPT_PERL: return "Perl";
        case SCRIPT_TCL: return "Tcl";
        case SCRIPT_AWK: return "AWK";
        case SCRIPT_SED: return "SED";
        case SCRIPT_BATCH: return "Batch";
        case SCRIPT_CONFIG: return "Config";
        default: return "Unknown";
    }
}

// Variable management
int script_set_var(script_context_t *ctx, const char *name, const char *value) {
    // Check if variable exists
    for (int i = 0; i < ctx->var_count; i++) {
        if (strcmp(ctx->var_names[i], name) == 0) {
            strncpy(ctx->var_values[i], value, 256);
            return 0;
        }
    }
    
    // Create new variable
    if (ctx->var_count >= 64) return -1;
    
    strncpy(ctx->var_names[ctx->var_count], name, 32);
    strncpy(ctx->var_values[ctx->var_count], value, 256);
    ctx->var_count++;
    
    return 0;
}

const char* script_get_var(script_context_t *ctx, const char *name) {
    for (int i = 0; i < ctx->var_count; i++) {
        if (strcmp(ctx->var_names[i], name) == 0) {
            return ctx->var_values[i];
        }
    }
    return NULL;
}

void script_error(script_context_t *ctx, const char *msg) {
    ctx->error_count++;
    strncpy(ctx->error_msg, msg, 256);
    
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    tty_printf("Error at line %d: %s\n", ctx->line_num, msg);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

// Parse a line into arguments
static int parse_line(char *line, char **argv, int max_args) {
    int argc = 0;
    char *p = line;
    
    while (*p && argc < max_args - 1) {
        // Skip whitespace
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '#' || *p == '\n') break;
        
        // Handle quoted strings
        if (*p == '"' || *p == '\'') {
            char quote = *p++;
            argv[argc++] = p;
            while (*p && *p != quote) p++;
            if (*p) *p++ = 0;
        } else {
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
            if (*p) *p++ = 0;
        }
    }
    
    argv[argc] = NULL;
    return argc;
}

// Expand variables in a string
static void expand_vars(script_context_t *ctx, const char *src, char *dst, int max) {
    int i = 0, j = 0;
    
    while (src[i] && j < max - 1) {
        if (src[i] == '$') {
            // Variable reference
            i++;
            char var_name[32];
            int k = 0;
            
            // Handle ${var} syntax
            if (src[i] == '{') {
                i++;
                while (src[i] && src[i] != '}' && k < 31) {
                    var_name[k++] = src[i++];
                }
                if (src[i] == '}') i++;
            } else {
                // $var syntax
                while (src[i] && ((src[i] >= 'a' && src[i] <= 'z') || 
                       (src[i] >= 'A' && src[i] <= 'Z') ||
                       (src[i] >= '0' && src[i] <= '9') || src[i] == '_') && k < 31) {
                    var_name[k++] = src[i++];
                }
            }
            var_name[k] = 0;
            
            // Get variable value
            const char *value = script_get_var(ctx, var_name);
            if (value) {
                while (*value && j < max - 1) {
                    dst[j++] = *value++;
                }
            }
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = 0;
}

// Execute a single shell-like command
static int execute_command(script_context_t *ctx, int argc, char **argv) {
    if (argc == 0) return 0;
    
    // Built-in script commands
    if (strcmp(argv[0], "echo") == 0) {
        for (int i = 1; i < argc; i++) {
            if (i > 1) tty_puts(" ");
            tty_puts(argv[i]);
        }
        tty_puts("\n");
        return 0;
    }
    
    if (strcmp(argv[0], "print") == 0) {
        // Same as echo for compatibility
        for (int i = 1; i < argc; i++) {
            if (i > 1) tty_puts(" ");
            tty_puts(argv[i]);
        }
        tty_puts("\n");
        return 0;
    }
    
    if (strcmp(argv[0], "set") == 0 || strcmp(argv[0], "let") == 0) {
        // Variable assignment: set VAR value
        if (argc >= 3) {
            // Combine remaining args as value
            char value[256] = {0};
            int pos = 0;
            for (int i = 2; i < argc && pos < 255; i++) {
                if (i > 2 && pos < 254) value[pos++] = ' ';
                const char *s = argv[i];
                while (*s && pos < 255) value[pos++] = *s++;
            }
            value[pos] = 0;
            script_set_var(ctx, argv[1], value);
        }
        return 0;
    }
    
    if (strcmp(argv[0], "sleep") == 0) {
        if (argc >= 2) {
            int ms = 0;
            const char *s = argv[1];
            while (*s >= '0' && *s <= '9') {
                ms = ms * 10 + (*s - '0');
                s++;
            }
            extern void pit_sleep_ms(u32);
            pit_sleep_ms(ms);
        }
        return 0;
    }
    
    if (strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "quit") == 0) {
        ctx->exit_code = argc >= 2 ? (argv[1][0] - '0') : 0;
        return -1; // Signal to stop execution
    }
    
    if (strcmp(argv[0], "if") == 0) {
        // Simple if: if $var == value then command
        // We'll handle this in the main interpreter
        return 0;
    }
    
    // Try to run as built-in app
    builtin_app_t *app = apps_find(argv[0]);
    if (app) {
        return app->main(argc, argv);
    }
    
    // Unknown command
    script_error(ctx, "Unknown command");
    return 1;
}

// Main shell/ICE script interpreter
static int interpret_shell(script_context_t *ctx) {
    char line[512];
    char expanded_line[512];
    char *argv[32];
    int line_pos = 0;
    ctx->line_num = 0;
    
    const char *src = ctx->source;
    int src_len = ctx->source_len;
    int pos = 0;
    
    while (pos < src_len) {
        ctx->line_num++;
        line_pos = 0;
        
        // Read a line
        while (pos < src_len && src[pos] != '\n' && line_pos < 511) {
            line[line_pos++] = src[pos++];
        }
        line[line_pos] = 0;
        if (pos < src_len && src[pos] == '\n') pos++;
        
        // Skip empty lines and comments
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == 0 || *p == '#' || *p == '\n') continue;
        
        // Skip shebang
        if (ctx->line_num == 1 && str_starts_with(p, "#!")) continue;
        
        // Expand variables
        expand_vars(ctx, line, expanded_line, 512);
        
        // Parse and execute
        int argc = parse_line(expanded_line, argv, 32);
        if (argc > 0) {
            int ret = execute_command(ctx, argc, argv);
            if (ret < 0) break; // Exit requested
        }
    }
    
    return ctx->exit_code;
}

// Python-like interpreter (very basic subset)
static int interpret_python(script_context_t *ctx) {
    char line[512];
    char *argv[32];
    int line_pos = 0;
    ctx->line_num = 0;
    
    const char *src = ctx->source;
    int src_len = ctx->source_len;
    int pos = 0;
    
    while (pos < src_len) {
        ctx->line_num++;
        line_pos = 0;
        
        // Read a line
        while (pos < src_len && src[pos] != '\n' && line_pos < 511) {
            line[line_pos++] = src[pos++];
        }
        line[line_pos] = 0;
        if (pos < src_len && src[pos] == '\n') pos++;
        
        // Skip empty lines and comments
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == 0 || *p == '#' || *p == '\n') continue;
        
        // Handle Python-like syntax
        if (str_starts_with(p, "print(")) {
            // Extract string between parentheses
            p += 6;
            while (*p == ' ') p++;
            
            if (*p == '"' || *p == '\'') {
                char quote = *p++;
                while (*p && *p != quote) {
                    tty_printf("%c", *p++);
                }
            } else {
                // Print variable
                char var_name[32];
                int i = 0;
                while (*p && *p != ')' && i < 31) {
                    var_name[i++] = *p++;
                }
                var_name[i] = 0;
                
                const char *val = script_get_var(ctx, var_name);
                if (val) tty_puts(val);
            }
            tty_puts("\n");
        }
        else if (str_starts_with(p, "import ")) {
            // Ignore imports for now
            continue;
        }
        else {
            // Try variable assignment: var = value
            char *eq = p;
            while (*eq && *eq != '=') eq++;
            if (*eq == '=') {
                *eq = 0;
                char *var = p;
                char *val = eq + 1;
                
                // Trim whitespace
                while (*var == ' ') var++;
                char *end = eq - 1;
                while (end > var && (*end == ' ' || *end == '\t')) *end-- = 0;
                
                while (*val == ' ') val++;
                
                // Remove quotes if present
                int val_len = strlen(val);
                if (val_len > 0 && val[val_len-1] == '\n') val[--val_len] = 0;
                if (val_len >= 2 && (val[0] == '"' || val[0] == '\'')) {
                    val++;
                    val[val_len - 2] = 0;
                }
                
                script_set_var(ctx, var, val);
            }
        }
    }
    
    return ctx->exit_code;
}

// Basic interpreter (classic BASIC)
static int interpret_basic(script_context_t *ctx) {
    char line[512];
    ctx->line_num = 0;
    
    const char *src = ctx->source;
    int src_len = ctx->source_len;
    int pos = 0;
    
    while (pos < src_len) {
        ctx->line_num++;
        int line_pos = 0;
        
        while (pos < src_len && src[pos] != '\n' && line_pos < 511) {
            line[line_pos++] = src[pos++];
        }
        line[line_pos] = 0;
        if (pos < src_len && src[pos] == '\n') pos++;
        
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == 0 || *p == '\'' || str_starts_with(p, "REM")) continue;
        
        // Skip line number if present
        while (*p >= '0' && *p <= '9') p++;
        while (*p == ' ') p++;
        
        if (str_starts_with(p, "PRINT ") || str_starts_with(p, "print ")) {
            p += 6;
            while (*p == ' ') p++;
            
            if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    tty_printf("%c", *p++);
                }
            }
            tty_puts("\n");
        }
        else if (str_starts_with(p, "LET ") || str_starts_with(p, "let ")) {
            p += 4;
            while (*p == ' ') p++;
            
            char var_name[32];
            int i = 0;
            while (*p && *p != '=' && *p != ' ' && i < 31) {
                var_name[i++] = *p++;
            }
            var_name[i] = 0;
            
            while (*p == ' ' || *p == '=') p++;
            
            char value[256];
            i = 0;
            if (*p == '"') {
                p++;
                while (*p && *p != '"' && i < 255) {
                    value[i++] = *p++;
                }
            } else {
                while (*p && *p != '\n' && i < 255) {
                    value[i++] = *p++;
                }
            }
            value[i] = 0;
            
            script_set_var(ctx, var_name, value);
        }
        else if (str_starts_with(p, "END") || str_starts_with(p, "end")) {
            break;
        }
    }
    
    return ctx->exit_code;
}

int script_init_context(script_context_t *ctx, const char *filename) {
    ctx->filename = filename;
    ctx->type = script_detect_type(filename);
    ctx->source = NULL;
    ctx->source_len = 0;
    ctx->line_num = 0;
    ctx->error_count = 0;
    ctx->error_msg[0] = 0;
    ctx->var_count = 0;
    ctx->exit_code = 0;
    
    return 0;
}

int script_run_file(const char *filename) {
    if (!vfs_is_mounted()) {
        tty_puts("Script error: No filesystem mounted\n");
        return -1;
    }
    
    // Normalize path
    char path[256];
    if (filename[0] != '/') {
        path[0] = '/';
        int i = 1;
        while (filename[i-1] && i < 255) {
            path[i] = filename[i-1];
            i++;
        }
        path[i] = 0;
    } else {
        strncpy(path, filename, 256);
    }
    
    // Open and read file
    vfs_file_t *f = vfs_open(path);
    if (!f) {
        tty_printf("Script error: Cannot open '%s'\n", filename);
        return -1;
    }
    
    // Read file contents
    static char script_buffer[16384]; // 16KB max script size
    int total = 0;
    int n;
    
    while ((n = vfs_read(f, script_buffer + total, sizeof(script_buffer) - total - 1)) > 0) {
        total += n;
        if (total >= (int)sizeof(script_buffer) - 1) break;
    }
    script_buffer[total] = 0;
    
    vfs_close(f);
    
    // Initialize context
    script_context_t ctx;
    script_init_context(&ctx, filename);
    ctx.source = script_buffer;
    ctx.source_len = total;
    
    // Print execution info
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    tty_printf("Running %s script: %s\n", script_type_name(ctx.type), filename);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    tty_puts("---\n");
    
    // Execute based on type
    int result = 0;
    switch (ctx.type) {
        case SCRIPT_SHELL:
        case SCRIPT_ICE:
        case SCRIPT_BATCH:
            result = interpret_shell(&ctx);
            break;
        case SCRIPT_PYTHON:
            result = interpret_python(&ctx);
            break;
        case SCRIPT_BASIC:
            result = interpret_basic(&ctx);
            break;
        case SCRIPT_JAVASCRIPT:
        case SCRIPT_LUA:
        case SCRIPT_RUBY:
        case SCRIPT_PERL:
        case SCRIPT_TCL:
            // Use shell-like interpretation for now
            result = interpret_shell(&ctx);
            break;
        default:
            tty_puts("Unsupported language\n");
            return -1;
    }
    
    tty_puts("---\n");
    if (ctx.error_count > 0) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        tty_printf("Script finished with %d error(s)\n", ctx.error_count);
    } else {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        tty_printf("Script finished (exit code: %d)\n", result);
    }
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    return result;
}

int script_run_source(const char *source, script_type_t type) {
    script_context_t ctx;
    ctx.filename = "<inline>";
    ctx.type = type;
    ctx.source = (char*)source;
    ctx.source_len = strlen(source);
    ctx.line_num = 0;
    ctx.error_count = 0;
    ctx.var_count = 0;
    ctx.exit_code = 0;
    
    switch (type) {
        case SCRIPT_SHELL:
        case SCRIPT_ICE:
            return interpret_shell(&ctx);
        case SCRIPT_PYTHON:
            return interpret_python(&ctx);
        case SCRIPT_BASIC:
            return interpret_basic(&ctx);
        default:
            return interpret_shell(&ctx);
    }
}

