CC = gcc
CFLAGS = -Wall -g -Iinclude
LDFLAGS =

all: folders controller runner

controller: bin/controller

runner: bin/runner

folders:
	@mkdir -p src include obj bin tmp

bin/controller: obj/controller.o
	$(CC) $(LDFLAGS) $^ -o $@

bin/runner: obj/runner.o
	$(CC) $(LDFLAGS) $^ -o $@

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f obj/* tmp/* bin/*