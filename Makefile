CC = gcc
CFLAGS = -Wall -g -Iinclude
LDFLAGS =

GLIB_CFLAGS := $(shell pkg-config --cflags glib-2.0)
GLIB_LIBS := $(shell pkg-config --libs glib-2.0)
LIBS := $(GLIB_LIBS) -lncursesw

OBJ_CONTROLLER_MAIN = obj/controller.o
OBJ_RUNNER_MAIN = obj/runner/runner.o

COMMON_OBJS = obj/protocol.o obj/runner/communication.o

CONTROLLER_SPECIFIC_OBJS = obj/scheduler/dispatcher.o

RUNNER_SPECIFIC_OBJS = obj/runner/executor.o


all: folders controller runner

controller: bin/controller

runner: bin/runner

folders:
	@mkdir -p src include obj/runner obj/scheduler bin tmp

bin/controller: $(OBJ_CONTROLLER_MAIN) $(COMMON_OBJS) $(CONTROLLER_SPECIFIC_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

bin/runner: $(OBJ_RUNNER_MAIN) $(COMMON_OBJS) $(RUNNER_SPECIFIC_OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(GLIB_CFLAGS) -c $< -o $@

format:
	@clang-format --verbose -i $(shell find src/ -name "*.c")
	@printf "\nFormatted all available files.\n"

clean:
	rm -rf obj/* tmp/* bin/*