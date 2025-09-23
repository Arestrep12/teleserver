# Makefile para TeleServer

CC := clang
CFLAGS_BASE := -std=c11 -Wall -Wextra -Werror -Iinclude
CFLAGS_DEBUG := $(CFLAGS_BASE) -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer -DDEBUG
CFLAGS_RELEASE := $(CFLAGS_BASE) -O2 -DNDEBUG

# Detectar sistema operativo
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    CFLAGS_BASE += -D_GNU_SOURCE
endif

# Directorios
SRC_DIR := src
TEST_DIR := tests
BIN_DIR := bin
BUILD_DIR := build
BUILD_TEST_DIR := $(BUILD_DIR)/tests

# Archivos fuente
SRCS_ALL := $(shell find $(SRC_DIR) -name '*.c')
SRCS_MAIN := $(SRC_DIR)/server/main.c
SRCS_LIB := $(filter-out $(SRCS_MAIN),$(SRCS_ALL))
OBJS_DEBUG := $(SRCS_ALL:$(SRC_DIR)/%.c=$(BUILD_DIR)/debug/%.o)
OBJS_RELEASE := $(SRCS_ALL:$(SRC_DIR)/%.c=$(BUILD_DIR)/release/%.o)
TEST_SRCS := $(wildcard $(TEST_DIR)/test_*.c)
TEST_BINS := $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_TEST_DIR)/%)

# Targets principales
.PHONY: all debug release test clean lint format help

all: debug

help:
	@echo "Targets disponibles:"
	@echo "  debug    - Compilar con símbolos de debug y sanitizers"
	@echo "  release  - Compilar optimizado para producción"
	@echo "  test     - Ejecutar todas las pruebas"
	@echo "  lint     - Ejecutar clang-tidy y cppcheck"
	@echo "  format   - Formatear código con clang-format"
	@echo "  clean    - Limpiar archivos generados"

debug: $(BIN_DIR)/tele_server_debug

release: $(BIN_DIR)/tele_server

# Compilación debug
$(BIN_DIR)/tele_server_debug: $(OBJS_DEBUG)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_DEBUG) -o $@ $^
	@echo "✓ Compilación debug completa: $@"

$(BUILD_DIR)/debug/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_DEBUG) -c -o $@ $<

# Compilación release
$(BIN_DIR)/tele_server: $(OBJS_RELEASE)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS_RELEASE) -o $@ $^
	@echo "✓ Compilación release completa: $@"

$(BUILD_DIR)/release/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS_RELEASE) -c -o $@ $<

# Tests
test: $(TEST_BINS)
	@echo "Ejecutando pruebas..."
	@for test in $(TEST_BINS); do \
		echo "→ Ejecutando $$test"; \
		$$test || exit 1; \
	done
	@echo "✓ Todas las pruebas pasaron"

$(BUILD_TEST_DIR)/test_%: $(TEST_DIR)/test_%.c $(SRCS_LIB)
	@mkdir -p $(BUILD_TEST_DIR)
	$(CC) $(CFLAGS_DEBUG) -o $@ $< $(SRCS_LIB)

# Linting
lint:
	@echo "Ejecutando clang-tidy..."
	@clang-tidy $(SRCS_ALL) -- $(CFLAGS_BASE)
	@echo "Ejecutando cppcheck..."
	@cppcheck --enable=all --inconclusive --std=c11 \
		--suppress=missingIncludeSystem $(SRC_DIR) include
	@echo "✓ Linting completo"

# Formato
format:
	@echo "Formateando código..."
	@clang-format -i $(SRCS_ALL) $(shell find include -name '*.h')
	@echo "✓ Formato aplicado"

# Limpieza
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "✓ Limpieza completa"

# Crear directorios necesarios
$(shell mkdir -p $(BUILD_DIR)/debug $(BUILD_DIR)/release $(BUILD_TEST_DIR) $(BIN_DIR))
