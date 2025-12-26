/**
 * ICE Script Interpreter
 * 
 * Simple script/source code interpreter supporting multiple languages.
 * Provides basic execution capabilities for interpreted languages.
 */

#ifndef ICE_SCRIPT_H
#define ICE_SCRIPT_H

#include "../types.h"

// Script types (separate from apm_lang_t to avoid conflicts)
typedef enum {
    SCRIPT_UNKNOWN = 0,
    SCRIPT_ICE,         // Native ICE scripting language (.ice)
    SCRIPT_SHELL,       // Shell script (.sh)
    SCRIPT_PYTHON,      // Python-like (.py)
    SCRIPT_JAVASCRIPT,  // JavaScript-like (.js)
    SCRIPT_LUA,         // Lua-like (.lua)
    SCRIPT_BASIC,       // BASIC (.bas)
    SCRIPT_FORTH,       // Forth (.4th)
    SCRIPT_LISP,        // Lisp-like (.lisp)
    SCRIPT_RUBY,        // Ruby-like (.rb)
    SCRIPT_PERL,        // Perl-like (.pl)
    SCRIPT_TCL,         // Tcl-like (.tcl)
    SCRIPT_AWK,         // AWK-like (.awk)
    SCRIPT_SED,         // SED-like (.sed)
    SCRIPT_BATCH,       // Batch/CMD (.bat)
    SCRIPT_CONFIG,      // Config files (.conf, .ini)
    SCRIPT_TYPE_COUNT
} script_type_t;

// Script execution context
typedef struct {
    script_type_t type;
    const char *filename;
    char *source;
    u32 source_len;
    int line_num;
    int error_count;
    char error_msg[256];
    
    // Variables (simple key-value store)
    char var_names[64][32];
    char var_values[64][256];
    int var_count;
    
    // Return value
    int exit_code;
} script_context_t;

// Detect language from file extension
script_type_t script_detect_type(const char *filename);

// Get language name
const char* script_type_name(script_type_t type);

// Initialize script context
int script_init_context(script_context_t *ctx, const char *filename);

// Execute a script file
int script_run_file(const char *filename);

// Execute script from memory
int script_run_source(const char *source, script_type_t type);

// Variable management
int script_set_var(script_context_t *ctx, const char *name, const char *value);
const char* script_get_var(script_context_t *ctx, const char *name);

// Error handling
void script_error(script_context_t *ctx, const char *msg);

#endif // ICE_SCRIPT_H
