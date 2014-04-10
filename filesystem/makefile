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
CFLAGS = $(INCLUDES) -g -std=c99 -Wall
CC = cc

# Output binaries
BIN = sh

# Build targets: all with a name from BIN in ./bin/
all: $(BIN)

DEPS = $(ODIR)/fs.o $(ODIR)/sh.o

define cc-command
$(CC) $(CFLAGS) -o $(BDIR)/$@ $^
endef

# Link objects into binaries
sh: $(ODIR)/sh.o $(ODIR)/fs.o
	$(cc-command)

# Build objects from source
$(ODIR)/sh.o: $(SDIR)/sh.c $(IDIR)/sh.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(ODIR)/fs.o: $(SDIR)/fs.c $(IDIR)/fs.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean: 
	rm -f fs
	rm -f vs110/Debug/fs
	rm -f $(ODIR)/*
	rm -f $(BIN:%=$(BDIR)/%)
