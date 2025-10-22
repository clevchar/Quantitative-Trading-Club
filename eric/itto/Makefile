# Makefile for building the deciphering program
# Usage: make         # build optimized binary
#        make debug   # build debug binary with -g -O0 and -DDEBUG
#        make run     # build then run the program
#        make valgrind # build then run under valgrind (if installed)
#        make clean   # remove objects and binary

CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -O2
LDFLAGS ?=

SRC := $(wildcard *.c)
OBJ := $(SRC:.c=.o)
TARGET := deciphering itto_parser

.PHONY: all debug clean run valgrind help

all: CFLAGS += -O2
all: $(TARGET)

debug: CFLAGS += -g -O0 -DDEBUG
debug: $(TARGET)

deciphering: deciphering.o
	$(CC) $(LDFLAGS) -o $@ $^

itto_parser: itto_parser.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

valgrind: all
	valgrind --leak-check=full ./$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)

help:
	@echo "Makefile targets:"
	@echo "  all (default)  - build optimized binary ($(TARGET))"
	@echo "  debug          - build debug binary with -g and -DDEBUG"
	@echo "  run            - build then run the program"
	@echo "  valgrind       - run under valgrind (if installed)"
	@echo "  clean          - remove build artifacts"
