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

typedef struct
{

    struct
    {
        /**
         * Implicit regions are split into code and data regions
         * - data regions are used for data accesses (loads and stores)
         * - code regions are used for instruction fetches
         * Implicit regions checks are not applied on explicit regions
         */
        struct
        {
            uint64_t reg_base_addr;
            uint64_t reg_lsb_mask;
            bool reg_perm_exec;
        } implicit_code[2];

        struct
        {
            uint64_t reg_base_addr;
            uint64_t reg_lsb_mask;
            bool reg_perm_read;
            bool reg_perm_write;
        } implicit_data[4];

        /**
         * Explicit regions are a handle to a memory range with normal base, bound
         */
        struct
        {
            uint64_t reg_base_addr;
            uint64_t reg_bound_addr;
            bool reg_perm_read;
            bool reg_perm_write;
            bool reg_is_large; // whether the region is large (i.e., 2^48 or larger)
        } explicit_data[4];
    } regions;

    struct
    {
        /**
         * The register storing the ID of the region that caused the fault
         * If the access was OOB and not related to any region, this is set to 255
         */
        uint8_t reg_fault_region_id; // the region ID that caused the fault (0-5 for implicit regions, 6-9 for explicit regions)

        /**
         * The registers storing the fault information for the traps
         * fmt: HFI_FaultReason
         */
        uint8_t reg_fault_reason;

        /**
         * The register storing the fault operation for traps
         * fmt: HFI_FaultOperation
         */
        uint8_t reg_fault_operation;

        /**
         * Whether a fault has occurred and the exit handler should be called. This is set by instrumentation in the translated code when a fault condition is met, and checked by the main loop to determine whether to call the exit handler.
         */
        bool reg_fault_occurred; // whether a fault has occurred and the exit handler should be called
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
        uint8_t exit_reason;
    } exit_state;

    struct
    {
        bool reg_enabled; // whether HFI is on or off (to check bounds)

        /**
         * bit 0 - whether regions are locked or not
         */
        uint8_t reg_config_opts;
    } control_config;
} CPUArchState_HFI;

#endif /* ARM_HFI_H */