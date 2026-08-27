# Make defines CC=cc by default, which does not exist on MSYS2/mingw.
# Only take that default over; a CC set in the environment or on the
# command line still wins.
ifeq ($(origin CC),default)
CC := gcc
endif
CFLAGS  := -DUNIT_TEST -Iunity -Isrc -Wall -Wextra -g
SRCS    := unity/unity.c $(wildcard src/*.c) $(wildcard test/*.c)
TARGET  := test_runner

.PHONY: test clean

test: $(TARGET)
	./$(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@

clean:
	rm -f $(TARGET) $(TARGET).exe
