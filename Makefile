CC := gcc
CFLAGS := \
    -std=c11 \
    -Wall \
    -Wextra \
    -Wpedantic \
    -g
CFLAGS += -fsanitize=address -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address

TARGET := prolog
BUILD_DIR := _build
EXAMPLES_DIR := examples

SRCS := $(wildcard src/*.c)
HDRS := $(wildcard src/*.h)
OBJS := $(SRCS:src/%.c=$(BUILD_DIR)/%.o)

# Filter out main.o for examples that have their own main
LIB_OBJS := $(filter-out $(BUILD_DIR)/main.o,$(OBJS))

# Examples
EXAMPLE_SRCS := $(wildcard $(EXAMPLES_DIR)/*.c)
EXAMPLE_BINS := $(EXAMPLE_SRCS:$(EXAMPLES_DIR)/%.c=$(BUILD_DIR)/%)

all: $(TARGET) $(EXAMPLE_BINS)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(BUILD_DIR)/%.o: src/%.c $(HDRS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build examples
$(BUILD_DIR)/%: $(EXAMPLES_DIR)/%.c $(LIB_OBJS) $(HDRS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< $(LIB_OBJS) $(LDFLAGS) -o $@

$(BUILD_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

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

.PHONY: test
test: $(TARGET)
	bats test/*.sh