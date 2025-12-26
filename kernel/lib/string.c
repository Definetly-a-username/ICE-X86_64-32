#include "string.h"

void *memcpy(void *dest, const void *src, size_t n) {
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;
    
    // Optimization for word-aligned copies
    if (n >= 4 && ((u32)d & 3) == 0 && ((u32)s & 3) == 0) {
        u32 *d32 = (u32 *)dest;
        const u32 *s32 = (const u32 *)src;
        size_t n32 = n / 4;
        
        while (n32--) {
            *d32++ = *s32++;
        }
        
        d = (u8 *)d32;
        s = (u8 *)s32;
        n %= 4;
    }
    
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void *memset(void *s, int c, size_t n) {
    u8 *p = (u8 *)s;
    
    // Optimization for word-aligned sets
    if (n >= 4 && ((u32)p & 3) == 0) {
        u32 c32 = c | (c << 8) | (c << 16) | (c << 24);
        u32 *p32 = (u32 *)s;
        size_t n32 = n / 4;
        
        while (n32--) {
            *p32++ = c32;
        }
        
        p = (u8 *)p32;
        n %= 4;
    }
    
    while (n--) {
        *p++ = (u8)c;
    }
    return s;
}

void *memmove(void *dest, const void *src, size_t n) {
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;

    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const u8 *p1 = (const u8 *)s1;
    const u8 *p2 = (const u8 *)s2;
    
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for ( ; i < n; i++)
        dest[i] = '\0';
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

char *strcat(char *dest, const char *src) {
    char *p = dest + strlen(dest);
    while ((*p++ = *src++));
    return dest;
}

char *strncat(char *dest, const char *src, size_t n) {
    char *p = dest + strlen(dest);
    while (n-- && *src) {
        *p++ = *src++;
    }
    *p = '\0';
    return dest;
}

char *strchr(const char *s, int c) {
    while (*s != (char)c) {
        if (!*s++) return NULL;
    }
    return (char *)s;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    do {
        if (*s == (char)c) last = s;
    } while (*s++);
    return (char *)last;
}
