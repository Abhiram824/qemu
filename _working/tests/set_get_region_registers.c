#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../hfi_test_helper.h"

/*
 * REGION ENUMERATION:
 * Implicit regions 0-1: code
 * Implicit regions 2-5: data
 * Explicit regions 6-9: data
 *
 * CONVENTION: Destination is always first param
 */

/* Test HFI_SRB / HFI_GRB (Region Base) */
int test_region_base_implicit_code() {
    uint64_t base_value = 0x1000000000000000ULL;
    
    do_hfi_srb(0, base_value);
    uint64_t result = do_hfi_grb(0);
    
    return result == base_value;
}

int test_region_base_implicit_data() {
    uint64_t base_value = 0x2000000000000000ULL;
    
    do_hfi_srb(2, base_value);
    uint64_t result = do_hfi_grb(2);
    
    return result == base_value;
}

int test_region_base_explicit_data() {
    uint64_t base_value = 0x3000000000000000ULL;
    
    do_hfi_srb(6, base_value);
    uint64_t result = do_hfi_grb(6);
    
    return result == base_value;
}

/* Test HFI_SRM / HFI_GRM (Region Mask/Bound) */
int test_region_mask_implicit_code() {
    uint64_t mask_value = 0x0000FFFF00000000ULL;
    
    do_hfi_srm(0, mask_value);
    uint64_t result = do_hfi_grm(0);
    
    return result == mask_value;
}

int test_region_mask_implicit_data() {
    uint64_t mask_value = 0x00000000FFFFFFFFULL;
    
    do_hfi_srm(3, mask_value);
    uint64_t result = do_hfi_grm(3);
    
    return result == mask_value;
}

int test_region_mask_explicit_data() {
    uint64_t bound_value = 0x7FFFFFFFFFFFFFFFULL;
    
    do_hfi_srm(7, bound_value);
    uint64_t result = do_hfi_grm(7);
    
    return result == bound_value;
}

/* Test HFI_SRP / HFI_GRP (Region Permissions) */
int test_region_permissions_implicit_code() {
    /* Code region: bit 2 is EXEC */
    uint64_t perm_value = 0x4;  // EXEC permission
    
    do_hfi_srp(0, perm_value);
    uint64_t result = do_hfi_grp(0);
    
    return result == perm_value;
}

int test_region_permissions_implicit_data() {
    /* Data region: bits 0-1 are READ/WRITE */
    uint64_t perm_value = 0x3;  // READ | WRITE permissions
    
    do_hfi_srp(2, perm_value);
    uint64_t result = do_hfi_grp(2);
    
    return result == perm_value;
}

int test_region_permissions_explicit_data() {
    /* Explicit data region: bits 0-1 are READ/WRITE */
    uint64_t perm_value = 0x1;  // READ permission only
    
    do_hfi_srp(8, perm_value);
    uint64_t result = do_hfi_grp(8);
    
    return result == perm_value;
}

/* Test multiple regions to ensure isolation */
int test_multiple_regions_isolation() {
    uint64_t base_region_0 = 0x1000000000000000ULL;
    uint64_t base_region_1 = 0x2000000000000000ULL;
    
    do_hfi_srb(0, base_region_0);
    do_hfi_srb(1, base_region_1);
    
    uint64_t result_0 = do_hfi_grb(0);
    uint64_t result_1 = do_hfi_grb(1);
    
    return (result_0 == base_region_0) && (result_1 == base_region_1);
}

/* Test all region types */
int test_all_region_types() {
    /* Test implicit code (0-1), implicit data (2-5), explicit data (6-9) */
    uint64_t bases[10];
    memset(bases, 0, sizeof(bases));

    for (int i = 0; i < 10; i++) {
        uint64_t test_base = 0x1000000000000000ULL + ((uint64_t)i << 32);
        bases[i] = test_base;
        
        do_hfi_srb(i, test_base);
    }
    
    /* Verify all regions */
    for (int i = 0; i < 10; i++) {
        uint64_t result = do_hfi_grb(i);
        if (result != bases[i]) {
            return 0;
        }
    }
    
    return 1;
}

TEST_MAIN({
    TEST(test_region_base_implicit_code(), 1 ==);
    TEST(test_region_base_implicit_data(), 1 ==);
    TEST(test_region_base_explicit_data(), 1 ==);
    TEST(test_region_mask_implicit_code(), 1 ==);
    TEST(test_region_mask_implicit_data(), 1 ==);
    TEST(test_region_mask_explicit_data(), 1 ==);
    TEST(test_region_permissions_implicit_code(), 1 ==);
    TEST(test_region_permissions_implicit_data(), 1 ==);
    TEST(test_region_permissions_explicit_data(), 1 ==);
    TEST(test_multiple_regions_isolation(), 1 ==);
})
