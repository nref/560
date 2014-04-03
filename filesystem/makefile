# Filesystem lab makefile
# Doug Slater and Christopher Craig
# mailto:cds@utk.edu, mailto:ccraig7@utk.edu
# CS560 April 2014, Dr. Cao
# University of Tennesse, Knoxville

IDIR = ./inc
SDIR = ./src
BDIR = ./bin
ODIR = $(BDIR)/obj

INCLUDES = -I$(IDIR)
CFLAGS = $(INCLUDES) -g -std=c99
CC = cc

# Output binaries
BIN = sh

# Build targets: all with a name from BIN in ./bin/
all: $(BIN)

DEPS = fs.o

define cc-command
$(CC) $(CFLAGS) -o $(BDIR)/$@ $^
endef

# Link objects into binaries
sh: fs.o
	$(cc-command)

# Build all objects from source
$(DEPS): $(patsubst %.o,$(SDIR)/%.c,$@)
	$(CC) $(CFLAGS) -c -o $@ $(patsubst %.o,$(SDIR)/%.c,$@)	 
	
clean: 
	rm -f $(ODIR)/*
	rm -f ./*.o
	rm -f $(BIN:%=$(BDIR)/%)
