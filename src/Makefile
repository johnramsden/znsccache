IDIR =../include
IEXTRADIR =/data/john/libs/include
CC=gcc
CFLAGS=-I$(IDIR) -I$(IEXTRADIR) -g

# define DEBUG in the source if we are debugging
ifeq ($(DEBUG), 1)
	CFLAGS+=-DDEBUG
endif

ODIR=obj
LDIR =-L/data/john/libs/lib

LIBS=-lm -lzbd -lgpg-error -luuid `libgcrypt-config --cflags --libs`

_DEPS = znsccache.h util.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = znsccache.o zncc.o bucket.o util.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

znsccache: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS) $(LDIR)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~
