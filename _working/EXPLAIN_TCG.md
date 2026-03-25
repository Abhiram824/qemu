# TCG (Tiny Code Generator) Architecture Explained

## Overview

TCG is QEMU's intermediate representation (IR) layer that sits between instruction translators (like ARM64) and machine code backends. Instead of directly generating native code for each architecture, QEMU translators generate TCG operations, which are then compiled to the target backend's native machine code.

**Key Design Pattern**: Batch processing — operations are queued during translation, then compiled all-at-once by `tcg_gen_code()`.

---

## Operation Pipeline

TCG follows a three-phase execution model:

```
Guest Instruction Translation     Batch Compilation        Native Execution
     (translate-a64.c)               (tcg_gen_code)          (Machine Code)
           ↓                              ↓                          ↓
    Queue TCG Ops              Process queued ops         Compiled ops run
    (tcg_gen_qemu_*            Call backend codegen       on host CPU
     tcg_temp_new_i64, etc)    Register allocation
```

### Phase 1: Translation (Synchronous)
- Translator (e.g., `translate-a64.c`) reads guest instructions
- Calls TCG functions to queue operations: `tcg_gen_qemu_ld_i64()`, `tcg_gen_add_i64()`, etc.
- Operations are added to `tcg_ctx->ops` queue
- Temporaries are allocated during this phase

### Phase 2: Batch Compilation (Synchronous)
- `tcg_gen_code()` processes **all** queued operations at once
- Calls optimization passes (dead code elimination, constant folding)
- Runs register allocation
- Calls backend-specific code generation (e.g., `tcg/aarch64/tcg-target.c`)
- Generates native machine code into `tb->tc.ptr`

### Phase 3: Execution (Asynchronous)
- Native compiled code executes on host CPU
- No more TCG involvement needed

**Critical Insight**: Operations don't execute immediately. They're queued and compiled together, which enables safe temporary freeing.

---

## Temporary Variable Types & Lifecycle

TCG supports five types of temporaries, each with different scope and lifetime:

### 1. Register Globals (`TEMP_FIXED`)
**Lifetime**: Program lifetime  
**Scope**: System-wide  
**Storage**: CPU register (fixed allocation)  
**Allocation**: `tcg_global_reg_new_internal()`

```c
// Example: tcg_env points to CPU_ENV in host register (x19 on ARM64)
TCGv_ptr tcg_env = tcg_global_reg_new_ptr(TCG_AREG0, "env");
```

**Use case**: Values that must be in the same register for entire program (environment pointer, special registers)

### 2. Memory Globals (`TEMP_GLOBAL`)
**Lifetime**: Program lifetime  
**Scope**: System-wide  
**Storage**: Fixed offset in memory  
**Allocation**: `tcg_global_mem_new_i64()`, `tcg_global_mem_new_i32()`

```c
// Example from translate-a64.c
cpu_pc = tcg_global_mem_new_i64(tcg_env, offsetof(CPUARMState, pc), "pc");
for (i = 0; i < 32; i++) {
    cpu_X[i] = tcg_global_mem_new_i64(tcg_env, 
                offsetof(CPUARMState, xregs[i]), regnames[i]);
}
```

**Use case**: CPU state that needs to be in memory (registers, program counter, flags). Automatically synced to/from memory at block boundaries.

### 3. EBB Temporaries (`TEMP_EBB`)
**Lifetime**: Extended Basic Block  
**Scope**: Block-local  
**Storage**: Stack or register (allocated by backend)  
**Allocation**: `tcg_temp_ebb_new_i64()`, `tcg_temp_ebb_new_i32()`

```c
// Example from plugin-gen.c
TCGv_i32 cpu_index = tcg_temp_ebb_new_i32();
tcg_gen_call2(..., tcgv_i32_temp(cpu_index), ...);  // Queue op
tcg_temp_free_i32(cpu_index);                        // Free slot immediately
```

**Use case**: Intermediate computation values within a single block. Slots are reused after `tcg_gen_code()` completes.

