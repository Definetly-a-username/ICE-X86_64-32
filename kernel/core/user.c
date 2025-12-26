 

#include "user.h"
#include "../drivers/vga.h"
#include <string.h>

 
static user_t users[MAX_USERS];
static int user_count = 0;
static uid_t next_uid = 1;
static uid_t current_uid = UID_INVALID;

 


 
// Rotate left helper
static inline u32 rotl(u32 x, int n) {
    return (x << n) | (x >> (32 - n));
}

// Salted hash function (using uid as salt context)
static u32 hash_password(const char *s, uid_t salt_uid) {
    u32 hash = 0xDEADBEEF;
    hash ^= salt_uid; // Salt with UID
    
    while (*s) {
        hash = rotl(hash, 5) ^ *s++;
        hash = (hash * 1664525) + 1013904223; // LCG mixing
    }
    return hash;
}

void user_init(void) {
     
    for (int i = 0; i < MAX_USERS; i++) {
        users[i].active = false;
        users[i].uid = 0;
    }
    
    user_count = 0;
    next_uid = 1;
    current_uid = UID_INVALID;
    
     
    user_create("root", "ice", USER_TYPE_UPU);
    
     
    user_create("user", "user", USER_TYPE_PU);
}

uid_t user_create(const char *username, const char *password, user_type_t type) {
    if (user_count >= MAX_USERS) return UID_INVALID;
    if (strlen(username) == 0 || strlen(username) >= MAX_USERNAME) return UID_INVALID;
    if (strlen(password) == 0 || strlen(password) >= 64) return UID_INVALID; // Max password input limit
    
     
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].active && strcmp(users[i].username, username) == 0) {
            return UID_INVALID;
        }
    }
    
     
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
    strncpy(u->username, username, MAX_USERNAME);
    u->username[MAX_USERNAME - 1] = '\0';
    
     
    u32 hash = hash_password(password, u->uid);
    char hash_str[12];
    for (int i = 0; i < 8; i++) {
        hash_str[i] = '0' + ((hash >> (i * 4)) & 0xF);
        if (hash_str[i] > '9') hash_str[i] = 'a' + (hash_str[i] - '0' - 10);
    }
    hash_str[8] = '\0';
    strncpy(u->password, hash_str, MAX_PASSWORD);
    u->password[MAX_PASSWORD - 1] = '\0';
    
    u->type = type;
    u->active = true;
    u->logged_in = false;
    
    user_count++;
    
    return u->uid;
}

uid_t user_login(const char *username, const char *password) {
    if (strlen(username) >= MAX_USERNAME || strlen(password) >= 64) return UID_INVALID;
     
    // Find user first to get salt (UID)
    user_t *target_user = 0;
    for (int i = 0; i < MAX_USERS; i++) {
        if (users[i].active && strcmp(users[i].username, username) == 0) {
            target_user = &users[i];
            break;
        }
    }
    
    if (!target_user) return UID_INVALID;
    
    // Hash password with salt
    u32 hash = hash_password(password, target_user->uid);
    char hash_str[12];
    for (int i = 0; i < 8; i++) {
        hash_str[i] = '0' + ((hash >> (i * 4)) & 0xF);
        if (hash_str[i] > '9') hash_str[i] = 'a' + (hash_str[i] - '0' - 10);
    }
    hash_str[8] = '\0';
    
    if (strcmp(target_user->password, hash_str) == 0) {
        target_user->logged_in = true;
        current_uid = target_user->uid;
        return target_user->uid;
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
    if (uid == UID_ROOT) return -1;   
    if (uid == current_uid) return -1;   
    
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
    
     
    u32 old_hash = hash_password(old_pw, u->uid);
    char hash_str[12];
    for (int i = 0; i < 8; i++) {
        hash_str[i] = '0' + ((old_hash >> (i * 4)) & 0xF);
        if (hash_str[i] > '9') hash_str[i] = 'a' + (hash_str[i] - '0' - 10);
    }
    hash_str[8] = '\0';
    
    if (strcmp(u->password, hash_str) != 0) {
        return -1;   
    }
    
     
    u32 new_hash = hash_password(new_pw, u->uid);
    for (int i = 0; i < 8; i++) {
        hash_str[i] = '0' + ((new_hash >> (i * 4)) & 0xF);
        if (hash_str[i] > '9') hash_str[i] = 'a' + (hash_str[i] - '0' - 10);
    }
    hash_str[8] = '\0';
    strncpy(u->password, hash_str, MAX_PASSWORD);
    
    return 0;
}
