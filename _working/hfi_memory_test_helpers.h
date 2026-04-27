#ifndef HFI_MEMORY_TEST_HELPERS_H
#define HFI_MEMORY_TEST_HELPERS_H

#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <signal.h>
#include "hfi_test_helper.h"

static inline void hfi_return() {
    printf("Returned from HFI sandbox successfully!\n");
    fflush(stdout);
}

static inline void* alloc_code(size_t size) {
    return mmap(NULL, size, PROT_READ|PROT_WRITE|PROT_EXEC,
                MAP_SHARED|MAP_ANONYMOUS, -1, 0);
}

static inline void* alloc_data(size_t size) {
    return mmap(NULL, 4096, PROT_READ|PROT_WRITE, 
        MAP_SHARED|MAP_ANONYMOUS, -1, 0);
}

#define HFI_SRB_X1(val, region_id) \
    asm volatile( \
        "mov x0, %1\n" \
        "mov x1, %0\n" \
        HFI_SRB(0, 1) \
        : \
        : "r"((uint64_t)(val)), "r"((uint64_t)(region_id)) \
        : "x0", "x1")

#define HFI_SRM_X1(val, region_id) \
    asm volatile( \
        "mov x0, %1\n" \
        "mov x1, %0\n" \
        HFI_SRM(0, 1) \
        : \
        : "r"((uint64_t)(val)), "r"((uint64_t)(region_id)) \
        : "x0", "x1")

#define HFI_SRP_X1(val, region_id) \
    asm volatile( \
        "mov x0, %1\n" \
        "mov x1, %0\n" \
        HFI_SRP(0, 1) \
        : \
        : "r"((uint64_t)(val)), "r"((uint64_t)(region_id)) \
        : "x0", "x1")

#endif // HFI_MEMORY_TEST_HELPERS_H