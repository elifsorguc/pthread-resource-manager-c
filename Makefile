# Compiler and flags
CC = gcc
CFLAGS = -Wall -pthread

# Target executable and library names
LIBRARY = libreman.a
TEST_APP = myapp

# Source files
LIB_SRC = reman.c
LIB_OBJ = reman.o
TEST_SRC = myapp.c

# Build all targets
all: $(LIBRARY) $(TEST_APP)

# Create the static library libreman.a
$(LIBRARY): $(LIB_OBJ)
	@ar -cvq $@ $^
	@ranlib $@

# Compile the library source to object file
$(LIB_OBJ): $(LIB_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile the test application and link with the library
$(TEST_APP): $(TEST_SRC) $(LIBRARY)
	$(CC) $(CFLAGS) -o $@ $^ -L. -lreman

# Clean up generated files
clean:
	rm -f $(LIB_OBJ) $(LIBRARY) $(TEST_APP) *.o *~

# Run the test application
run: $(TEST_APP)
	./$(TEST_APP) 1