**Key Property**: `tcg_temp_free_{i32,i64}()` marks slot as reusable **immediately** (see "Temp Reuse Mechanism" below).

### 4. TB Temporaries (`TEMP_TB`)
**Lifetime**: Translation Block  
**Scope**: Full block (may span multiple basic blocks)  
**Storage**: Stack or register  
**Allocation**: `tcg_temp_new_i64()`, `tcg_temp_new_i32()`

```c
TCGv_i64 long_lived = tcg_temp_new_i64();
// ... use throughout TB ...
tcg_temp_free_i64(long_lived);  // Optional (freed at TB boundary)
```

**Use case**: Values computed once and reused across multiple basic blocks.

### 5. Constants (`TEMP_CONST`)
**Lifetime**: Compile-time  
**Scope**: N/A  
**Storage**: Embedded in temp structure  
**Allocation**: `tcg_constant_i64()`, `tcg_constant_i32()`

```c
TCGv_i64 const_val = tcg_constant_i64(0x1000);  // Optimized by backend
```

**Use case**: Literal values. Backend optimizes these (e.g., uses immediate instructions).

---

## Context Management

### Global Context Structure

TCG manages multiple contexts (one per CPU in system-mode):

```c
// tcg/tcg.c
__thread TCGContext *tcg_ctx;           // Thread-local: current active context
TCGContext tcg_init_ctx;                // Global template context
TCGContext **tcg_ctxs;                  // System-mode: array of all CPU contexts
int tcg_ctxs_num;                       // Number of active contexts
```

### Context Lifecycle

1. **Initialization** (`tcg_init(max_cpus)`):
   - Initializes `tcg_init_ctx` (template)
   - Allocates `tcg_ctxs` array: `tcg_ctxs = g_new0(TCGContext *, max_cpus)`
   - Sets `tcg_ctx = &tcg_init_ctx`

2. **Per-CPU Registration** (`tcg_register_thread()`):
   - Allocates new context: `s = g_new0(TCGContext, 1)`
   - Copies template from `tcg_init_ctx`
   - **Relinking**: Updates `mem_base` pointers to point to new context's temps array
   - Stores in `tcg_ctxs[cpu_index]`

### Memory-Based Linking (`mem_base`)

Temporaries can be allocated at fixed offsets from other temporaries. The `mem_base` pointer tracks dependency:

```c
struct TCGTemp {
    TCGType type;
    TCGTempKind kind;
    TCGv_ptr mem_base;    // Points to base temporary (relative offset only)
    intptr_t mem_offset;  // Offset from mem_base
    // ...
};
```

**Relinking Process** (when registering new thread):
```c
// tcg/tcg.c lines 1002-1010
for (i = 0, n = tcg_init_ctx.nb_globals; i < n; ++i) {
    if (tcg_init_ctx.temps[i].mem_base) {
        ptrdiff_t b = tcg_init_ctx.temps[i].mem_base - tcg_init_ctx.temps;
        s->temps[i].mem_base = &s->temps[b];  // Recalculate offset in new context
    }
}
```

This ensures that when a new CPU context is created, dependent memory temporaries still point to correct offsets in the new temps array.

---

## Temporary Allocation & Reuse Mechanism

### How Allocation Works

**Temporaries are stored in array `tcg_ctx->temps[]`**:

```c
// tcg/tcg.c line 1699
static TCGTemp *tcg_temp_alloc(TCGContext *s)
{
    int n = s->nb_temps++;
    tcg_debug_assert(n < TCG_MAX_TEMPS);
    return memset(&s->temps[n], 0, sizeof(TCGTemp));
}
```

**Key**: Every allocation increments `nb_temps`, always allocating the **next slot**.

### How Freeing Works (The Critical Insight)

