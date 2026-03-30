CC ?= gcc

BUILD_TYPE ?= debug

# Core warnings and includes
BASE_CFLAGS := -Wall -Wextra -Werror=pedantic -Iinclude

# Build profiles
CFLAGS_debug := -g -O0
CFLAGS_release := -O2 -DNDEBUG
CFLAGS_asan := -g -O1 -fsanitize=address -fno-omit-frame-pointer
LDFLAGS_asan := -fsanitize=address

GLIB_CFLAGS := $(shell pkg-config --cflags glib-2.0)
GLIB_LIBS := $(shell pkg-config --libs glib-2.0)

CFLAGS := $(BASE_CFLAGS) $(GLIB_CFLAGS) $(CFLAGS_$(BUILD_TYPE))
LDFLAGS += $(LDFLAGS_$(BUILD_TYPE))
LDLIBS := $(GLIB_LIBS) -lncursesw

SRC := $(shell find src -name "*.c")
COMMON_SRC := $(filter-out src/controller.c src/runner/runner.c,$(SRC))

BUILD_DIR := build/$(BUILD_TYPE)
COMMON_OBJ := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(COMMON_SRC))
CONTROLLER_OBJ := $(BUILD_DIR)/controller.o
RUNNER_OBJ := $(BUILD_DIR)/runner/runner.o

CONTROLLER_BIN := bin/controller
RUNNER_BIN := bin/runner

RED := \033[1;31m
WHITE := \033[0m
BLUE := \033[1;34m
GREEN := \033[1;32m
PINK := \033[1;35m
YELLOW := \033[1;33m

.PHONY:

all: folders controller runner
	@printf "\n$(GREEN)[BUILD]$(WHITE) Build Status: $(GREEN)$(BUILD_TYPE)$(WHITE).\n"
	@printf "$(GREEN)[BUILD]$(WHITE) Binaries are ready in $(GREEN)bin/$(WHITE).\n"
	
folders:
	@mkdir -p bin tmp $(BUILD_DIR)

controller: $(CONTROLLER_BIN)

runner: $(RUNNER_BIN)

$(CONTROLLER_BIN): $(CONTROLLER_OBJ) $(COMMON_OBJ) | folders
	@printf "$(BLUE)[LINK]$(WHITE) Creating $(BLUE)$@$(WHITE).\n"
	@$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(RUNNER_BIN): $(RUNNER_OBJ) $(COMMON_OBJ) | folders
	@printf "$(BLUE)[LINK]$(WHITE) Creating $(BLUE)$@$(WHITE).\n"
	@$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	@printf "$(PINK)[CC]$(WHITE) Compiling $(PINK)$<$(WHITE).\n"
	@$(CC) $(CFLAGS) -c $< -o $@

debug:
	@printf "\n$(YELLOW)[BUILD]$(WHITE) Starting debug build.\n"
	@$(MAKE) BUILD_TYPE=debug all

release:
	@printf "\n$(YELLOW)[BUILD]$(WHITE) Starting release build.\n"
	@$(MAKE) BUILD_TYPE=release all

asan:
	@printf "\n$(RED)[BUILD]$(WHITE) Starting ASan build.\n"
	@$(MAKE) BUILD_TYPE=asan all

format:
	@printf "\n$(YELLOW)[FORMAT]$(WHITE) Formatting files with clang-format.\n"
	@clang-format --verbose -i $(SRC) $(wildcard include/*.h include/*/*.h include/*/*/*.h)
	@printf "$(GREEN)[FORMAT]$(WHITE) Formatting finished.\n"

clean:
	@rm -rf build
	@rm -rf obj
	@rm -f $(CONTROLLER_BIN) $(RUNNER_BIN)
	@rm -rf tmp/*
	@printf "\n$(YELLOW)[CLEAN]$(WHITE) Build artifacts removed.\n"

rebuild: clean all
