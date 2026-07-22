CC := gcc
CFLAGS := \
    -std=c11 \
    -Wall \
    -Wextra \
    -Wpedantic \
	-Werror \
    -g
CFLAGS += -fsanitize=address -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address

TARGET := trilog
BUILD_DIR := _build
EXAMPLES_DIR := examples

SRCS := $(wildcard src/*.c)
HDRS := $(wildcard src/*.h)
OBJS := $(SRCS:src/%.c=$(BUILD_DIR)/%.o)

LIB_OBJS := $(filter-out $(BUILD_DIR)/main.o,$(OBJS))

EXAMPLE_SRCS := $(wildcard $(EXAMPLES_DIR)/*.c)
EXAMPLE_BINS := $(EXAMPLE_SRCS:$(EXAMPLES_DIR)/%.c=$(BUILD_DIR)/%)

all: format $(TARGET) $(EXAMPLE_BINS)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(BUILD_DIR)/%.o: src/%.c $(HDRS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%: $(EXAMPLES_DIR)/%.c $(LIB_OBJS) $(HDRS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(WEB_DIR)/trilog.js $(WEB_DIR)/trilog.wasm wokwi/build

.PHONY: examples
examples: $(EXAMPLE_BINS)

.PHONY: format
format:
	clang-format -i $(SRCS) $(HDRS)

.PHONY: format-check
format-check:
	clang-format --dry-run --Werror $(SRCS) $(HDRS)

.PHONY: run
run: $(TARGET)
	./$(TARGET)

.PHONY: debug
debug: $(TARGET)
	./$(TARGET) -d

QUAD_TIMEOUT := 60

.PHONY: quad
quad: $(TARGET)
	@for f in test/*_quad.pl; do \
		[ -f "$$f" ] || continue; \
		timeout $(QUAD_TIMEOUT) ./$(TARGET) -e "consult('lib/quad.pl'), quad_cli('$$f')" || true; \
	done

.PHONY: quad-junit
quad-junit: $(TARGET)
	@mkdir -p _build/test-results
	@for f in test/*_quad.pl; do \
		[ -f "$$f" ] || continue; \
		timeout $(QUAD_TIMEOUT) ./$(TARGET) -e "consult('lib/quad.pl'), quad_cli_junit('$$f', '_build/test-results')" || true; \
	done
	@echo "JUnit reports written to _build/test-results/"

.PHONY: syscheck
syscheck: $(TARGET)
	bats test/*.bats

.PHONY: syscheck-junit
syscheck-junit: $(TARGET)
	@mkdir -p _build/test-results
	bats --report-formatter junit --output _build/test-results test/*.bats

.PHONY: test
test: quad syscheck

WEB_DIR := web
WEB_LIB_SRCS := $(filter-out src/main.c, $(SRCS))
WEB_ENTRY := $(WEB_DIR)/main_web.c

# arm cortex-m0+ constraints (rp2040, 264kb sram)
SMALL_FLAGS := \
    -DMAX_NAME=48 \
    -DMAX_LIST_LIT=128 \
    -DMAX_CLAUSES=256 \
    -DMAX_BINDINGS=1024 \
    -DMAX_VARS=2048 \
    -DMAX_GOALS=64 \
    -DMAX_STACK=128 \
    -DMAX_ERROR_MSG=128 \
    -DMAX_CUSTOM_BUILTINS=8 \
    -DMAX_STRING_POOL=8192 \
    -DMAX_FILE_PATH=128 \
    -DMAX_MAKE_FILES=4 \
    -DMAX_OPEN_STREAMS=4 \
    -DMAX_CLAUSE_VARS=32 \
    -DMAX_OPS=48 \
    -DTERM_POOL_BYTES=49152

SMALL_SRCS := src/arith.c src/builtins.c src/cli.c src/debug.c src/env.c \
              src/errors.c src/ffi.c src/io.c src/main.c src/parse.c \
              src/print.c src/solve.c src/streams.c src/term.c \
              src/unify.c

.PHONY: small
small: format
	$(CC) -std=c11 -Os -ffunction-sections -fdata-sections -Wl,--gc-sections \
	    $(SMALL_FLAGS) $(SMALL_SRCS) -o $(BUILD_DIR)/trilog-small
	@strip $(BUILD_DIR)/trilog-small
	@size $(BUILD_DIR)/trilog-small


.PHONY: pico
pico:
	mkdir -p wokwi/build
	cd wokwi/build && cmake .. -Wno-dev > /dev/null
	$(MAKE) -C wokwi/build -j$$(nproc)
	@echo "sim:   wokwi/build/trilog/trilog.elf"
	@echo "flash: wokwi/build/trilog.uf2"

