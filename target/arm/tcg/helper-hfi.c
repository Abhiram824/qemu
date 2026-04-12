/*
 *  AArch64 HFI (Hardware Fault Isolation) helpers
 *
 *  Copyright (c) 2024
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

// clang-format off
#include "qemu/osdep.h"
#include "qemu/log.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "exec/exec-all.h"
#include "exec/cpu-defs.h"
#include "syndrome.h"
#include "internals.h"
#include "../hfi.h"
// clang-format on

// ===============================================================================
// =================================== DEBUG =====================================
// ===============================================================================

void HELPER(debuglog)(CPUARMState* env, uint32_t id) {
    static int x = 0;
    // uint64_t x8val = env->xregs[8];
    // uint64_t hfi_enabled = env->hfi.control_config.reg_enabled;
    // qemu_log("\tqemu[QEMU_DEBUG_LOG] id=%-3u | x=%-3d | x8=%-7ld | hfienable=%-lx\n", id, x, x8val, hfi_enabled);
    x++;
}

// ===============================================================================
// ================================= VALIDATION ==================================
// ===============================================================================

/**
 * Raise an HFI-specific undefined instruction exception
 * Used for: invalid register access, invalid indexing, invalid mode switches
 */
static inline void hfi_raise_exception(CPUARMState* env) {
    raise_exception_ra(env, EXCP_HFI, syn_uncategorized(), exception_target_el(env), GETPC());
}

/**
 * Check if HFI is enabled (pure boolean check, no exception)
 * Returns: true if enabled, false otherwise
 */
static inline bool hfi_is_enabled(CPUARMState* env) {
    return env->hfi.control_config.reg_enabled;
}

/**
 * Check if HFI regions are locked (pure boolean check, no exception)
 * Returns: true if locked, false otherwise
 */
static inline bool hfi_is_locked(CPUARMState* env) {
    return hfi_is_region_locked(env->hfi.control_config.reg_config_opts);
}

/**
 * Check if region ID is valid (pure boolean check, no exception)
 * Returns: true if valid (0 to HFI_TOTAL_REGIONS-1), false otherwise
 */
static inline bool hfi_is_region_id_valid(uint64_t region_id) {
    return HFI_REGION_ID_IS_VALID(region_id);
}

// ===============================================================================
// ================================= REGION OPERATIONS ===========================
// ===============================================================================

/**
 * Set Region Base
 * No access control - these are setup registers
 */
void HELPER(hfi_srb)(CPUARMState* env, uint64_t region_id, uint64_t value) {
    if (!hfi_is_region_id_valid(region_id)) {
        hfi_raise_exception(env);
        return;
    }
    if (hfi_is_enabled(env) && hfi_is_locked(env)) {
        hfi_raise_exception(env);
        return;
    }
    env->hfi.regions[region_id].reg_base = value;
}

/**
 * Get Region Base
 * No access control - these are setup registers
 */
uint64_t HELPER(hfi_grb)(CPUARMState* env, uint64_t region_id) {
    if (!hfi_is_region_id_valid(region_id)) {
        hfi_raise_exception(env);
        return 0;
    }
    return env->hfi.regions[region_id].reg_base;
}

/**
 * Set Region Mask/Bound
 * No access control - these are setup registers
 */
void HELPER(hfi_srm)(CPUARMState* env, uint64_t region_id, uint64_t value) {
    if (!hfi_is_region_id_valid(region_id)) {
        hfi_raise_exception(env);
        return;
    }
    if (hfi_is_enabled(env) && hfi_is_locked(env)) {
        hfi_raise_exception(env);
        return;
    }
    env->hfi.regions[region_id].reg_mask_or_bound = value;
}

/**
 * Get Region Mask/Bound
 * No access control - these are setup registers
 */
uint64_t HELPER(hfi_grm)(CPUARMState* env, uint64_t region_id) {
    if (!hfi_is_region_id_valid(region_id)) {
        hfi_raise_exception(env);
        return 0;
    }

    return env->hfi.regions[region_id].reg_mask_or_bound;
}

/**
 * Set Region Permissions
 * No access control - these are setup registers
 *
 * Filters permissions based on region type:
 *   - Code regions (0-1): only EXEC permission allowed
 *   - Data regions (2-9): only READ/WRITE permissions allowed
 */
void HELPER(hfi_srp)(CPUARMState* env, uint64_t region_id, uint64_t value) {
    if (!hfi_is_region_id_valid(region_id)) {
        hfi_raise_exception(env);
        return;
    }
    if (hfi_is_enabled(env) && hfi_is_locked(env)) {
        hfi_raise_exception(env);
        return;
    }

    /* Filter permissions based on region type */
    if (HFI_REGION_IS_IMPLICIT_CODE(region_id)) {
        /* Code regions: only EXEC permission */
        value &= HFI_PERM_EXEC;
    } else {
        /* Data regions: only READ/WRITE permissions */
        value &= (HFI_PERM_READ | HFI_PERM_WRITE);
    }

    env->hfi.regions[region_id].reg_perms_flags = value;
}

/**
 * Get Region Permissions
 * No access control - these are setup registers
 */
