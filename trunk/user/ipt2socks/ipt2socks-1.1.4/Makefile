CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -pthread
LIBS = -lm
SRCS = src/ipt2socks.c src/lrucache.c src/netutils.c src/protocol.c
OBJS = $(SRCS:.c=.o)
MAIN = ipt2socks
DESTDIR = /usr/local/bin

EVCFLAGS = -O2 -fno-strict-aliasing
EVSRCFILE = libev/ev.c
EVOBJFILE = ev.o

.PHONY: all install clean

all: $(MAIN)
	@@ -19,13 +16,10 @@ install: $(MAIN)
	install -m 0755 $(MAIN) $(DESTDIR)

clean:
	$(RM) $(MAIN) src/*.o libev/*.o

$(MAIN): $(EVOBJFILE) $(OBJS)
	$(CC) $(CFLAGS) -s -o $(MAIN) $(OBJS) $(EVOBJFILE) $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

$(EVOBJFILE): $(EVSRCFILE)
	$(CC) $(EVCFLAGS) -c $(EVSRCFILE) -o $(EVOBJFILE)