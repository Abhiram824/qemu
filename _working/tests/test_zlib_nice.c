#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "../hfi_test_helper.h"
#include "zlib.h"

typedef uLong (*compress_bound_fn_t)(uLong source_len);
typedef int (*deflate_init_fn_t)(z_streamp strm, int level,
                                 const char *version, int stream_size);
typedef int (*deflate_fn_t)(z_streamp strm, int flush);
typedef int (*deflate_end_fn_t)(z_streamp strm);

typedef struct {
    deflate_init_fn_t deflate_init_fn;
    deflate_fn_t deflate_fn;
    deflate_end_fn_t deflate_end_fn;
    Bytef *dest_buf;
    uLongf dest_len;
    const Bytef *source_buf;
    uLong source_len;
    Bytef *alloc_pool;
    size_t alloc_pool_size;
    size_t alloc_pool_used;
    size_t last_alloc_bytes;
    size_t alloc_count;
    int alloc_failed;
    int zlib_result;
} zlib_sandbox_ctx_t;

typedef struct {
    uint64_t base;
    uint64_t bound;
} hfi_region_t;

// ============================================================================
// HFI Context State
// ============================================================================

uint64_t hfi_end_addr = (uint64_t)-1;
uint64_t hfi_end_sp = (uint64_t)-1;
uint64_t hfi_config = (uint64_t)-1;

static uint64_t align_up_u64(uint64_t value, uint64_t align);
static voidpf sandbox_zalloc(voidpf opaque, uInt items, uInt size);
static void sandbox_zfree(voidpf opaque, voidpf address);

// ============================================================================
// Sandbox Proxy
// ============================================================================

__attribute__((no_stack_protector))
void zlib_sandbox_proxy(zlib_sandbox_ctx_t *ctx)
{
    z_stream stream;
    unsigned char *stream_bytes = (unsigned char *)&stream;
    size_t i;

    for (i = 0; i < sizeof(stream); i++) {
        stream_bytes[i] = 0;
    }

    stream.next_in = (Bytef *)ctx->source_buf;
    stream.avail_in = (uInt)ctx->source_len;
    stream.next_out = ctx->dest_buf;
    stream.avail_out = (uInt)ctx->dest_len;
    stream.zalloc = sandbox_zalloc;
    stream.zfree = sandbox_zfree;
    stream.opaque = ctx;

    ctx->zlib_result =
        ctx->deflate_init_fn(&stream, Z_DEFAULT_COMPRESSION,
                             ZLIB_VERSION, (int)sizeof(stream));
    if (ctx->zlib_result != Z_OK) {
        return;
    }

    ctx->zlib_result = ctx->deflate_fn(&stream, Z_FINISH);
    if (ctx->zlib_result == Z_STREAM_END) {
        ctx->zlib_result = Z_OK;
    }
    ctx->dest_len = stream.total_out;
    ctx->deflate_end_fn(&stream);
}

__attribute__((no_stack_protector))
static voidpf sandbox_zalloc(voidpf opaque, uInt items, uInt size)
{
    zlib_sandbox_ctx_t *ctx = (zlib_sandbox_ctx_t *)opaque;
    unsigned char *bytes_ptr;
    size_t bytes = align_up_u64((size_t)items * (size_t)size, 16);
    size_t i;
    voidpf result;

    if (ctx->alloc_pool_used + bytes > ctx->alloc_pool_size) {
        ctx->last_alloc_bytes = bytes;
        ctx->alloc_failed = 1;
        return Z_NULL;
    }

    result = ctx->alloc_pool + ctx->alloc_pool_used;
    ctx->alloc_pool_used += bytes;
    ctx->last_alloc_bytes = bytes;
    ctx->alloc_count += 1;
    bytes_ptr = (unsigned char *)result;
    for (i = 0; i < bytes; i++) {
        bytes_ptr[i] = 0;
    }
    return result;
}

__attribute__((no_stack_protector))
static void sandbox_zfree(voidpf opaque, voidpf address)
{
    (void)opaque;
    (void)address;
}

void zlib_sandbox_entry(void);
asm(
    ".text\n"
    ".align 4\n"
    ".global zlib_sandbox_entry\n"
    ".type zlib_sandbox_entry, @function\n"
    "zlib_sandbox_entry:\n"
    "mov sp, x1\n"
    "bl zlib_sandbox_proxy\n"
    "" HFI_EXIT());

