# Makefile for rsync. This is processed by configure to produce the final
# Makefile

INSTALL_BIN=/usr/local/bin
INSTALL_MAN=/usr/local/man

LIBS=
CC=gcc 
CFLAGS=-g -O2
 
INSTALLCMD=/usr/bin/install -c

SRC=.
SHELL=/bin/sh


.SUFFIXES:
.SUFFIXES: .c .o

LIBOBJ=lib/getopt.o lib/fnmatch.o lib/zlib.o
OBJS1=rsync.o exclude.o util.o md4.o main.o checksum.o match.o 
OBJS=$(OBJS1) flist.o io.o compat.o hlink.o token.o $(LIBOBJ)

.c.o:
	$(CC) $(CFLAGS) -c $*.c -o $*.o

all: rsync

install: all
	${INSTALLCMD} -m 755 rsync ${INSTALL_BIN}
	${INSTALLCMD} -m 644 rsync.1 ${INSTALL_MAN}/man1

rsync: $(OBJS) 
	$(CC) $(CFLAGS) -o rsync $(OBJS) $(LIBS)

proto:
	cat *.c | awk -f mkproto.awk > proto.h

clean:
	rm -f *~ $(OBJS) rsync config.cache config.log config.status

dist: 
	tar --exclude-from .ignore -czf dist.tar.gz .
	-mkdir rsync-$(VERSION)
	(cd rsync-$(VERSION) ; tar xzf ../dist.tar.gz)
	tar -czf rsync-$(VERSION).tar.gz rsync-$(VERSION)
	rm -f dist.tar.gz
	echo rsync-$(VERSION) >> .cvsignore
