GCC=gcc
CCFLAGS=-Wall -pedantic -std=c99 -I/usr/include/SDL
LDFLAGS=-lSDL
PROG=unifont
MODS=unifont.o

all: $(PROG)

%.o: %.c
	$(GCC) -c $(CCFLAGS) -o $@ $<

$(PROG): $(MODS)
	$(GCC) -o $@ $< $(LDFLAGS)

clean:
	-rm -f $(PROG) $(MODS)

