# Makefile for the rest-in-c-major project with dynamic linking and dependency installation

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -I/usr/include/postgresql -I/usr/include/jansson

# Library paths and libraries
LIBS =-lulfius -ljansson -lpq

# Source files
SRC = rest-in-c-major.c

# Output binary directory and file
BIN_DIR = bin
TARGET = $(BIN_DIR)/rest-in-c-major

# Default target
all: $(TARGET)

# Create output directory
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Link the executable
$(TARGET): $(BIN_DIR) $(SRC)
	$(CC) -o $@ $(SRC) $(CFLAGS) $(LIBS)

# Install dependencies
install-deps:
	sudo apt-get update
	sudo apt-get install -y build-essential libmicrohttpd-dev libjansson-dev libpq-dev libulfius-dev

# Clean target
clean:
	rm -f $(TARGET)

# Phony targets
.PHONY: all clean install-deps

