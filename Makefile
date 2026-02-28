# Compiler
CC = gcc
CCFLAGS = -g -Wall

# Target executable
TARGET = my-sum
SRCS = my-sum.cpp

# Default values (can be overridden from command line)
n ?= 5
m ?= 2
input ?= input.dat
output ?= output.dat

# Default build
all: $(TARGET)

# Compile the target
$(TARGET): $(SRCS)
	$(CC) $(CCFLAGS) -o $(TARGET) $(SRCS) -lm

# Run with configurable arguments
run: $(TARGET)
	./$(TARGET) $(n) $(m) $(input) $(output)

# Clean
clean:
	rm -f $(TARGET)