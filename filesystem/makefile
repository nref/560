# RPC lab makefile
# Doug Slater
# mailto:cds@utk.edu
# CS560 February 2014

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
	
# Each .o file depends on a .c and .h of the same name. 
#$(CLIENTDEPS) $(SERVDEPS) $(ODIR)/%.o: $(SDIR)/%.c $(IDIR)/%.h 

# Build all objects from source
$(DEPS): $(patsubst %.o,$(SDIR)/%.c,$@) $(GENH)
	$(CC) $(CFLAGS) -c -o $@ $(patsubst %.o,$(SDIR)/%.c,$@)	 

# Generate source with rpcgen
$(GENH) $(GENC): transaction.x
	rpcgen transaction.x
	mv $(GENH) $(IDIR)
	mv $(GENC) $(SDIR)
	
clean: 
	rm -f $(GENC:%=$(SDIR)/%)
	rm -f $(IDIR)/$(GENH)
	rm -f $(ODIR)/*
	rm -f ./*.o
	rm -f $(BIN:%=$(BDIR)/%)
