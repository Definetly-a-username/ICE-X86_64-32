/*
 * ICE Operating System - Kernel Types
 * Standard type definitions
 */

#ifndef ICE_TYPES_H
#define ICE_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Standard sized types */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

/* Physical and virtual addresses */
typedef u32 phys_addr_t;
typedef u32 virt_addr_t;

/* Process ID */
typedef u32 ice_pid_t;
#define PID_INVALID 0

/* Executable ID */
typedef u32 exec_id_t;
#define EXEC_ID_INVALID 0
#define EXEC_ID_FORMAT "#%08X"

#endif /* ICE_TYPES_H */
