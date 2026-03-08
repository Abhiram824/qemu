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

# Directories
TEST_DIR = tests
OUT_DIR = obj
QEMU_BUILD = ../build
QEMU_BIN = $(QEMU_BUILD)/qemu-aarch64

# Get all test sources and derive names
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_NAMES = $(basename $(notdir $(TEST_SOURCES)))

# Generated files
TEST_ASM = $(addprefix $(OUT_DIR)/,$(addsuffix .s,$(TEST_NAMES)))
TEST_BIN = $(addprefix $(OUT_DIR)/,$(TEST_NAMES))

# Declare all targets as phony
.PHONY: all qemu qemu-rebuild asm bin build run run-% clean help status

# Default target
all: qemu asm bin
	@echo "[BUILD] All tests compiled successfully"

# ============================================================================
# QEMU Build
# ============================================================================

qemu:
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
# Assembly Generation (C -> .s)
# ============================================================================

$(OUT_DIR)/%.s: $(TEST_DIR)/%.c | $(OUT_DIR)
	@echo "[ASM] Compiling: $< -> $@"
	@$(CC) $(CFLAGS) -S $< -o $@

# ============================================================================
# Binary Compilation (C -> binary)
# ============================================================================

$(OUT_DIR)/%: $(TEST_DIR)/%.c | $(OUT_DIR)
	@echo "[BIN] Compiling: $< -> $@"
	@$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

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

# ============================================================================
# Run Targets
# ============================================================================

# Run a specific test: make run-testname
run-%: qemu $(OUT_DIR)/%
	@echo "[RUN] Executing: $*"
	@$(QEMU_BIN) $(OUT_DIR)/$*
	@echo "[DONE] Test completed with exit code: $$?"

# Run all tests
run: qemu $(TEST_BIN)
	@echo ""
	@passed=0; failed=0; \
	for test in $(TEST_BIN); do \
		testname=$$(basename $$test); \
		echo "[RUN] $$testname"; \
		if $(QEMU_BIN) $$test; then \
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
	@echo "  CFLAGS:           $(CFLAGS)"
	@echo "  Test directory:   $(TEST_DIR)/"
	@echo "  Output directory: $(OUT_DIR)/"
	@echo "  QEMU location:    $(QEMU_BIN)"
	@echo ""
	@echo "Test Files:"
	@if [ -n "$(TEST_SOURCES)" ]; then \
		echo "$(TEST_SOURCES)" | tr ' ' '\n' | sed 's/^/  /'; \
	else \
		echo "  (none found)"; \
	fi
	@echo ""
	@echo "Generated Files:"
	@if [ -d "$(OUT_DIR)" ]; then \
		ls -1 $(OUT_DIR)/ 2>/dev/null | sed 's/^/  /'; \
	else \
		echo "  ($(OUT_DIR) does not exist yet)"; \
	fi
