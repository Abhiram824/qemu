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

static inline bool hfi_check_region_locked_access(CPUARMState* env) {
    return hfi_is_hfi_enabled(env->hfi.control_config.reg_enabled) &&
           hfi_is_region_locked(env->hfi.control_config.reg_config_opts);
}

/* Set Region Base */
void HELPER(hfi_srb)(CPUARMState* env, uint32_t region_id, uint64_t value) {
    if (!hfi_check_region_locked_access(env) || region_id >= HFI_TOTAL_REGIONS) {
        return;
    }

    env->hfi.regions[region_id].reg_base = value;
}

/* Get Region Base */
uint64_t HELPER(hfi_grb)(CPUARMState* env, uint32_t region_id) {
    if (!hfi_check_region_locked_access(env) || region_id >= HFI_TOTAL_REGIONS) {
        return 0;
    }

    return env->hfi.regions[region_id].reg_base;
}

/* Set Region Mask/Bound */
void HELPER(hfi_srm)(CPUARMState* env, uint32_t region_id, uint64_t value) {
    if (!hfi_check_region_locked_access(env) || region_id >= HFI_TOTAL_REGIONS) {
        return;
    }

    env->hfi.regions[region_id].reg_mask_or_bound = value;
}

/* Get Region Mask/Bound */
uint64_t HELPER(hfi_grm)(CPUARMState* env, uint32_t region_id) {
    if (!hfi_check_region_locked_access(env) || region_id >= HFI_TOTAL_REGIONS) {
        return 0;
    }

    return env->hfi.regions[region_id].reg_mask_or_bound;
}

/* Set Region Permissions */
void HELPER(hfi_srp)(CPUARMState* env, uint32_t region_id, uint64_t value) {
    if (!hfi_check_region_locked_access(env) || region_id >= HFI_TOTAL_REGIONS) {
        return;
    }

    // TOOD FIX: we should only allow setting of the permission bits, not the flags bits
    env->hfi.regions[region_id].reg_perms_flags = value & HFI_PERM_ALL_MASK;
}

/* Get Region Permissions */
uint64_t HELPER(hfi_grp)(CPUARMState* env, uint32_t region_id) {
    if (!hfi_check_region_locked_access(env) || region_id >= HFI_TOTAL_REGIONS) {
        return 0;
    }

    return env->hfi.regions[region_id].reg_perms_flags;
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
    if (!hfi_is_hfi_enabled(env->hfi.control_config.reg_enabled)) {
        return 0;
    }
    return (uint64_t)(uintptr_t)env->hfi.exit_state.reg_exit_handler_addr;
}

/* Enter Protected Region */
void HELPER(hfi_enter)(CPUARMState* env, uint64_t region_id, uint64_t arg2) {
}

/* Exit Protected Region */
void HELPER(hfi_exit)(CPUARMState* env) {
    /* Placeholder for EXIT logic */
}
