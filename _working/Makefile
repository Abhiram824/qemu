# Build system for HFI instruction testing with QEMU
# Usage: make [target]
# See: make help

# Compiler settings
ARCH = aarch64
CC = aarch64-linux-gnu-gcc
AS = aarch64-linux-gnu-as
OBJDUMP = aarch64-linux-gnu-objdump

# Compiler flags
CFLAGS = -static
ASFLAGS = 
LDFLAGS = -static

# QEMU runtime flags
QEMU_RUN_FLAGS = -one-insn-per-tb

# Directories
TEST_DIR = tests
OUT_DIR = obj
LIB_DIR = lib
QEMU_BUILD = ../build
QEMU_BIN = $(QEMU_BUILD)/qemu-aarch64
QEMU_SYSROOT = /usr/aarch64-linux-gnu

# Zlib source directories (local copies)
NICE_ZLIB_DIR    = _nice_zlib
MALICIOUS_ZLIB_DIR = _malicious_zlib

# Get all test sources and derive names
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_NAMES = $(basename $(notdir $(TEST_SOURCES)))

# Generated files
TEST_ASM = $(addprefix $(OUT_DIR)/,$(addsuffix .s,$(TEST_NAMES)))
TEST_BIN = $(addprefix $(OUT_DIR)/,$(TEST_NAMES))
TEST_DUMPS = $(addprefix $(OUT_DIR)/,$(addsuffix .dump,$(TEST_NAMES)))

# Find all HFI-related files in the parent directory, excluding build and hidden directories
HFI_FILES = $(shell find .. -type f -name "*hfi*" | grep -vE "(/build/|/\.[^/]+/|/_)")

# Declare all targets as phony
.PHONY: all configure qemu qemu-rebuild asm bin build run run-% clean clean-reset help status git-push debug-% objdump libs

# Default target
all: qemu libs asm bin
	@echo "[BUILD] All tests compiled successfully"

# ============================================================================
# GIT AND WORKSPACE MANAGEMENT
# ============================================================================

git-push:
	@git add ..
	@git status
	@read -p "Enter commit message: " FILENAME; \
	git commit -m "$$FILENAME"
	git push

git-pull:
	@git pull

workspace:
	@echo "Finding HFI files with grep, excluding build directory and directories starting with . or _ ..."
	@echo "Found HFI files:"
	@for file in $(HFI_FILES); do \
		echo "  $$file"; \
	done
	@echo ""
	@printf "Do you want to open these files in VS Code? [y/N]: "; \
	read ans; \
	if [ "$$ans" = "y" ] || [ "$$ans" = "Y" ]; then \
		code -r $(HFI_FILES); \
	else \
		echo "Opening cancelled."; \
	fi


# ============================================================================
# QEMU Build
# ============================================================================

configure:
	@if [ -d "../build" ]; then \
		echo "[CONFIGURE] Found Build Configuration"; \
	else \
		echo "[CONFIGURE] Building Configuration for aarch64"; \
		sudo apt update; \
		sudo apt install ninja-build; \
		sudo apt install libglib2.0-dev; \
		mkdir -p ../build; \
		cd ../build && ../configure --target-list="aarch64-linux-user"; \
		echo "[CONFIGURE] Building Complete for aarch64"; \
	fi

	@if ! command -v aarch64-linux-gnu-gcc; then \
		echo "[CONFIGURE] Installing aarch64-linux-gnu-gcc"; \
		sudo apt update; \
		sudo apt install gcc-aarch64-linux-gnu; \
	fi

qemu: configure
	@echo "[QEMU] Building QEMU with make -j$$(nproc) -C $(QEMU_BUILD)..."
	@make -j$$(nproc) -C $(QEMU_BUILD)
	@if [ -f "$(QEMU_BIN)" ]; then \
		echo "[QEMU] ✓ Build successful"; \
	else \
		echo "[QEMU] ✗ Build failed"; \
		exit 1; \
	fi
	@echo ""

qemu-rebuild:
	@echo "[QEMU] Cleaning QEMU build directory..."
	@make -C $(QEMU_BUILD) clean > /dev/null 2>&1
	@echo "[QEMU] Rebuilding QEMU with make -j$$(nproc) -C $(QEMU_BUILD)..."
	@make -j$$(nproc) -C $(QEMU_BUILD)
	@if [ -f "$(QEMU_BIN)" ]; then \
		echo "[QEMU] ✓ Rebuild successful"; \
	else \
		echo "[QEMU] ✗ Rebuild failed"; \
		exit 1; \
	fi
	@echo ""

# ============================================================================
# Output Directory
# ============================================================================

