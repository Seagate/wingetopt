
# This is a handwritten GNUMakefile for wingetopt
# This was done so as not to collide with the makefile output from CMake when CMake is used.
# This makefile is for projects unable to use cmake or meson, but support GNUMake
# Can convert this to a POSIX makefile if needed. -TJE

NAME=wingetopt
CC ?= cc 
SRC_DIR=./src
INC_DIR=-I./src/
CC ?= gcc
AR ?= ar
CFLAGS ?= -Wall -Wextra -c -fPIC -I.
SRC_FILES = $(SRC_DIR)/getopt.c
LIB_OBJ_FILES = $(SRC_FILES:.c=.o)
STATIC_LIB = lib$(NAME).a
SHARED_LIB = lin$(NAME).so

FILE_OUTPUT_DIR=lib

.PHONY: all 

all: clean mkoutputdir static

%.o: %.c
	$(CC) $(CFLAGS) $(INC_DIR) $< -o $@

static: $(LIB_OBJ_FILES)
	rm -f $(FILE_OUTPUT_DIR)/$(STATIC_LIB) $(FILE_OUTPUT_DIR)/$(SHARED_LIB)
	$(AR) cq $(FILE_OUTPUT_DIR)/$(STATIC_LIB) $(LIB_OBJ_FILES)

shared: $(LIB_OBJ_FILES)
	rm -f $(FILE_OUTPUT_DIR)/$(STATIC_LIB) $(FILE_OUTPUT_DIR)/$(SHARED_LIB)
	$(CC) -shared $(LIB_OBJ_FILES) -o $(FILE_OUTPUT_DIR)/$(SHARED_LIB)

clean:
	rm -f $(FILE_OUTPUT_DIR)/$(STATIC_LIB) $(FILE_OUTPUT_DIR)/$(SHARED_LIB) *.o $(SRC_DIR)*.o
	rm -rf $(FILE_OUTPUT_DIR)

mkoutputdir:
	mkdir -p $(FILE_OUTPUT_DIR)

