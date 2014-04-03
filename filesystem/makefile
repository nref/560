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

DEPS = $(ODIR)/fs.o

define cc-command
$(CC) $(CFLAGS) -o $(BDIR)/$@ $^
endef

# Link objects into binaries
sh: $(ODIR)/sh.o $(ODIR)/fs.o
	$(cc-command)

# Build objects from source
$(ODIR)/sh.o: $(SDIR)/sh.c
	$(CC) $(CFLAGS) -c -hexo $@ $^

$(ODIR)/fs.o: $(SDIR)/fs.c
	$(CC) $(CFLAGS) -c -o $@ $^

clean: 
	rm -f $(ODIR)/*
	rm -f $(BIN:%=$(BDIR)/%)
