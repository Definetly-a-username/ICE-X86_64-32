 

#ifndef ICE_APPS_H
#define ICE_APPS_H

#include "../types.h"

 
typedef int (*app_main_t)(int argc, char **argv);

 
typedef struct {
    const char *name;
    const char *description;
    app_main_t main;
    bool requires_admin;
} builtin_app_t;

 
void apps_init(void);

 
int apps_run(const char *name, int argc, char **argv);

 
builtin_app_t* apps_find(const char *name);

 
void apps_list(void);

// File operations
int app_cat(int argc, char **argv);
int app_ls(int argc, char **argv);
int app_touch(int argc, char **argv);
int app_mkdir(int argc, char **argv);
int app_rm(int argc, char **argv);
int app_rmdir(int argc, char **argv);
int app_cp(int argc, char **argv);
int app_mv(int argc, char **argv);
int app_write(int argc, char **argv);
int app_stat(int argc, char **argv);
int app_head(int argc, char **argv);
int app_tail(int argc, char **argv);
int app_wc(int argc, char **argv);
int app_grep(int argc, char **argv);
int app_find(int argc, char **argv);

// Text utilities
int app_echo(int argc, char **argv);
int app_iced(int argc, char **argv);

// System information
int app_pwd(int argc, char **argv);
int app_whoami(int argc, char **argv);
int app_hostname(int argc, char **argv);
int app_uname(int argc, char **argv);
int app_date(int argc, char **argv);
int app_env(int argc, char **argv);
int app_df(int argc, char **argv);
int app_free(int argc, char **argv);
int app_hexdump(int argc, char **argv);
int app_history(int argc, char **argv);

// User management
int app_users(int argc, char **argv);
int app_adduser(int argc, char **argv);
int app_passwd(int argc, char **argv);

// Network
int app_ip(int argc, char **argv);
int app_ping(int argc, char **argv);
int app_netstat(int argc, char **argv);
int app_route(int argc, char **argv);
int app_arp(int argc, char **argv);

// System control
int app_reboot(int argc, char **argv);
int app_shutdown(int argc, char **argv);
int app_clear(int argc, char **argv);
int app_dmesg(int argc, char **argv);
void dmesg_log(const char *msg);

// Help
int app_help(int argc, char **argv);
int app_devguide(int argc, char **argv);

// Utility functions
void add_to_history(const char *cmd);
const char* get_hostname(void);

#endif  
