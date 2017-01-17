CC := gcc
CFLAGS := -Iinclude -std=c11 -ggdb -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE $(shell pkg-config --cflags libedit)
LFLAGS := $(shell pkg-config --libs libedit)

SRC := src/
OBJ := obj/

TARGET := pnfs

SOURCES := $(shell find $(SRC) -iname "*.c")
HEADERS := $(shell find $(SRC) -iname "*.h")
OBJECTS := $(patsubst $(SRC)%.c, $(OBJ)%.o, $(SOURCES))

.PHONY: all clean editCheck

all: clean editCheck $(TARGET)

run: all
	@./$(TARGET)

editCheck:
	@pkg-config libedit || ( echo -e "\033[93;41m############################################\nYou need to have libedit installed, search for libedit-dev (can also just be called libedit)\nand then try to compile again!\n############################################\033[0m"; exit 1)

$(OBJ)%.o: $(SRC)%.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJECTS) -o $(TARGET) $(LFLAGS)

clean:
	$(RM) -rf $(BIN) $(OBJ)

format:
	astyle --style=java --indent=tab $(SOURCES) $(HEADERS)
