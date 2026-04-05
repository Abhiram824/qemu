#ifndef ARM_HFI_H
#define ARM_HFI_H

#include <stdbool.h>
#include <stdint.h>

/* HFI_FaultReason */
#define HFI_FAULT_OUT_OF_BOUNDS 1
#define HFI_FAULT_PERMISSION 2

/* HFI_FaultOperation */
#define HFI_FAULT_OPERATION_LOAD 1
#define HFI_FAULT_OPERATION_STORE 2
#define HFI_FAULT_OPERATION_FETCH 3

/* HFI_ExitReason */
#define HFI_EXIT_CALLED 1
#define HFI_SYSCALL_REQUESTED 2

/* HFI_Permissions */
#define HFI_PERM_READ 0x1
#define HFI_PERM_WRITE 0x2
#define HFI_PERM_EXEC 0x4
#define HFI_PERM_ALL_MASK (HFI_PERM_READ | HFI_PERM_WRITE | HFI_PERM_EXEC)

/* HFI_Flags */
#define HFI_REGION_IS_LARGE 0x8

/* REGION ENUMERATIONS */
#define HFI_REGION_IMPLICIT_CODE_COUNT 2
#define HFI_REGION_IMPLICIT_DATA_COUNT 4
#define HFI_REGION_EXPLICIT_DATA_COUNT 4

#define HFI_REGION_IMPLICIT_CODE_I 0
#define HFI_REGION_IMPLICIT_DATA_I (HFI_REGION_IMPLICIT_CODE_I + HFI_REGION_IMPLICIT_CODE_COUNT)
#define HFI_REGION_EXPLICIT_DATA_I (HFI_REGION_IMPLICIT_DATA_I + HFI_REGION_IMPLICIT_DATA_COUNT)
#define HFI_TOTAL_REGIONS (HFI_REGION_EXPLICIT_DATA_I + HFI_REGION_EXPLICIT_DATA_COUNT)

#define HFI_REGION_IS_IMPLICIT_CODE(region_id) ((region_id) < HFI_REGION_IMPLICIT_DATA_I)
#define HFI_REGION_IS_IMPLICIT_DATA(region_id) ((region_id) >= HFI_REGION_IMPLICIT_DATA_I && (region_id) < HFI_REGION_EXPLICIT_DATA_I)
#define HFI_REGION_IS_EXPLICIT_DATA(region_id) ((region_id) >= HFI_REGION_EXPLICIT_DATA_I && (region_id) < HFI_TOTAL_REGIONS)

#define HFI_REGION_ID_IS_VALID(region_id) ((region_id) < HFI_TOTAL_REGIONS)

/* ============================================================================
 * Fault State Bitvector Accessors (reg_fault_state)
 * ============================================================================
 * Bit layout:
 * [0]      : fault_occurred (1 bit)
 * [2:1]    : fault_operation (2 bits)
 * [4:3]    : fault_reason (2 bits)
 * [9:5]    : region_id (5 bits)
 * [63:10]  : reserved (54 bits)
 */

#define HFI_FAULT_STATE_GET_OCCURRED(state)     (((state) >> 0) & 0x1ULL)
#define HFI_FAULT_STATE_SET_OCCURRED(state, val) \
    ((state) = (((state) & ~(0x1ULL << 0)) | (((uint64_t)(val) & 0x1ULL) << 0)))

#define HFI_FAULT_STATE_GET_OPERATION(state)    (((state) >> 1) & 0x3ULL)
#define HFI_FAULT_STATE_SET_OPERATION(state, val) \
    ((state) = (((state) & ~(0x3ULL << 1)) | (((uint64_t)(val) & 0x3ULL) << 1)))

#define HFI_FAULT_STATE_GET_REASON(state)       (((state) >> 3) & 0x3ULL)
#define HFI_FAULT_STATE_SET_REASON(state, val) \
    ((state) = (((state) & ~(0x3ULL << 3)) | (((uint64_t)(val) & 0x3ULL) << 3)))

#define HFI_FAULT_STATE_GET_REGION_ID(state)    (((state) >> 5) & 0x1FULL)
#define HFI_FAULT_STATE_SET_REGION_ID(state, val) \
    ((state) = (((state) & ~(0x1FULL << 5)) | (((uint64_t)(val) & 0x1FULL) << 5)))

/* ============================================================================
 * Exit State Bitvector Accessors (reg_exit_state)
 * ============================================================================
 * Bit layout:
 * [1:0]    : exit_reason (2 bits)
 * [63:2]   : exit_pc_offset (62 bits) - stores (PC >> 2)
 */

