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

SRCS := $(wildcard src/*.c)
HDRS := $(wildcard src/*.h)
OBJS := $(SRCS:src/%.c=$(BUILD_DIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(BUILD_DIR)/%.o: src/%.c $(HDRS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

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
	bats test/*