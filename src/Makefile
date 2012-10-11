.PHONY:	all app lib test clean

# Settings

MAKEFLAGS += --no-builtin-rules --no-builtin-variables

CC = g++
CC_OPTS_EX = -O0 -g -Wall -Wextra -Werror
CC_OPTS_SO = -fpic -O2 -DNDEBUG -Wall -Wextra -Werror
LD = g++
LD_OPTS_EX = -O0 -g -lboost_unit_test_framework -lpthread -ldl
LD_OPTS_SO = -fpic -shared -lpthread -ldl

BIN = ../bin

# Targets

all: app lib

app: $(BIN)/test-application

lib: $(BIN)/alloc-randomizer.so

test: $(BIN)/test-application
	MALLOC_CHECK_=3 $(BIN)/test-application

clean:
	rm -f *.o
	rm -f *.dep

# Files

MM_SO = alloc-randomizer
MM_EX = test-application

OO_SO = $(addsuffix .o, $(MM_SO))
OO_EX = $(addsuffix .o, $(MM_EX))

DD_SO = $(addsuffix .dep, $(MM_SO))
DD_EX = $(addsuffix .dep, $(MM_EX))
DD = $(DD_SO) $(DD_EX)

# Modules

$(OO_SO): %.o: %.c
	$(CC) $(CC_OPTS_SO) -c -o $@ $<

$(OO_EX): %.o: %.c
	$(CC) $(CC_OPTS_EX) -c -o $@ $<

# Executables

$(BIN)/alloc-randomizer.so: $(OO_SO)
	$(LD) $(LD_OPTS_SO) -o $@ $^

$(BIN)/test-application: $(OO_EX)
	$(LD) $(LD_OPTS_EX) -o $@ $^

# Dependencies

include $(DD)

%.dep: %.c
	$(CC) -MM -MT "$@ $(basename $@).o" -o $@ $<
