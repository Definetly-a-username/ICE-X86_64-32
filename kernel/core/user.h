/*
 * ICE Operating System - User Account System Header
 * UPU (Upper Process User) - Admin
 * PU (Process User) - Normal user
 */

#ifndef ICE_USER_H
#define ICE_USER_H

#include "../types.h"

/* User types */
typedef enum {
    USER_TYPE_PU = 0,       /* Process User (normal) */
    USER_TYPE_UPU,          /* Upper Process User (admin) */
} user_type_t;

/* User ID */
typedef u32 uid_t;
#define UID_INVALID 0
#define UID_ROOT    1       /* Default UPU */

/* Maximum values */
#define MAX_USERS       16
#define MAX_USERNAME    16
#define MAX_PASSWORD    32

/* User account structure */
typedef struct {
    uid_t uid;
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];  /* Hashed in production */
    user_type_t type;
    bool active;
    bool logged_in;
} user_t;

/* Initialize user system */
void user_init(void);

/* Create user account */
uid_t user_create(const char *username, const char *password, user_type_t type);

/* Authenticate user */
uid_t user_login(const char *username, const char *password);

/* Logout current user */
void user_logout(void);

/* Get current user */
user_t* user_get_current(void);

/* Get user by UID */
user_t* user_get(uid_t uid);

/* Check if current user is admin (UPU) */
bool user_is_admin(void);

/* List all users */
void user_list(void (*callback)(user_t *user));

/* Delete user (requires UPU) */
int user_delete(uid_t uid);

/* Change password */
int user_change_password(uid_t uid, const char *old_pw, const char *new_pw);

#endif /* ICE_USER_H */
