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
#include "cpu.h"
#include "exec/helper-proto.h"
#include "../hfi.h"
// clang-format on

// ===============================================================================
// =================================== HELPERS ===================================
// ===============================================================================

static inline bool hfi_verify_region_access(CPUARMState* env, uint32_t region_id) {
    // generate exception if region is locked
    if (hfi_is_hfi_enabled(env->hfi.control_config.reg_enabled) &&
        hfi_is_region_locked(env->hfi.control_config.reg_config_opts)) {
        // for now, just return false to indicate invalid access
        return false;
    }
    return true;
}

static inline bool hfi_verify_region_id(uint32_t region_id) {
    // generate exception if region id is out of bounds
    if (!HFI_REGION_ID_IS_VALID(region_id)) {
        // for now, just return false to indicate invalid access
        return false;
    }
    return true;
}

// ===============================================================================
// ==================================== DEFS =====================================
// ===============================================================================

/* Set Region Base */
void HELPER(hfi_srb)(CPUARMState* env, uint32_t region_id, uint64_t value) {
    if (!hfi_verify_region_id(region_id) || !hfi_verify_region_access(env, region_id)) {
        return;
    }

    env->hfi.regions[region_id].reg_base = value;
}

/* Get Region Base */
uint64_t HELPER(hfi_grb)(CPUARMState* env, uint32_t region_id) {
    if (!hfi_verify_region_id(region_id)) {
        return 0;
        
    }

    return env->hfi.regions[region_id].reg_base;
}

/* Set Region Mask/Bound */
void HELPER(hfi_srm)(CPUARMState* env, uint32_t region_id, uint64_t value) {
    if (!hfi_verify_region_id(region_id) || !hfi_verify_region_access(env, region_id)) {
        return;
    }

    env->hfi.regions[region_id].reg_mask_or_bound = value;
}

/* Get Region Mask/Bound */
uint64_t HELPER(hfi_grm)(CPUARMState* env, uint32_t region_id) {
    if (!hfi_verify_region_id(region_id)) {
        return 0;
    }

    return env->hfi.regions[region_id].reg_mask_or_bound;
}

/* Set Region Permissions */
void HELPER(hfi_srp)(CPUARMState* env, uint32_t region_id, uint64_t value) {
    if (!hfi_verify_region_id(region_id) || !hfi_verify_region_access(env, region_id)) {
        return;
    }

    // if region is code
    if (HFI_REGION_IS_IMPLICIT_CODE(region_id)) {
        // only allow EXEC permission for code regions, ignore other bits
        value &= HFI_PERM_EXEC;
    } else {
        // for data regions, only allow READ/WRITE permissions, ignore other bits
        value &= (HFI_PERM_READ | HFI_PERM_WRITE);
    }

    // set the permission
    env->hfi.regions[region_id].reg_perms_flags &= ~HFI_PERM_ALL_MASK;  // Clear existing permission bits
    env->hfi.regions[region_id].reg_perms_flags |= value;
}

/* Get Region Permissions */
uint64_t HELPER(hfi_grp)(CPUARMState* env, uint32_t region_id) {
    return env->hfi.regions[region_id].reg_perms_flags & HFI_PERM_ALL_MASK;  // Return only the permission bits
}

/* Set Exit Handler */
void HELPER(hfi_seh)(CPUARMState* env, uint64_t value) {
    if (!hfi_is_hfi_enabled(env->hfi.control_config.reg_enabled)) {
        return;
    }
    
    env->hfi.exit_state.reg_exit_handler_addr = (uintptr_t)value;
}

/* Get Exit Handler */
uint64_t HELPER(hfi_geh)(CPUARMState* env) {
    return (uint64_t)env->hfi.exit_state.reg_exit_handler_addr;
}

/* Enter Protected Region */
void HELPER(hfi_enter)(CPUARMState* env, uint64_t jump_target, uint64_t options) {
    // if we are already in hfi, we need to trigger exception
    if (hfi_is_hfi_enabled(env->hfi.control_config.reg_enabled)) {
        // for now, just ret
        return;
    }

    env->hfi.control_config.reg_enabled = 1;
    env->hfi.control_config.reg_config_opts = options;
    env->pc = jump_target;
}

/* Exit Protected Region */
void HELPER(hfi_exit)(CPUARMState* env) {
    // if we are not in hfi, this should be an exception
    if (!hfi_is_hfi_enabled(env->hfi.control_config.reg_enabled)) {
        // for now, just ret
        return;
    }

    env->hfi.control_config.reg_enabled = 0;

    // we need to jump to the exit handler
}
