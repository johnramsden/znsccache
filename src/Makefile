IDIR =../include
IEXTRADIR =/data/john/libs/include
CC=gcc
CFLAGS=-I$(IDIR) -I$(IEXTRADIR) -g # -fsanitize=address -static-libasan

# define DEBUG in the source if we are debugging
ifeq ($(DEBUG), 1)
	CFLAGS+=-DDEBUG
endif
ifeq ($(EMULATE_S3), 1)
	CFLAGS+=-DEMULATE_S3
endif

S3_DELAY ?= 1
CFLAGS+=-DS3_DELAY=$(S3_DELAY)

ODIR=obj
LDIR =-L/data/john/libs/lib

LIBS=-lm -lzbd -lgpg-error -luuid -ls3 -lcjson `libgcrypt-config --cflags --libs`

_DEPS = znsccache.h util.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = znsccache.o zncc.o bucket.o util.o s3.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

znsccache: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS) $(LDIR)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~
