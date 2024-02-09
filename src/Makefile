IDIR =../include
IEXTRADIR =/data/john/libs/include
CC=gcc
CFLAGS=-I$(IDIR) -I$(IEXTRADIR)

ODIR=obj
LDIR =-L/data/john/libs/lib

LIBS=-lm -lzbd

_DEPS = zns.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = zns.o zns.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

zns: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS) $(LDIR)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~
