/*
 * ICE Operating System - Built-in Applications Header
 */

#ifndef ICE_APPS_H
#define ICE_APPS_H

#include "../types.h"

/* App entry function type */
typedef int (*app_main_t)(int argc, char **argv);

/* Built-in app registration */
typedef struct {
    const char *name;
    const char *description;
    app_main_t main;
    bool requires_admin;
} builtin_app_t;

/* Initialize built-in apps */
void apps_init(void);

/* Run a built-in app by name */
int apps_run(const char *name, int argc, char **argv);

/* Get app by name */
builtin_app_t* apps_find(const char *name);

/* List all apps */
void apps_list(void);

/* Individual app entry points */
int app_cat(int argc, char **argv);
int app_echo(int argc, char **argv);
int app_iced(int argc, char **argv);
int app_ls(int argc, char **argv);
int app_mkdir(int argc, char **argv);
int app_rm(int argc, char **argv);
int app_pwd(int argc, char **argv);
int app_whoami(int argc, char **argv);
int app_users(int argc, char **argv);
int app_adduser(int argc, char **argv);
int app_passwd(int argc, char **argv);
int app_reboot(int argc, char **argv);
int app_shutdown(int argc, char **argv);
int app_date(int argc, char **argv);
int app_hexdump(int argc, char **argv);
int app_ip(int argc, char **argv);
int app_ping(int argc, char **argv);
int app_touch(int argc, char **argv);
int app_mkdir(int argc, char **argv);
int app_head(int argc, char **argv);
int app_tail(int argc, char **argv);
int app_wc(int argc, char **argv);
int app_env(int argc, char **argv);

#endif /* ICE_APPS_H */