$(OUT_DIR):
	@mkdir -p $(OUT_DIR)

# ============================================================================
# Header File Dependencies
# ============================================================================

HEADERS = $(wildcard *.h) $(wildcard $(TEST_DIR)/*.h)

# ============================================================================
# Zlib Library Builds (from local source copies)
# ============================================================================

# Build nice zlib and place static lib in lib/
$(LIB_DIR)/libz.a: $(wildcard $(NICE_ZLIB_DIR)/*.c $(NICE_ZLIB_DIR)/*.h)
	@echo "[LIBS] Building nice zlib from $(NICE_ZLIB_DIR)..."
	@$(MAKE) -C $(NICE_ZLIB_DIR) libz.a
	@cp $(NICE_ZLIB_DIR)/libz.a $@
	@echo "[LIBS] ✓ $(LIB_DIR)/libz.a ready"

# Build malicious zlib and place static lib in lib/
$(LIB_DIR)/libz_malicious.a: $(wildcard $(MALICIOUS_ZLIB_DIR)/*.c $(MALICIOUS_ZLIB_DIR)/*.h)
	@echo "[LIBS] Building malicious zlib from $(MALICIOUS_ZLIB_DIR)..."
	@$(MAKE) -C $(MALICIOUS_ZLIB_DIR) libz.a
	@cp $(MALICIOUS_ZLIB_DIR)/libz.a $@
	@echo "[LIBS] ✓ $(LIB_DIR)/libz_malicious.a ready"

# Convenience target to build both static libs
libs: $(LIB_DIR)/libz.a $(LIB_DIR)/libz_malicious.a
	@echo "[LIBS] ✓ All zlib variants built"

# ============================================================================
# Test-Specific Library Configurations
# ============================================================================

# Configuration for test_zlib
ZLIB_NICE_SO = $(LIB_DIR)/libz_nice.so
$(ZLIB_NICE_SO): $(LIB_DIR)/libz.a
	@echo "[SO] Linking: $< -> $@"
	$(CC) -shared -o $@ -Wl,--whole-archive $< -Wl,--no-whole-archive
$(OUT_DIR)/test_zlib_nice: CFLAGS := $(filter-out -static,$(CFLAGS))
$(OUT_DIR)/test_zlib_nice: CFLAGS += -fno-stack-protector
$(OUT_DIR)/test_zlib_nice: LDFLAGS := $(filter-out -static,$(LDFLAGS))
$(OUT_DIR)/test_zlib_nice: LDLIBS += -ldl
$(OUT_DIR)/test_zlib_nice: $(ZLIB_NICE_SO)

ZLIB_EVIL_SO = $(LIB_DIR)/libz_evil.so
$(ZLIB_EVIL_SO): $(LIB_DIR)/libz_malicious.a
	@echo "[SO] Linking: $< -> $@"
	$(CC) -shared -o $@ -Wl,--whole-archive $< -Wl,--no-whole-archive
$(OUT_DIR)/test_zlib_evil: CFLAGS := $(filter-out -static,$(CFLAGS))
$(OUT_DIR)/test_zlib_evil: CFLAGS += -fno-stack-protector
$(OUT_DIR)/test_zlib_evil: LDFLAGS := $(filter-out -static,$(LDFLAGS))
$(OUT_DIR)/test_zlib_evil: LDLIBS += -ldl
$(OUT_DIR)/test_zlib_evil: $(ZLIB_EVIL_SO)

# ============================================================================
# Assembly Generation (C -> .s)
# ============================================================================

$(OUT_DIR)/%.s: $(TEST_DIR)/%.c $(HEADERS) | $(OUT_DIR)
	@echo "[ASM] Compiling: $< -> $@"
	@$(CC) $(CFLAGS) -S $< -o $@

# ============================================================================
# Binary Compilation (C -> binary)
# ============================================================================

$(OUT_DIR)/%: $(TEST_DIR)/%.c $(HEADERS) | $(OUT_DIR)
	@echo "[BIN] Compiling: $< -> $@"
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@ $(LDLIBS)

# ============================================================================
# Disassembly Generation (binary -> .dump)
# ============================================================================

$(OUT_DIR)/%.dump: $(OUT_DIR)/%
	@echo "[DUMP] Disassembling: $< -> $@"
	@$(OBJDUMP) -d $< > $@

# ============================================================================
# Build Targets
# ============================================================================

# Compile all assembly files
asm: $(TEST_ASM)
	@echo "[ASM] ✓ All assembly files generated"

# Compile all binary files
bin: qemu $(TEST_BIN)
	@echo "[BIN] ✓ All binaries compiled"
	@echo ""

# Compile both assembly and binaries
build: asm bin
	@echo "[BUILD] ✓ All targets built"

# Generate disassembly dumps
objdump: bin $(TEST_DUMPS)
	@echo "[DUMP] ✓ All disassembly files generated"

# ============================================================================
# Run Targets
# ============================================================================

# Run a specific test: make run-testname
run-%: qemu $(OUT_DIR)/%
	@echo "[RUN] Executing: $*"
	@echo ">>>>>"
	@if [ "$*" = "test_zlib_nice" ] || [ "$*" = "test_zlib_evil" ]; then \
		$(QEMU_BIN) -L $(QEMU_SYSROOT) $(QEMU_RUN_FLAGS) $(OUT_DIR)/$*; \
	else \
		$(QEMU_BIN) $(QEMU_RUN_FLAGS) $(OUT_DIR)/$*; \
	fi
	@echo "<<<<<"
	@echo "[DONE] Test completed with exit code: $$?"

# Debug a specific test: make debug-testname
debug-%: qemu $(OUT_DIR)/%
	@echo "[DEBUG] Running: $* with gdb"
	@if [ "$*" = "test_zlib_nice" ] || [ "$*" = "test_zlib_evil" ]; then \
		gdb --args $(QEMU_BIN) -L $(QEMU_SYSROOT) $(QEMU_RUN_FLAGS) $(OUT_DIR)/$*; \
	else \
		gdb --args $(QEMU_BIN) $(QEMU_RUN_FLAGS) $(OUT_DIR)/$*; \
	fi

# Run all tests
run: qemu $(TEST_BIN)
	@echo ""
	@passed=0; failed=0; \
	for test in $(TEST_BIN); do \
		testname=$$(basename $$test); \
		echo "[RUN] $$testname"; \
		if [ "$$testname" = "test_zlib_nice" ] || [ "$$testname" = "test_zlib_evil" ]; then \
			run_cmd="$(QEMU_BIN) -L $(QEMU_SYSROOT) $(QEMU_RUN_FLAGS) $$test"; \
		else \
			run_cmd="$(QEMU_BIN) $(QEMU_RUN_FLAGS) $$test"; \
		fi; \
		if sh -c "$$run_cmd"; then \
			echo "[PASS] $$testname"; \
			passed=$$((passed + 1)); \
		else \
			echo "[FAIL] $$testname (exit: $$?)"; \
			failed=$$((failed + 1)); \
		fi; \
		echo ""; \
	done; \
	echo "[SUMMARY] Passed: $$passed, Failed: $$failed"; \
	if [ $$failed -eq 0 ]; then \
		echo "[SUCCESS] All tests passed!"; \
	else \
		exit 1; \
	fi

# ============================================================================
# Maintenance Targets
# ============================================================================

# Clean build artifacts
clean:
	@echo "[CLEAN] Removing output directory..."
	@rm -rf $(OUT_DIR)
	@echo "[CLEAN] Cleaning zlib source trees..."
	@$(MAKE) -C $(NICE_ZLIB_DIR) clean > /dev/null 2>&1 || true
	@$(MAKE) -C $(MALICIOUS_ZLIB_DIR) clean > /dev/null 2>&1 || true
	@echo "[CLEAN] Removing lib artifacts..."
	@rm -f $(LIB_DIR)/libz.a $(LIB_DIR)/libz_malicious.a $(LIB_DIR)/libz_nice.so $(LIB_DIR)/libz_evil.so
	@echo "[CLEAN] ✓ Done"

# Clean everything including QEMU build
clean-reset: clean
	@echo "[CLEAN] Cleaning QEMU build directory..."
	@$(MAKE) -C $(QEMU_BUILD) clean > /dev/null 2>&1 || true
	@echo "[CLEAN] ✓ Full reset done"

# Clean disassembly dumps only
clean-dumps:
	@echo "[CLEAN] Removing disassembly files..."
	@rm -f $(TEST_DUMPS)
	@echo "[CLEAN] ✓ Done"

# Show help
help:
	@echo "╔════════════════════════════════════════════════════════════════╗"
	@echo "║        HFI Instruction Testing Build System (QEMU)             ║"
	@echo "╚════════════════════════════════════════════════════════════════╝"
	@echo ""
	@echo "MAIN TARGETS:"
	@echo "  make all         - Build QEMU and compile all tests (default)"
	@echo "  make help        - Show this help message"
	@echo ""
	@echo "BUILD TARGETS:"
	@echo "  make qemu        - Build/rebuild QEMU"	@echo "  make qemu-rebuild - Clean and rebuild QEMU from scratch"	@echo "  make asm         - Compile all tests to assembly (.s files)"
	@echo "  make bin         - Compile all binaries"
	@echo "  make build       - Build both assembly and binaries"
	@echo "  make clean       - Remove all generated files ($(BUILD_DIR)/ directory)"
	@echo ""
	@echo "TEST TARGETS:"
	@echo "  make run         - Run all compiled tests with summary"
	@echo "  make run-TESTNAME - Run a specific test (e.g., make run-basic)"
	@echo ""
	@echo "UTILITY TARGETS:"
	@echo "  make status      - Show build system configuration"
	@echo "  make objdump     - Generate disassembly dumps for all binaries"
	@echo ""
	@echo "CLEANUP:"
	@echo "  make clean       - Remove all build artifacts"
	@echo "  make clean-dumps - Remove only disassembly dump files"
	@echo ""
	@echo "AVAILABLE TESTS:"
	@if [ -n "$(TEST_SOURCES)" ]; then \
		for test in $(TEST_NAMES); do echo "    • $$test"; done; \
	else \
		echo "    (no tests found in $(TEST_DIR)/)"; \
	fi
	@echo ""
	@echo "EXAMPLE USAGE:"
	@echo "  make              # Build QEMU and all tests"
	@echo "  make objdump      # Generate disassembly dumps for all tests"
	@echo "  make run-basic    # Run the 'basic' test"
	@echo "  make run          # Run all tests"
	@echo "  make clean        # Clean build artifacts"
	@echo ""
	@echo "OUTPUT LOCATION: $(OUT_DIR)/"
	@echo "QEMU: $(QEMU_BIN)"

# Show status
status:
	@echo "Build System Configuration:"
	@echo "  Architecture:     $(ARCH)"
	@echo "  Compiler:         $(CC)"
	@echo "  Assembler:        $(AS)"
	@echo "  Objdump:          $(OBJDUMP)"
	@echo "  CFLAGS:           $(CFLAGS)"
	@echo "  Test directory:   $(TEST_DIR)/"
	@echo "  Output directory: $(OUT_DIR)/"
	@echo "  QEMU location:    $(QEMU_BIN)"
	@echo ""
	@echo "Toolchain Binaries:"
	@for tool in $(CC) $(AS) $(OBJDUMP) aarch64-linux-gnu-ld; do \
		if command -v $$tool > /dev/null 2>&1; then \
			echo "  [✓] $$tool  ($$(command -v $$tool))"; \
		else \
			echo "  [✗] $$tool  (NOT FOUND)"; \
		fi; \
	done
	@echo ""
	@echo "QEMU Binary:"
	@if [ -f "$(QEMU_BIN)" ]; then \
		echo "  [✓] $(QEMU_BIN)"; \
	else \
		echo "  [✗] $(QEMU_BIN)  (not built — run: make qemu)"; \
	fi
	@echo ""
	@echo "Zlib Libraries ($(LIB_DIR)/):"
	@for lib in libz.a libz_malicious.a libz_nice.so libz_evil.so; do \
		if [ -f "$(LIB_DIR)/$$lib" ]; then \
			echo "  [✓] $(LIB_DIR)/$$lib"; \
		else \
			echo "  [✗] $(LIB_DIR)/$$lib  (not built — run: make libs)"; \
		fi; \
	done
	@echo ""
	@echo "Test Sources:"
	@if [ -n "$(TEST_SOURCES)" ]; then \
		echo "$(TEST_SOURCES)" | tr ' ' '\n' | sed 's/^/  /'; \
	else \
		echo "  (none found in $(TEST_DIR)/)"; \
	fi
	@echo ""
	@echo "Assembly Files ($(OUT_DIR)/*.s):"
	@if [ -d "$(OUT_DIR)" ] && ls $(OUT_DIR)/*.s > /dev/null 2>&1; then \
		for f in $(OUT_DIR)/*.s; do echo "  [✓] $$f"; done; \
	else \
		echo "  (none — run: make asm)"; \
	fi
	@echo ""
	@echo "Compiled Binaries ($(OUT_DIR)/):"
	@if [ -d "$(OUT_DIR)" ]; then \
		found=0; \
		for name in $(TEST_NAMES); do \
			if [ -f "$(OUT_DIR)/$$name" ]; then \
				echo "  [✓] $(OUT_DIR)/$$name"; \
				found=1; \
			else \
				echo "  [✗] $(OUT_DIR)/$$name  (not built — run: make bin)"; \
			fi; \
		done; \
		if [ "$$found" = "0" ]; then echo "  (none — run: make bin)"; fi; \
	else \
		echo "  ($(OUT_DIR) does not exist — run: make bin)"; \
	fi
