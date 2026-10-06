CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -Wpedantic -O2 -g -Iinclude -D_POSIX_C_SOURCE=200809L
LDFLAGS  =

SRC_DIR   = src
BUILD_DIR = build
TARGET    = $(BUILD_DIR)/fire

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

all: $(TARGET)

# Линковка: собрать всё в один бинарник
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Компиляция одного .c → .o, с автогенерацией списка зависимостей
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

# Гарантировать, что папка build существует до компиляции
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

run: $(TARGET)
	./$(TARGET)

# Подхватить сгенерированные .d файлы с зависимостями (если есть)
-include $(OBJS:.o=.d)

.PHONY: all clean run