```c
// tcg/tcg.c line 1962
static void tcg_temp_free_internal(TCGContext *s, TCGTemp *ts)
{
    tcg_debug_assert(ts->temp_allocated != 0);
    ts->temp_allocated = 0;
    
    // Mark slot as reusable in bitmap
    if (ts->kind == TEMP_EBB) {
        set_bit(ts->base.n, s->free_temps[ts->type]);
    }
}
```

**Freed temps do NOT delete data**. They:
1. Set `temp_allocated = 0` flag
2. Mark slot as reusable in `free_temps[]` bitmap
3. Data persists in `temps[]` array

### Why Freeing Immediately is Safe

**Timeline**:
```
1. Allocate slot 10    → temp10 created, nb_temps becomes 11
2. Queue operation    → Operation stores pointer to &temps[10]
3. Free slot 10       → Mark as reusable, data persists
4. Allocate slot 11   → Next allocation gets nb_temps++, slot 11 (skips 10)
5. tcg_gen_code()     → Compile all queued ops
                         Operations still see valid &temps[10]
6. tcg_temp_ebb_reset_freed() → NOW slot 10 can be recycled
```

Operations store **pointers** to temp slots, not values. Data persists in the array until code generation completes.

### Slot Increment Strategy

New allocations always claim the **next slot** via `nb_temps++`. Freed slots sit in `free_temps[]` bitmap but are **skipped by new allocations**. This means:

- Freed temp at slot 10
- New allocation gets slot 11 (not 10)
- No overwrite risk; two different operations can't access same slot

**Actual reuse** happens only after `tcg_gen_code()` calls `tcg_temp_ebb_reset_freed()`:

```c
// tcg/tcg.c (called from tcg_gen_code)
static void tcg_temp_ebb_reset_freed(TCGContext *s)
{
    for (int i = 0; i < ARRAY_SIZE(s->free_temps); ++i) {
        bitmap_zero(s->free_temps[i], s->nb_temps);
    }
}
```

Now the bitmap is cleared, and future allocations can recycle freed slots.

---

## Code Generation Abstraction Layers

### Translation Block (TB)
A **Translation Block** (full guest function/section):
- Contains **multiple basic blocks**
- One entry point, multiple exits (branches)
- All operations compiled together in **single `tcg_gen_code()` call**
- Temporaries persist for entire TB compilation

**Example**: ARM64 function with multiple branch targets compiles as one TB.

### Basic Block (BB)
A **Basic Block** (linear code segment):
- No branches in the middle
- Single entry, single exit
- Terminated by branch/jump/return
- Multiple BBs exist within one TB

### Extended Basic Block (EBB)
An **Extended Basic Block**:
- Sub-unit of BB (usually same as BB, but can span empty BBs)
- `TEMP_EBB` temporaries freed at EBB boundaries
- Within same TB, EBB temps can be recycled
- At block boundary, `set_label` op triggers `tcg_reg_alloc_bb_end()`

**Relationship**:
```
Translation Block (Full Guest Function)
├─ Extended Basic Block 1 (linear code)
│  └─ TEMP_EBB freed here
├─ Label + Branch
├─ Extended Basic Block 2  
│  └─ TEMP_EBB freed here
├─ Label + Branch
└─ ...all compiled together...
```

---

## Key Code Files & Functions

### [tcg/tcg.c](tcg/tcg.c)

**Context Initialization**:
- `tcg_init(max_cpus)` - Global initialization
- `tcg_context_init()` - Set up context array
- `tcg_register_thread()` - Register per-CPU context with relinking

**Temporary Allocation**:
- `tcg_temp_alloc()` (line 1699) - `nb_temps++` to get next slot
- `tcg_global_mem_new_internal()` (line 1745) - Create memory-based global
- `tcg_global_reg_new_internal()` - Create register-based global
- `tcg_temp_ebb_new_i64()` - Allocate EBB temporary

**Temporary Freeing**:
- `tcg_temp_free_internal()` (line 1962) - Mark slot reusable

**Code Generation**:
- `tcg_gen_code()` (line 6310) - Batch compile operations
  - Line 6352: `tcg_temp_ebb_reset_freed(s)` - Clear free bitmap
  - Calls optimization and register allocation
  - Invokes backend code generation
  - Returns compiled code size

