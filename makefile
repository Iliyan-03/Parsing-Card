CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -fPIC -g -Iinclude
LDFLAGS = -shared
OBJ_DIR = src
BIN_DIR = bin

LIB_TARGET = libvcparser.so
OBJ_FILES = $(OBJ_DIR)/VCParser.o $(OBJ_DIR)/LinkedListAPI.o $(OBJ_DIR)/VCHelper.o

all: parser

$(BIN_DIR)/$(LIB_TARGET): $(OBJ_FILES)
	@mkdir -p $(BIN_DIR)
	$(CC) $(LDFLAGS) $(OBJ_FILES) -o $(BIN_DIR)/$(LIB_TARGET)

parser: $(BIN_DIR)/$(LIB_TARGET)

$(OBJ_DIR)/VCParser.o: $(OBJ_DIR)/VCParser.c
	$(CC) $(CFLAGS) -c $(OBJ_DIR)/VCParser.c -o $(OBJ_DIR)/VCParser.o

$(OBJ_DIR)/LinkedListAPI.o: $(OBJ_DIR)/LinkedListAPI.c
	$(CC) $(CFLAGS) -c $(OBJ_DIR)/LinkedListAPI.c -o $(OBJ_DIR)/LinkedListAPI.o

$(OBJ_DIR)/VCHelper.o: $(OBJ_DIR)/VCHelper.c
	$(CC) $(CFLAGS) -c $(OBJ_DIR)/VCHelper.c -o $(OBJ_DIR)/VCHelper.o

.PHONY: clean
clean:
	rm -f $(OBJ_DIR)/*.o $(BIN_DIR)/*.so  *.so