uint64_t HELPER(hfi_grp)(CPUARMState* env, uint64_t region_id) {
    if (!hfi_is_region_id_valid(region_id)) {
        hfi_raise_exception(env);
        return 0;
    }
    return env->hfi.regions[region_id].reg_perms_flags & HFI_PERM_ALL_MASK;
}

// ===============================================================================
// ============================= FAULT STATE OPERATIONS ==========================
// ===============================================================================

/**
 * Get Fault State (user and kernel allowed)
 * Returns the fault state bitvector containing:
 * - fault_occurred (bit 0)
 * - fault_operation (bits 2:1)
 * - fault_reason (bits 4:3)
 * - region_id (bits 9:5)
 */
uint64_t HELPER(hfi_gfs)(CPUARMState* env) {
    return env->hfi.exec_state.reg_fault_state;
}

/**
 * Set Fault State (kernel-only)
 * Allows writing to the fault state bitvector.
 * Raises: EXCP_HFI if called from user mode (EL==0)
 */
void HELPER(hfi_sfs)(CPUARMState* env, uint64_t value) {
    /* Kernel-only: check execution level (EL0 = user, EL1+ = kernel) */
    if (arm_current_el(env) == 0) {
        hfi_raise_exception(env);
        return;
    }
    env->hfi.exec_state.reg_fault_state = value;
}

// ===============================================================================
// ============================= EXIT STATE OPERATIONS ===========================
// ===============================================================================

/**
 * Get Exit State (user and kernel allowed)
 * Returns the exit state bitvector containing:
 * - exit_reason (bits 1:0)
 * - exit_pc_offset (bits 63:2) where PC = (exit_pc_offset << 2)
 */
uint64_t HELPER(hfi_ges)(CPUARMState* env) {
    return env->hfi.exec_state.reg_exit_state;
}

/**
 * Set Exit State (kernel-only)
 * Allows writing to the exit state bitvector.
 * Raises: EXCP_HFI if called from user mode (EL==0)
 */
void HELPER(hfi_ses)(CPUARMState* env, uint64_t value) {
    /* Kernel-only: check execution level (EL0 = user, EL1+ = kernel) */
    if (arm_current_el(env) == 0) {
        hfi_raise_exception(env);
        return;
    }
    env->hfi.exec_state.reg_exit_state = value;
}

// ===============================================================================


/**
 * Set Exit Handler
 * Requires: HFI not locked
 * Raises: EXCP_HFI if HFI enabled and locked (regions locked while enabled)
 */
void HELPER(hfi_seh)(CPUARMState* env, uint64_t value) {
    if (hfi_is_enabled(env) && hfi_is_locked(env)) {
        hfi_raise_exception(env);
        return;
    }
    env->hfi.control_config.reg_exit_handler_addr = (uintptr_t)value;
}

/**
 * Get Exit Handler
 * No access control: always allowed
 */
uint64_t HELPER(hfi_geh)(CPUARMState* env) {
    return (uint64_t)env->hfi.control_config.reg_exit_handler_addr;
}

// ===============================================================================
// =========================== ENTER/EXIT REGION OPERATIONS ======================
// ===============================================================================

/**
 * Enter Protected Region
 * Requires: HFI NOT already enabled (nested entry not allowed)
 * Raises: EXCP_HFI if already in HFI mode (invalid mode switch)
 */
void HELPER(hfi_enter)(CPUARMState* env, uint64_t jump_target, uint64_t options) {
    CPUState* cpu = env_cpu(env);

    /* Cannot nest HFI enter - must not already be enabled */
    if (hfi_is_enabled(env)) {
        hfi_raise_exception(env);
        return;
    }

    /* Initialize fault state bitvector to 0 (no fault) */
    env->hfi.exec_state.reg_fault_state = 0;

    /* Enable HFI and set configuration options */
    env->hfi.control_config.reg_enabled = 1;
    env->hfi.control_config.reg_config_opts = options;
    env->pc = jump_target;

    qemu_log("\tqemu[HFI_ENTER] pc=%lx\n", env->pc);

    /* Exit to main loop so it restarts at the new PC */
    cpu_loop_exit(cpu);
}

/**
 * Exit Protected Region
 * Requires: HFI currently enabled
 * Raises: EXCP_HFI if HFI not currently enabled (invalid mode switch)
 * Parameters:
 *   env: CPU state
 *   exit_cause: Reason for exiting (HFI_ExitReason)
 */
void HELPER(hfi_exit)(CPUARMState* env, uint32_t exit_cause) {
    CPUState* cpu = env_cpu(env);

    qemu_log("\tqemu[HELPER] Exiting HFI cause=%u | pc=%lx\n", exit_cause, env->pc);
    
    /* Cannot exit if not currently in HFI mode */
    if (!hfi_is_enabled(env)) {
        hfi_raise_exception(env);
        return;
    }

    /* Pack exit reason and PC into exit state bitvector */
    env->hfi.exec_state.reg_exit_state = HFI_EXIT_STATE_SET_PC_AND_REASON(env->pc, exit_cause);

    /* Disable HFI */
    env->hfi.control_config.reg_enabled = 0;

    /* Jump to exit handler address */
    env->pc = env->hfi.control_config.reg_exit_handler_addr;

    /* Exit to main loop so it restarts at the new PC */
    cpu_loop_exit(cpu);
}