// ============================================================================
// Assembly Scaffold & Handlers
// ============================================================================

void exit_handler_scaffold(void);
asm(
    ".text\n"
    ".align 4\n"
    ".global exit_handler_scaffold\n"
    ".type exit_handler_scaffold, @function\n"
    "exit_handler_scaffold:\n"
    "sub sp, sp, #256\n"
    "stp x0, x1, [sp, #0]\n"
    "stp x2, x3, [sp, #16]\n"
    "stp x4, x5, [sp, #32]\n"
    "stp x6, x7, [sp, #48]\n"
    "stp x8, x9, [sp, #64]\n"
    "stp x10, x11, [sp, #80]\n"
    "stp x12, x13, [sp, #96]\n"
    "stp x14, x15, [sp, #112]\n"
    "stp x16, x17, [sp, #128]\n"
    "stp x18, x19, [sp, #144]\n"
    "stp x20, x21, [sp, #160]\n"
    "stp x22, x23, [sp, #176]\n"
    "stp x24, x25, [sp, #192]\n"
    "stp x26, x27, [sp, #208]\n"
    "stp x28, x29, [sp, #224]\n"
    "str x30, [sp, #240]\n"
    "mov x0, sp\n"
    "bl exit_handler\n"
    "" HFI_GES(0)
    "lsr x0, x0, #2\n"
    "lsl x0, x0, #2\n"
    "str x0, [sp, #128]\n"
    "ldp x0, x1, [sp, #0]\n"
    "ldp x2, x3, [sp, #16]\n"
    "ldp x4, x5, [sp, #32]\n"
    "ldp x6, x7, [sp, #48]\n"
    "ldp x8, x9, [sp, #64]\n"
    "ldp x10, x11, [sp, #80]\n"
    "ldp x12, x13, [sp, #96]\n"
    "ldp x14, x15, [sp, #112]\n"
    "ldp x16, x17, [sp, #128]\n"
    "ldp x18, x19, [sp, #144]\n"
    "ldp x20, x21, [sp, #160]\n"
    "ldp x22, x23, [sp, #176]\n"
    "ldp x24, x25, [sp, #192]\n"
    "ldp x26, x27, [sp, #208]\n"
    "ldp x28, x29, [sp, #224]\n"
    "ldr x30, [sp, #240]\n"
    "add sp, sp, #256\n"
    "add x16, x16, #4\n"
    "adrp x17, hfi_config\n"
    "ldr x17, [x17, :lo12:hfi_config]\n"
    "" HFI_ENTER(17, 16));

uint64_t do_syscall(uint64_t regs[31])
{
    printf("  -> Proxying syscall: x8=%lu, x0=%lu, x1=%lu, x2=%lu, x3=%lu, x4=%lu, x5=%lu\n",
           regs[8], regs[0], regs[1], regs[2], regs[3], regs[4], regs[5]);

    uint64_t result;
    asm volatile(
        "mov x0, %[x0]\n"
        "mov x1, %[x1]\n"
        "mov x2, %[x2]\n"
        "mov x3, %[x3]\n"
        "mov x4, %[x4]\n"
        "mov x5, %[x5]\n"
        "mov x8, %[x8]\n"
        "svc #0\n"
        "str x0, %[result]\n"
        : [result] "=m"(result)
        : [x0] "r"(regs[0]), [x1] "r"(regs[1]), [x2] "r"(regs[2]),
          [x3] "r"(regs[3]), [x4] "r"(regs[4]), [x5] "r"(regs[5]),
          [x8] "r"(regs[8])
        : "x6", "x7", "x16", "x17", "x30", "cc", "memory");
    return result;
}

void exit_handler(uint64_t regs[31])
{
    uint64_t exit_state = do_hfi_ges();
    uint64_t exit_reason = HFI_EXIT_STATE_GET_REASON(exit_state);

    if (exit_reason == HFI_SYSCALL_REQUESTED) {
        regs[0] = do_syscall(regs);
    } else if (exit_reason == HFI_EXIT_CALLED) {
        asm volatile(
            "mov sp, %[trusted_sp]\n"
            "br %[trusted_pc]\n"
            :
            : [trusted_pc] "r"(hfi_end_addr), [trusted_sp] "r"(hfi_end_sp)
            : "x0", "memory");
    } else if (exit_reason == HFI_FAULT_OCCURRED) {
        uint64_t fault_state = do_hfi_gfs();
        uint64_t exit_pc = HFI_EXIT_STATE_GET_PC(exit_state);
        printf("  -> A fault occurred inside HFI.\n");
        printf("    - PC: 0x%lx\n", exit_pc);
        printf("    - Operation: %llu\n",
               HFI_FAULT_STATE_GET_OPERATION(fault_state));
        printf("    - Reason: %llu\n", HFI_FAULT_STATE_GET_REASON(fault_state));
        printf("    - Region ID: %llu\n",
               HFI_FAULT_STATE_GET_REGION_ID(fault_state));
        exit(1);
    }
}

