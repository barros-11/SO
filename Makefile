CC = gcc

CFLAGS = -Wall -g -Iinclude
LDFLAGS =

GLIB_CFLAGS := $(shell pkg-config --cflags glib-2.0)
GLIB_LIBS := $(shell pkg-config --libs glib-2.0)

LIBS := $(GLIB_LIBS) -lncursesw

SRC := $(shell find src/ -name "*.c")


all: folders controller runner

//Todo: remove these (testing)
test-rr: bin/round_robin_test

bin/round_robin_test: src/scheduler/round_robin_test.c src/scheduler/round_robin.c
	$(CC) $(CFLAGS) $(GLIB_CFLAGS) $^ -o $@ $(GLIB_LIBS)

controller: bin/controller

runner: bin/runner

folders:
	@mkdir -p src include obj bin tmp

bin/controller: obj/controller.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

bin/runner: obj/runner.o
	$(CC) $(LDFLAGS) $^ -o $@ $(LIBS)

obj/%.o: src/%.c
	$(CC) $(CFLAGS) $(GLIB_CFLAGS) -c $< -o $@

format:
	@clang-format --verbose -i $(SRC)
	@printf "\nFormatted all available files.\n"

clean:
	rm -f obj/* tmp/* bin/*