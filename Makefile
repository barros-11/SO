CC = gcc

CFLAGS = -Wall -g -Iinclude
LDFLAGS =

GLIB_CFLAGS := $(shell pkg-config --cflags glib-2.0)
GLIB_LIBS := $(shell pkg-config --libs glib-2.0)

LIBS := $(GLIB_LIBS) -lncursesw

SRC := $(shell find src/ -name "*.c")

COMMON_SRC := $(filter-out src/controller.c src/runner.c, $(SRC))

COMMON_OBJ := $(patsubst src/%.c, obj/%.o, $(COMMON_SRC))

all: folders controller runner

controller: bin/controller

runner: bin/runner

folders:
	@mkdir -p src include obj bin tmp

bin/controller: obj/controller.o $(COMMON_OBJ)
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

bin/runner: obj/runner.o $(COMMON_OBJ)
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(GLIB_CFLAGS) -c $< -o $@

format:
	@clang-format --verbose -i $(SRC)
	@printf "\nFormatted all available files.\n"

clean:
	rm -f obj/* tmp/* bin/*