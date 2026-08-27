CC      ?= gcc
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
