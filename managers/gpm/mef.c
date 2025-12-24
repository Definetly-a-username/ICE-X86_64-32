/*
 * ICE Operating System - Make Executable File (MEF)
 * Creates and registers executables from source files
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>

#include "../../mpm/core/mpm.h"
#include "../../EXC/format/exc.h"
#include "../../EXC/registry/registry.h"

#define BIN_DIR "/home/delta/basement/ice/bin"

/* ============================================================================
 * ANIMATION
 * ============================================================================ */

static void animate_progress(void) {
    const char *frames[] = {"-", "\\", "|", "/"};
    int num_frames = 4;
    
    for (int i = 0; i < 8; i++) {
        printf("\r[%s]", frames[i % num_frames]);
        fflush(stdout);
        usleep(100000); /* 100ms */
    }
    printf("\r   \r");
    fflush(stdout);
}

/* ============================================================================
 * LANGUAGE DETECTION
 * ============================================================================ */

typedef enum {
    LANG_UNKNOWN = 0,
    LANG_C,
    LANG_PYTHON
} source_lang_t;

static source_lang_t detect_language(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return LANG_UNKNOWN;
    
    if (strcmp(ext, ".c") == 0) return LANG_C;
    if (strcmp(ext, ".py") == 0) return LANG_PYTHON;
    
    return LANG_UNKNOWN;
}

static const char* lang_to_string(source_lang_t lang) {
    switch (lang) {
        case LANG_C:      return "C";
        case LANG_PYTHON: return "Python";
        default:          return "Unknown";
    }
}

/* ============================================================================
 * COMPILATION
 * ============================================================================ */

static int compile_c(const char *source, const char *output) {
    pid_t pid = fork();
    
    if (pid == 0) {
        /* Child: execute compiler */
        execlp("gcc", "gcc", "-o", output, source, 
               "-I/home/delta/basement/ice",
               "-L/home/delta/basement/ice/bin",
               NULL);
        exit(127);
    } else if (pid > 0) {
        /* Parent: wait */
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
    }
    
    return -1;
}

static int wrap_python(const char *source, const char *output) {
    /* Create a wrapper script that invokes python */
    FILE *fp = fopen(output, "w");
    if (!fp) return -1;
    
    fprintf(fp, "#!/usr/bin/env python3\n");
    fprintf(fp, "# ICE Python Wrapper\n");
    fprintf(fp, "exec(open('%s').read())\n", source);
    fclose(fp);
    
    chmod(output, 0755);
    return 0;
}

/* ============================================================================
 * MEF MAIN
 * ============================================================================ */

int mef_main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: gpm mef <path/to/source>\n");
        return 1;
    }
    
    const char *source_path = argv[1];
    
    /* Step 1: Validate path */
    printf("Path accessed\n");
    
    if (access(source_path, R_OK) != 0) {
        fprintf(stderr, "Error: Cannot access file: %s\n", source_path);
        return 1;
    }
    
    /* Step 2: Check source exists */
    printf("Source found\n");
    
    /* Step 3: Detect language */
    source_lang_t lang = detect_language(source_path);
    if (lang == LANG_UNKNOWN) {
        fprintf(stderr, "Error: Unknown source language. Supported: C, Python\n");
        return 1;
    }
    
    /* Step 4: Read source */
    printf("Reading source\n");
    animate_progress();
    
    FILE *fp = fopen(source_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot read source file\n");
        return 1;
    }
    fclose(fp);
    
    printf("Source read\n");
    
    /* Step 5: Execute MEF */
    printf("Executing MEF\n");
    animate_progress();
    
    /* Create output filename */
    const char *basename = strrchr(source_path, '/');
    basename = basename ? basename + 1 : source_path;
    
    char output_name[256];
    strncpy(output_name, basename, sizeof(output_name) - 1);
    char *dot = strrchr(output_name, '.');
    if (dot) *dot = '\0';
    
    char output_path[512];
    snprintf(output_path, sizeof(output_path), "%s/%s.exc", BIN_DIR, output_name);
    
    /* Ensure bin directory exists */
    mkdir(BIN_DIR, 0755);
    
    /* Compile or wrap */
    int result;
    if (lang == LANG_C) {
        result = compile_c(source_path, output_path);
    } else {
        result = wrap_python(source_path, output_path);
    }
    
    if (result != 0) {
        fprintf(stderr, "Error: Compilation failed\n");
        return 1;
    }
    
    /* Step 6: Register executable */
    mpm_request_t req = {
        .type = API_EXEC_REGISTER,
        .caller = CALLER_GPM,
        .params.exec.flags = EXC_FLAG_NONE
    };
    strncpy(req.params.exec.path, output_path, MAX_PATH_LEN - 1);
    
    mpm_response_t resp = mpm_process_request(&req);
    
    if (resp.error != MPM_OK) {
        fprintf(stderr, "Error: Failed to register executable: %s\n",
                mpm_error_string(resp.error));
        return 1;
    }
    
    /* Step 7: Output success */
    printf("Success\n");
    printf("ID " EXEC_ID_FORMAT "\n", resp.data.exec.exec_id);
    
    return 0;
}