// ============================================================================
// Helpers
// ============================================================================

static uint64_t align_down_u64(uint64_t value, uint64_t align)
{
    return value & ~(align - 1);
}

static uint64_t align_up_u64(uint64_t value, uint64_t align)
{
    return (value + align - 1) & ~(align - 1);
}

static hfi_region_t build_hfi_region(uint64_t start, uint64_t end)
{
    hfi_region_t region;
    uint64_t size = end - start;
    uint64_t block_size;
    uint64_t block_start;

    block_size = 1;
    while (block_size < size)
        block_size <<= 1;

    block_start = start & ~(block_size - 1);
    while (block_start + block_size < end) {
        block_size <<= 1;
        block_start = start & ~(block_size - 1);
    }

    region.base = block_start;
    region.bound = ~(block_size - 1);
    return region;
}

static int find_exec_mapping(void *addr, uint64_t *start, uint64_t *end)
{
    FILE *maps;
    char line[1024];
    uint64_t target;

    maps = fopen("/proc/self/maps", "r");
    if (maps == NULL) {
        perror("fopen(/proc/self/maps)");
        return 0;
    }

    target = (uint64_t)addr;
    while (fgets(line, sizeof(line), maps) != NULL) {
        unsigned long long map_start;
        unsigned long long map_end;
        char perms[5];

        if (sscanf(line, "%llx-%llx %4s", &map_start, &map_end, perms) != 3) {
            continue;
        }
        if (target < map_start || target >= map_end) {
            continue;
        }
        if (strchr(perms, 'x') == NULL) {
            continue;
        }

        *start = (uint64_t)map_start;
        *end = (uint64_t)map_end;
        fclose(maps);
        return 1;
    }

    fclose(maps);
    return 0;
}

static int find_object_mappings(const char *path, int want_exec,
                                uint64_t *start, uint64_t *end)
{
    FILE *maps;
    char line[1024];
    int found = 0;

    maps = fopen("/proc/self/maps", "r");
    if (maps == NULL) {
        perror("fopen(/proc/self/maps)");
        return 0;
    }

    while (fgets(line, sizeof(line), maps) != NULL) {
        unsigned long long map_start;
        unsigned long long map_end;
        char perms[5];

        if (strstr(line, path) == NULL) {
            continue;
        }
        if (sscanf(line, "%llx-%llx %4s", &map_start, &map_end, perms) != 3) {
            continue;
        }
        if ((strchr(perms, 'x') != NULL) != want_exec) {
            continue;
        }

        if (!found || map_start < *start) {
            *start = (uint64_t)map_start;
        }
        if (!found || map_end > *end) {
            *end = (uint64_t)map_end;
        }
        found = 1;
    }

    fclose(maps);
    return found;
}

static int find_shared_object_mappings(int want_exec,
                                       uint64_t *start, uint64_t *end)
{
    FILE *maps;
    char line[1024];
    int found = 0;

    maps = fopen("/proc/self/maps", "r");
    if (maps == NULL) {
        perror("fopen(/proc/self/maps)");
        return 0;
    }

    while (fgets(line, sizeof(line), maps) != NULL) {
        unsigned long long map_start;
        unsigned long long map_end;
        char perms[5];

        if (strstr(line, ".so") == NULL) {
            continue;
        }
        if (sscanf(line, "%llx-%llx %4s", &map_start, &map_end, perms) != 3) {
            continue;
        }
        if ((strchr(perms, 'x') != NULL) != want_exec) {
            continue;
        }

        if (!found || map_start < *start) {
            *start = (uint64_t)map_start;
        }
        if (!found || map_end > *end) {
            *end = (uint64_t)map_end;
        }
        found = 1;
    }

    fclose(maps);
    return found;
}