### [target/arm/tcg/translate-a64.c](target/arm/tcg/translate-a64.c)

**Global Temporaries** (lines 84-95):
```c
void a64_translate_init(void)
{
    cpu_pc = tcg_global_mem_new_i64(tcg_env, offsetof(CPUARMState, pc), "pc");
    for (i = 0; i < 32; i++) {
        cpu_X[i] = tcg_global_mem_new_i64(tcg_env, 
                        offsetof(CPUARMState, xregs[i]), regnames[i]);
    }
    cpu_exclusive_high = tcg_global_mem_new_i64(...);
    // ... more globals ...
}
```

**Load/Store Translation**:
- Functions like `disas_ldst_*()`, `gen_ldst_op()` call `tcg_gen_qemu_ld_*()`/`tcg_gen_qemu_st_*()`
- **Note**: No HFI checks yet — this is where bounds checking injection should go

### [target/arm/tcg/translate-hfi.c](target/arm/tcg/translate-hfi.c)

**HFI Instruction Translators** (lines 82-115):
- `trans_CSTM()` - HFI state machine instruction
- `trans_HFI_SR()` - HFI save/restore instruction

**⚠️ BUG IDENTIFIED**: Temporaries allocated but never freed
```c
TCGv_i64 value1 = tcg_temp_new_i64();
TCGv_i64 value2 = tcg_temp_new_i64();
TCGv_i64 addr2 = tcg_temp_new_i64();
// ... use them ...
// MISSING: tcg_temp_free_i64(value1); tcg_temp_free_i64(value2); tcg_temp_free_i64(addr2);
```

**Impact**: Temps leak, wasting slots in same TB. Fix: add free calls before return.

### [accel/tcg/plugin-gen.c](accel/tcg/plugin-gen.c)

**Proper Temp Management Pattern** (lines 107-127):
```c
static void gen_cpu_index(TCGContext *s)
{
    TCGv_i32 cpu_index = tcg_temp_ebb_new_i32();
    tcg_gen_call2(..., tcgv_i32_temp(cpu_index), ...);  // Queue op
    tcg_temp_free_i32(cpu_index);                        // Free immediately
}
```

Shows standard pattern: alloc → queue ops → free.

---

## Summary: Memory Temporal Safety

### The Key Design Pattern

1. **Allocation is sequential**: `nb_temps++` always gets next slot
2. **Freeing marks, doesn't delete**: Data persists, slot marked reusable
3. **Operations store references**: Each op has pointers to temp slots
4. **Compilation is atomic**: All ops compiled before any freed slot is recycled
5. **Reset is explicit**: Only after `tcg_gen_code()` completes are freed slots actually reused

This enables:
- Safe temporary freeing (data persists through compilation)
- Slot reuse efficiency (freed slots marked but not immediately recycled)
- Thread-safe context management (relinking handles multi-CPU scenarios)
- Scope-based cleanup (EBB temps freed at block boundaries)

### For HFI Implementation

When implementing HFI bounds checks on all loads/stores:

1. Find all `tcg_gen_qemu_ld_*()` and `tcg_gen_qemu_st_*()` calls in translate-a64.c
2. Before each load/store, inject HFI check operations:
   ```c
   TCGv_i64 addr = ...;  // Address from load/store
   gen_hfi_check(addr, memop, is_write);  // Generate check ops
   tcg_gen_qemu_ld_i64(..., addr, ...);   // Original load
   ```
3. Remember to free temporary slots used in HFI check
4. Ensure check ops reference valid data (EBB temps are fine; freed immediately after check)

---

## References

- `tcg/tcg.c` - Core TCG infrastructure (1500+ function, context management)
- `target/arm/tcg/translate-a64.c` - ARM64 translator using TCG
- `tcg/README` - TCG operation reference
- QEMU Documentation: https://qemu.readthedocs.io/

