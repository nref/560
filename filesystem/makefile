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
CFLAGS = $(INCLUDES) -std=c99 -Wall -Wextra -pedantic -Wformat=2
CC = cc

# Output binaries
BIN = sh

all: $(BIN)

release: CFLAGS += -O3
release: sh
debug: CFLAGS += -DDEBUG -g3
debug: sh
analyze: CFLAGS += --analyze
analyze: sh

DEPS = $(ODIR)/sh.o $(ODIR)/fs.o $(ODIR)/_fs.o

define cc-command
$(CC) $(CFLAGS) -o $(BDIR)/$@ $^
endef

# Link objects into binaries
sh: $(DEPS)
	$(cc-command)

# Build objects from source
$(ODIR)/sh.o: $(SDIR)/sh.c $(IDIR)/sh.h 
	$(CC) $(CFLAGS) -c -o $@ $<

$(ODIR)/_fs.o: $(SDIR)/_fs.c $(IDIR)/_fs.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(ODIR)/fs.o: $(SDIR)/fs.c $(IDIR)/fs.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean: 
	rm -f fs
	rm -f vs110/Debug/fs
	rm -f $(ODIR)/*

	# Remove files with a name from BIN in ./bin/
	rm -f $(BIN:%=$(BDIR)/%)
