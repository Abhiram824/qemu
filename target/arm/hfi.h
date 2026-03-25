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
         * The register storing the ID of the region that caused the fault
         * If the access was OOB and not related to any region, this is set to 255
         */
        uint32_t reg_fault_region_id;  // the region ID that caused the fault (0-5 for implicit regions, 6-9 for explicit regions)

        /**
         * The registers storing the fault information for the traps
         * fmt: HFI_FaultReason
         */
        uint32_t reg_fault_reason;

        /**
         * The register storing the fault operation for traps
         * fmt: HFI_FaultOperation
         */
        uint32_t reg_fault_operation;

        /**
         * Whether a fault has occurred and the exit handler should be called. This is set by instrumentation in the translated code when a fault condition is met, and checked by the main loop to determine whether to call the exit handler.
         */
        uint32_t reg_fault_occurred;  // whether a fault has occurred and the exit handler should be called
    } fault_config;

    struct
    {
        /**
         * The register storing the exit handler address for traps
         */
        uintptr_t reg_exit_handler_addr;

        /**
         * The reason for leaving hfi
         * fmt: HFI_ExitReason
         */
        uint32_t exit_reason;
    } exit_state;

    struct
    {
        uint32_t reg_enabled;  // whether HFI is on or off (to check bounds)

        /**
         * bit 0 - whether regions are locked or not
         */
        uint32_t reg_config_opts;
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