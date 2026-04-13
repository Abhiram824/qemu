/*
 *  AArch64 HFI (Hardware Fault Isolation) helper definitions
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

/* Region-based helpers */
DEF_HELPER_3(hfi_srb, void, env, i32, i64)
DEF_HELPER_FLAGS_2(hfi_grb, TCG_CALL_NO_WG, i64, env, i32)
DEF_HELPER_3(hfi_srm, void, env, i32, i64)
DEF_HELPER_FLAGS_2(hfi_grm, TCG_CALL_NO_WG, i64, env, i32)
DEF_HELPER_3(hfi_srp, void, env, i32, i64)
DEF_HELPER_FLAGS_2(hfi_grp, TCG_CALL_NO_WG, i64, env, i32)
DEF_HELPER_5(hfi_addr_in_region, void, env, i64, i32, i32, i32)

/* Exit handler helpers */
DEF_HELPER_2(hfi_seh, void, env, i64)
DEF_HELPER_FLAGS_1(hfi_geh, TCG_CALL_NO_WG, i64, env)

/* ENTER and EXIT helpers */
DEF_HELPER_3(hfi_enter, void, env, i64, i64)
DEF_HELPER_1(hfi_exit, void, env)