static int is_true(int val)
{
    return val == 1;
}

// ============================================================================
// Test Logic
// ============================================================================

int test_zlib_success(void)
{
    const char *lib_path = "./lib/libz_nice.so";
    char resolved_lib_path[512];
    const char *hello_msg =
        "Hello Hardware Fault Isolation! If you can read this, dynamic zlib "
        "sandboxing works while proxying syscalls.";
    const size_t sandbox_stack_size = 16 * 1024;
    const size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    const size_t source_len = strlen(hello_msg) + 1;
    const size_t alloc_pool_size = 1024 * 1024;
    size_t source_offset;
    size_t dest_offset;
    size_t alloc_pool_offset;
    size_t stack_offset;
    size_t alloc_size;
    void *zlib_handle = NULL;
    compress_bound_fn_t compress_bound_fn = NULL;
    deflate_init_fn_t deflate_init_fn = NULL;
    deflate_fn_t deflate_fn = NULL;
    deflate_end_fn_t deflate_end_fn = NULL;
    void *sandbox_mapping = MAP_FAILED;
    zlib_sandbox_ctx_t *ctx;
    uint8_t *sandbox_base;
    uint8_t *sandbox_sp;
    hfi_region_t harness_code_region;
    hfi_region_t zlib_code_region;
    hfi_region_t zlib_data_region;
    hfi_region_t sandbox_data_region;
    uint64_t harness_map_start;
    uint64_t harness_map_end;
    uint64_t foreign_exec_start;
    uint64_t foreign_exec_end;
    uint64_t foreign_data_start;
    uint64_t foreign_data_end;
    int ok = 0;

    if (realpath(lib_path, resolved_lib_path) == NULL) {
        perror("realpath");
        goto out;
    }

    zlib_handle = dlopen(resolved_lib_path, RTLD_NOW | RTLD_LOCAL);
    if (zlib_handle == NULL) {
        printf("  -> FAILED: dlopen(%s) failed: %s\n",
               resolved_lib_path, dlerror());
        goto out;
    }

    dlerror();
    compress_bound_fn = (compress_bound_fn_t)dlsym(zlib_handle, "compressBound");
    deflate_init_fn = (deflate_init_fn_t)dlsym(zlib_handle, "deflateInit_");
    deflate_fn = (deflate_fn_t)dlsym(zlib_handle, "deflate");
    deflate_end_fn = (deflate_end_fn_t)dlsym(zlib_handle, "deflateEnd");
    if (compress_bound_fn == NULL || deflate_init_fn == NULL ||
        deflate_fn == NULL || deflate_end_fn == NULL) {
        printf("  -> FAILED: dlsym() failed: %s\n", dlerror());
        goto out;
    }

    source_offset = align_up_u64(sizeof(*ctx), 16);
    dest_offset = align_up_u64(source_offset + source_len, 16);
    alloc_pool_offset =
        align_up_u64(dest_offset + compress_bound_fn(source_len), 16);
    stack_offset = align_up_u64(alloc_pool_offset + alloc_pool_size, 16);
    alloc_size = align_up_u64(stack_offset + sandbox_stack_size, page_size);

    sandbox_mapping = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE,
                           MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sandbox_mapping == MAP_FAILED) {
        perror("mmap");
        goto out;
    }

    sandbox_base = (uint8_t *)sandbox_mapping;
    ctx = (zlib_sandbox_ctx_t *)sandbox_base;
    memset(ctx, 0, sizeof(*ctx));
    ctx->deflate_init_fn = deflate_init_fn;
    ctx->deflate_fn = deflate_fn;
    ctx->deflate_end_fn = deflate_end_fn;
    ctx->source_buf = sandbox_base + source_offset;
    ctx->source_len = source_len;
    ctx->dest_buf = sandbox_base + dest_offset;
    ctx->dest_len = compress_bound_fn(source_len);
    ctx->alloc_pool = sandbox_base + alloc_pool_offset;
    ctx->alloc_pool_size = alloc_pool_size;
    ctx->alloc_pool_used = 0;
    ctx->zlib_result = -999;
    memcpy((void *)ctx->source_buf, hello_msg, source_len);

    sandbox_sp = sandbox_base + alloc_size - 16;

    if (!find_exec_mapping((void *)zlib_sandbox_entry,
                           &harness_map_start, &harness_map_end)) {
        printf("  -> FAILED: could not find harness executable mapping\n");
        goto out;
    }
    if (!find_shared_object_mappings(1, &foreign_exec_start, &foreign_exec_end)) {
        printf("  -> FAILED: could not find shared-object executable mappings\n");
        goto out;
    }
    if (!find_shared_object_mappings(0, &foreign_data_start, &foreign_data_end)) {
        printf("  -> FAILED: could not find shared-object data mappings\n");
        goto out;
    }

    harness_code_region = build_hfi_region(harness_map_start, harness_map_end);
    zlib_code_region = build_hfi_region(foreign_exec_start, foreign_exec_end);
    zlib_data_region = build_hfi_region(foreign_data_start, foreign_data_end);
    sandbox_data_region =
        build_hfi_region((uint64_t)sandbox_base, (uint64_t)sandbox_base + alloc_size);

    do_hfi_srb(0, harness_code_region.base);
    do_hfi_srm(0, harness_code_region.bound);
    do_hfi_srp(0, HFI_PERM_EXEC | HFI_PERM_READ);

    do_hfi_srb(1, zlib_code_region.base);
    do_hfi_srm(1, zlib_code_region.bound);
    do_hfi_srp(1, HFI_PERM_EXEC | HFI_PERM_READ);

    do_hfi_srb(3, sandbox_data_region.base);
    do_hfi_srm(3, sandbox_data_region.bound);
    do_hfi_srp(3, HFI_PERM_READ | HFI_PERM_WRITE);

    do_hfi_srb(4, zlib_data_region.base);
    do_hfi_srm(4, zlib_data_region.bound);
    do_hfi_srp(4, HFI_PERM_READ | HFI_PERM_WRITE);

    do_hfi_seh((uint64_t)exit_handler_scaffold);
    hfi_config = HFI_OPT_LOCK_REGIONS;

    asm volatile(
        "mov x0, %[ctx]\n"
        "mov x1, %[sandbox_sp]\n"
        "mov x20, %[entry]\n"
        "mov x21, %[hfi_config]\n"
        "adr x22, 1f\n"
        "str x22, %[hfi_end_addr]\n"
        "mov x22, sp\n"
        "str x22, %[hfi_end_sp]\n"
        "" HFI_ENTER(21, 20)
        "1:\n"
        : [hfi_end_addr] "=m"(hfi_end_addr), [hfi_end_sp] "=m"(hfi_end_sp)
        : [ctx] "r"(ctx), [sandbox_sp] "r"(sandbox_sp),
          [entry] "r"(zlib_sandbox_entry), [hfi_config] "r"(hfi_config)
        : "x0", "x1", "x20", "x21", "x22", "x29", "x30", "memory");

    if (HFI_FAULT_STATE_GET_OCCURRED(do_hfi_gfs())) {
        uint64_t fault_state = do_hfi_gfs();
        uint64_t exit_pc = HFI_EXIT_STATE_GET_PC(do_hfi_ges());
        printf("  -> FAILED: A fault occurred inside zlib.\n");
        printf("    - PC: 0x%lx\n", exit_pc);
        printf("    - Operation: %llu\n",
               HFI_FAULT_STATE_GET_OPERATION(fault_state));
        printf("    - Reason: %llu\n", HFI_FAULT_STATE_GET_REASON(fault_state));
        printf("    - Region ID: %llu\n",
               HFI_FAULT_STATE_GET_REGION_ID(fault_state));
        goto out;
    }

    if (ctx->zlib_result != Z_OK) {
        printf("  -> FAILED: zlib returned error %d\n",
               ctx->zlib_result);
        printf("    - alloc_count=%lu, alloc_used=%lu, last_alloc=%lu, alloc_failed=%d\n",
               ctx->alloc_count, ctx->alloc_pool_used, ctx->last_alloc_bytes,
               ctx->alloc_failed);
        goto out;
    }

    printf("  -> SUCCESS: Dynamically loaded zlib compressed %lu bytes down to %lu bytes inside HFI.\n",
           ctx->source_len, ctx->dest_len);
    ok = 1;

out:
    if (sandbox_mapping != MAP_FAILED) {
        munmap(sandbox_mapping, alloc_size);
    }
    if (zlib_handle != NULL) {
        dlclose(zlib_handle);
    }
    return ok;
}

TEST_MAIN(
    TEST(test_zlib_success(), is_true);)
