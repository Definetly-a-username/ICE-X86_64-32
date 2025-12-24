/*
 * ICE Operating System - User Account System
 */

#include "user.h"
#include "../drivers/vga.h"

/* User table */
static user_t users[MAX_USERS];
static int user_count = 0;
static uid_t next_uid = 1;
static uid_t current_uid = UID_INVALID;

/* String helpers */
static int str_len(const char *s) {
    int len = 0;
    while (*s++) len++;
    return len;
}

static void str_copy(char *dest, const char *src, int max) {
    int i;
    for (i = 0; i < max - 1 && src[i]; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
}

static int str_cmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

/* Simple hash for password (not cryptographically secure) */
static u32 simple_hash(const char *s) {
    u32 hash = 5381;
    while (*s) {
        hash = ((hash << 5) + hash) + *s++;
    }
    return hash;
}

void user_init(void) {
    /* Clear user table */
    for (int i = 0; i < MAX_USERS; i++) {
        users[i].active = false;
        users[i].uid = 0;
    }
    
    user_count = 0;
    next_uid = 1;
    current_uid = UID_INVALID;
    
    /* Create default root account (UPU) */
    user_create("root", "ice", USER_TYPE_UPU);
    
    /* Create default user account (PU) */
    user_create("user", "user", USER_TYPE_PU);
}

uid_t user_create(const char *username, const char *password, user_type_t type) {
    if (user_count >= MAX_USERS) return UID_INVALID;
    if (str_len(username) == 0 || str_len(username) >= MAX_USERNAME) return UID_INVALID;
    
    /* Check if username exists */
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].active && str_cmp(users[i].username, username) == 0) {
            return UID_INVALID;
        }
    }
    
    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < MAX_USERS; i++) {
        if (!users[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) return UID_INVALID;
    
    user_t *u = &users[slot];
    u->uid = next_uid++;
    str_copy(u->username, username, MAX_USERNAME);
    
    /* Store hashed password */
    u32 hash = simple_hash(password);
    char hash_str[12];
    for (int i = 0; i < 8; i++) {
        hash_str[i] = '0' + ((hash >> (i * 4)) & 0xF);
        if (hash_str[i] > '9') hash_str[i] = 'a' + (hash_str[i] - '0' - 10);
    }
    hash_str[8] = '\0';
    str_copy(u->password, hash_str, MAX_PASSWORD);
    
    u->type = type;
    u->active = true;
    u->logged_in = false;
    
    user_count++;
    
    return u->uid;
}

uid_t user_login(const char *username, const char *password) {
    /* Hash the input password */
    u32 hash = simple_hash(password);
    char hash_str[12];
    for (int i = 0; i < 8; i++) {
        hash_str[i] = '0' + ((hash >> (i * 4)) & 0xF);
        if (hash_str[i] > '9') hash_str[i] = 'a' + (hash_str[i] - '0' - 10);
    }
    hash_str[8] = '\0';
    
    /* Find user */
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].active && str_cmp(users[i].username, username) == 0) {
            if (str_cmp(users[i].password, hash_str) == 0) {
                /* Login successful */
                users[i].logged_in = true;
                current_uid = users[i].uid;
                return users[i].uid;
            }
            break;
        }
    }
    
    return UID_INVALID;
}

void user_logout(void) {
    if (current_uid != UID_INVALID) {
        for (int i = 0; i < MAX_USERS; i++) {
            if (users[i].uid == current_uid) {
                users[i].logged_in = false;
                break;
            }
        }
    }
    current_uid = UID_INVALID;
}

user_t* user_get_current(void) {
    if (current_uid == UID_INVALID) return 0;
    
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].uid == current_uid) {
            return &users[i];
        }
    }
    return 0;
}

user_t* user_get(uid_t uid) {
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].active && users[i].uid == uid) {
            return &users[i];
        }
    }
    return 0;
}

bool user_is_admin(void) {
    user_t *u = user_get_current();
    return u && u->type == USER_TYPE_UPU;
}

void user_list(void (*callback)(user_t *user)) {
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].active) {
            callback(&users[i]);
        }
    }
}

int user_delete(uid_t uid) {
    if (!user_is_admin()) return -1;
    if (uid == UID_ROOT) return -1;  /* Can't delete root */
    if (uid == current_uid) return -1;  /* Can't delete self */
    
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].uid == uid) {
            users[i].active = false;
            user_count--;
            return 0;
        }
    }
    return -1;
}

int user_change_password(uid_t uid, const char *old_pw, const char *new_pw) {
    user_t *u = user_get(uid);
    if (!u) return -1;
    
    /* Verify old password */
    u32 old_hash = simple_hash(old_pw);
    char hash_str[12];
    for (int i = 0; i < 8; i++) {
        hash_str[i] = '0' + ((old_hash >> (i * 4)) & 0xF);
        if (hash_str[i] > '9') hash_str[i] = 'a' + (hash_str[i] - '0' - 10);
    }
    hash_str[8] = '\0';
    
    if (str_cmp(u->password, hash_str) != 0) {
        return -1;  /* Wrong old password */
    }
    
    /* Set new password */
    u32 new_hash = simple_hash(new_pw);
    for (int i = 0; i < 8; i++) {
        hash_str[i] = '0' + ((new_hash >> (i * 4)) & 0xF);
        if (hash_str[i] > '9') hash_str[i] = 'a' + (hash_str[i] - '0' - 10);
    }
    hash_str[8] = '\0';
    str_copy(u->password, hash_str, MAX_PASSWORD);
    
    return 0;
}
