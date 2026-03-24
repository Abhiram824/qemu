#ifndef ARM_HFI_H
#define ARM_HFI_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    /**
     * Implicit regions are split into code and data regions
     * - data regions are used for data accesses (loads and stores)
     * - code regions are used for instruction fetches
     * Implicit regions checks are not applied on explicit regions
     */
    struct {
        uint64_t reg_base_addr;
        uint64_t reg_lsb_mask;
        bool reg_perm_exec;
    } implicit_code_regions[2];

    struct {
        uint64_t reg_base_addr;
        uint64_t reg_lsb_mask;
        bool reg_perm_read;
        bool reg_perm_write;
    } implicit_data_regions[4];

    /**
     * Explicit regions are a handle to a memory range with normal base, bound
     */
    struct {
        uint64_t reg_base_addr;
        uint64_t reg_bound_addr;
        bool reg_perm_read;
        bool reg_perm_write;
        bool reg_is_large;  // whether the region is large (i.e., 2^48 or larger)
    } explicit_data_regions[4];

    struct {
        /**
         * The register storing the exit handler address for traps
         */
        uintptr_t reg_exit_handler_addr;

        /**
         *  The register storing the fault reason for traps
         */
        uint8_t reg_fault_reason;
    } fault_config;

    struct {
        bool reg_enabled;    // whether HFI is on or off (to check bounds)
        bool reg_is_hybrid;  // native vs hybrid
        bool reg_lock_regions; // whether regions are locked (i.e., cannot be modified until next reset)
    } control_config;
} CPUArchState_HFI;

#endif /* ARM_HFI_H */