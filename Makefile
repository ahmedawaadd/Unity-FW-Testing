# Make defines CC=cc by default, which does not exist on MSYS2/mingw.
# Only take that default over; a CC set in the environment or on the
# command line still wins.
ifeq ($(origin CC),default)
CC := gcc
endif
CFLAGS  := -DUNIT_TEST -Iunity -Isrc -Itest -Wall -Wextra -g
SRCS    := unity/unity.c $(wildcard src/*.c) $(wildcard test/*.c)
TARGET  := test_runner

.PHONY: test coverage clean

test: $(TARGET)
	./$(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

# Line coverage over the firmware in src/. Needs gcc/gcov (already present
# on GitHub runners and in MSYS2 mingw64). Compiles the firmware to object
# files so the coverage notes sit next to each source and gcov can find
# them; reports only the src/ files, which is the code we actually test.
COV_OBJS := $(patsubst src/%.c,cov/%.o,$(wildcard src/*.c))

coverage: clean cov $(COV_OBJS)
	$(CC) $(CFLAGS) --coverage unity/unity.c $(wildcard test/*.c) $(COV_OBJS) -o $(TARGET)
	./$(TARGET)
	@echo
	gcov -n -o cov $(wildcard src/*.c)

cov:
	@mkdir -p cov

cov/%.o: src/%.c
	$(CC) $(CFLAGS) --coverage -c $< -o $@

clean:
	rm -f $(TARGET) $(TARGET).exe *.gcno *.gcda *.gcov *.o
	rm -rf cov