#define HFI_EXIT_STATE_GET_REASON(state)        (((state) >> 0) & 0x3ULL)
#define HFI_EXIT_STATE_SET_REASON(state, val) \
    ((state) = (((state) & ~(0x3ULL << 0)) | (((uint64_t)(val) & 0x3ULL) << 0)))

#define HFI_EXIT_STATE_GET_PC(state)            (((state) >> 2) << 2)  /* Reconstruct PC from offset */
#define HFI_EXIT_STATE_SET_PC_AND_REASON(pc, reason) \
    ((((uint64_t)(pc) >> 2) << 2) | (((uint64_t)(reason) & 0x3ULL)))

typedef struct
{
    struct
    {
        /**
         * Implicit regions are split into code and data regions
         * - data regions are used for data accesses (loads and stores)
         * - code regions are used for instruction fetches
         * Implicit regions checks are not applied on explicit regions
         * Explicit regions are a handle to a memory range with normal base, bound
         * Permission and flags are in a bit vector.
         * Regions 0-1: implicit code
         * Regions 2-5: implicit data
         * Regions 6-9: explicit data
         */
        uint64_t reg_base;
        uint64_t reg_mask_or_bound;  // mask for implicit regions, bound for explicit regions
        uint32_t reg_perms_flags;
    } regions[HFI_TOTAL_REGIONS];

    struct
    {
        /**
         * Fault state bitvector (64-bit):
         * [0]      : fault_occurred (1 bit) - set if fault detected
         * [2:1]    : fault_operation (2 bits) - LOAD=1, STORE=2, FETCH=3
         * [4:3]    : fault_reason (2 bits) - OUT_OF_BOUNDS=1, PERMISSION=2
         * [9:5]    : region_id (5 bits) - 0-31 (only 0-9 valid, 255 maps to 0)
         * [63:10]  : reserved (54 bits, padding)
         */
        uint64_t reg_fault_state;

        /**
         * Exit state bitvector (64-bit):
         * [1:0]    : exit_reason (2 bits) - EXIT_CALLED=1, SYSCALL_REQUESTED=2
         * [63:2]   : exit_pc_offset (62 bits) - (PC >> 2) to store 4-byte-aligned PC
         */
        uint64_t reg_exit_state;
    } exec_state;

    struct
    {
        uint32_t reg_enabled;  // whether HFI is on or off (to check bounds)

        /**
         * bit 0 - whether regions are locked or not
         */
        uint32_t reg_config_opts;

        /**
         * The exit handler address for traps (moved from exit_state struct)
         */
        uintptr_t reg_exit_handler_addr;
    } control_config;
} CPUArchState_HFI;

inline bool hfi_get_perm_read(uint32_t perms_flags) {
    return (perms_flags & HFI_PERM_READ) != 0;
}

inline void hfi_set_perm_read(uint32_t* perms_flags, bool can_read) {
    if (can_read) {
        *perms_flags |= HFI_PERM_READ;
    } else {
        *perms_flags &= ~HFI_PERM_READ;
    }
}

inline void hfi_set_perm_write(uint32_t* perms_flags, bool can_write) {
    if (can_write) {
        *perms_flags |= HFI_PERM_WRITE;
    } else {
        *perms_flags &= ~HFI_PERM_WRITE;
    }
}

inline bool hfi_get_perm_exec(uint32_t perms_flags) {
    return (perms_flags & HFI_PERM_EXEC) != 0;
}

inline void hfi_set_perm_exec(uint32_t* perms_flags, bool can_exec) {
    if (can_exec) {
        *perms_flags |= HFI_PERM_EXEC;
    } else {
        *perms_flags &= ~HFI_PERM_EXEC;
    }
}

inline bool hfi_get_flag_is_large(uint32_t perms_flags) {
    return (perms_flags & HFI_REGION_IS_LARGE) != 0;
}

inline void hfi_set_flag_is_large(uint32_t* perms_flags, bool is_large) {
    if (is_large) {
        *perms_flags |= HFI_REGION_IS_LARGE;
    } else {
        *perms_flags &= ~HFI_REGION_IS_LARGE;
    }
}

inline bool hfi_is_region_locked(uint32_t config_opts) {
    return (config_opts & 0x1) != 0;
}

inline bool hfi_set_region_locked(uint32_t* config_opts, bool locked) {
    if (locked) {
        *config_opts |= 0x1;
    } else {
        *config_opts &= ~0x1;
    }
}

inline bool hfi_is_hfi_enabled(uint32_t enabled) {
    return enabled != 0;
}

#endif /* ARM_HFI_H */