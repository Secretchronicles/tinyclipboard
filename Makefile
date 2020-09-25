# tinyclipboard - a cross-platform C library for accessing the clipboard.
#
# Copyright © 2016 Marvin Gülker <m-guelker@guelkerdev.de>
#
# All rights reserved. See the README and LICENSE files for the
# licensing conditions.

CC := cc
AR := ar
INSTALL := install
CFLAGS := -Wall -Wextra -pedantic -Wno-unused-parameter -g
LDFLAGS := -g
DESTDIR :=
PREFIX := /usr/local

sonum := 1
sominnum := 0
soname := libtinyclipboard.so.$(sonum)
realname := $(soname).$(sominnum)

all: compile

tinyclipboard.o: src/tinyclipboard.c include/tinyclipboard.h
	$(CC) $(CFLAGS) $< -c -o $@
tinyclipboard.fpic.o: src/tinyclipboard.c include/tinyclipboard.h
	$(CC) $(CFLAGS) -fPIC $< -c -o $@
libtinyclipboard.a: tinyclipboard.o
	$(AR) rcs $@ $<
$(realname): tinyclipboard.fpic.o
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$(soname) -o $@ $<

compile: libtinyclipboard.a $(realname)

examples_x11: compile
	cd examples && $(CC) $(CFLAGS) $(LDLFLAGS) -I../include read.c ../libtinyclipboard.a -lX11 -o read
	cd examples && $(CC) $(CFLAGS) $(LDLFLAGS) -I../include write.c ../libtinyclipboard.a -lX11 -o write
	cd examples && $(CC) $(CFLAGS) $(LDLFLAGS) -I../include write2.c ../libtinyclipboard.a -lX11 -o write2
	cd examples && $(CC) $(CFLAGS) $(LDLFLAGS) -I../include version.c ../libtinyclipboard.a -lX11 -o version
	cd examples && $(CC) $(CFLAGS) $(LDLFLAGS) -I../include unicode.c ../libtinyclipboard.a -lX11 -o unicode

examples_win32: compile
	cd examples && $(CC) $(CFLAGS) $(LDLFLAGS) -I../include read.c ../libtinyclipboard.a -o read
	cd examples && $(CC) $(CFLAGS) $(LDLFLAGS) -I../include write.c ../libtinyclipboard.a -o write
	cd examples && $(CC) $(CFLAGS) $(LDLFLAGS) -I../include write2.c ../libtinyclipboard.a -o write2
	cd examples && $(CC) $(CFLAGS) $(LDLFLAGS) -I../include version.c ../libtinyclipboard.a -o version
	cd examples && $(CC) $(CFLAGS) $(LDLFLAGS) -I../include unicode.c ../libtinyclipboard.a -o unicode

install: compile
	$(INSTALL) -m 0644 -D include/tinyclipboard.h $(DESTDIR)$(PREFIX)/include/tinyclipboard.h
	$(INSTALL) -m 0644 -D libtinyclipboard.so.1.0 $(DESTDIR)$(PREFIX)/lib/libtinyclipboard.so.1.0
	$(INSTALL) -m 0644 -D libtinyclipboard.a $(DESTDIR)$(PREFIX)/lib/libtinyclipboard.a
	for manpage in `ls man/*.3` ; do \
		$(INSTALL) -m 0644 -D $$manpage $(DESTDIR)$(PREFIX)/share/man/man3/`basename $$manpage` ; \
	done

clean:
	rm -f *.o *.a *.so.*
	rm -f examples/{read,write,write2,version}
	rm -rf html

htmlman:
	mkdir -p html
	for manpage in `ls man/*.3` ; do \
		groff -k -T html -m mandoc $$manpage > html/`basename $$manpage | sed s/\\\./_/`.html ; \
	done
