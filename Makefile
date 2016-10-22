CC := gcc
CFLAGS := -Iinclude -Wall -Werror -std=gnu11 -ggdb
LFLAGS := -ledit

SRC := src/
OBJ := obj/

TARGET := pnfs

SOURCES := $(shell find $(SRC) -iname "*.c")
HEADERS := $(shell find $(SRC) -iname "*.h")
OBJECTS := $(patsubst $(SRC)%.c, $(OBJ)%.o, $(SOURCES))

include ./Utils.mk

.PHONY: all clean init done

all: clean init $(TARGET) done

run: all
	@./$(TARGET)

init:
	@$(call INFO, "--", "Building PNFS...");

done:
	@$(call INFO, "--", "Building PNFS...Done!");

$(OBJ)%.o: $(SRC)%.c
	@$(call BEG, "CC", "$<")
	@mkdir -p $(dir $@)
	@$(CC) -c $< -o $@ $(CFLAGS) $(ERRORS)
	@$(call END, "CC", "$<")

$(TARGET): $(OBJECTS)
	@$(call BEG, "LD", "$@")
	@mkdir -p $(dir $@)
	@$(CC) $(OBJECTS) -o $(TARGET) $(LFLAGS) $(ERRORS)
	@$(call END, "LD", "$@")

clean:
	@$(call BEGRM, "RM", "$(BIN) $(OBJ)")
	@$(RM) -rf $(BIN) $(OBJ)
	@$(call ENDRM, "RM", "$(BIN) $(OBJ)")

format:
	astyle --style=java -n --indent=tab $(SOURCES) $(HEADERS)
