# Generated automatically from Makefile.in by configure.
# Makefile for rsync. This is processed by configure to produce the final
# Makefile

prefix=/usr/local
exec_prefix=${prefix}
INSTALL_BIN=$(exec_prefix)/bin
INSTALL_MAN=$(prefix)/man

LIBS=
CC=gcc
CFLAGS=-g -O2

INSTALLCMD=/usr/bin/install -c

srcdir=.
SHELL=/bin/sh


.SUFFIXES:
.SUFFIXES: .c .o

LIBOBJ=lib/getopt.o lib/fnmatch.o lib/zlib.o lib/compat.o
OBJS1=rsync.o exclude.o util.o md4.o main.o checksum.o match.o syscall.o
OBJS=$(OBJS1) flist.o io.o compat.o hlink.o token.o uidlist.o $(LIBOBJ)

# note that the -I. is needed to handle config.h when using VPATH
.c.o:
	$(CC) -I. -I$(srcdir) $(CFLAGS) -c $< -o $@

all: rsync

install: all
	-mkdir -p ${INSTALL_BIN}
	${INSTALLCMD} -m 755 rsync ${INSTALL_BIN}
	-mkdir -p ${INSTALL_MAN}/man1
	${INSTALLCMD} -m 644 $(srcdir)/rsync.1 ${INSTALL_MAN}/man1

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
