#
# Makefile.  Generated from Makefile.in by configure.
#

sourcefiles = $(srcdir)/socketcand.c \
	$(srcdir)/state_raw.c \
	$(srcdir)/can-os.c \
	$(srcdir)/can-so.c \
	$(srcdir)/extract-line.c 

executable = socketcand

sourcefiles_cl = $(srcdir)/socketcandcl.c \
	$(srcdir)/can-os.c \
	$(srcdir)/can-so.c \
	$(srcdir)/extract-line.c 

sourcefiles_br = $(srcdir)/socketbridge.c 

executable_cl = socketcandcl

executable_br = socketbridge

srcdir = .
prefix = /usr/local
exec_prefix = ${prefix}
datarootdir = ${prefix}/share
bindir = ${exec_prefix}/bin
mandir = ${datarootdir}/man
sysconfdir = ${prefix}/etc
CFLAGS =  -Wall -Wno-parentheses -DPF_CAN=29 -DAF_CAN=PF_CAN
LIBS = -lpthread 
init_script = yes
rc_script = no
CC = gcc
LDFLAGS = 
DEFS = -DHAVE_CONFIG_H
CPPFLAGS = 
sysroot = 

all: socketcand socketcandcl socketbridge

socketcand: $(sourcefiles)
	$(CC) $(CFLAGS) $(DEFS) $(CPPFLAGS) $(LDFLAGS) -I . -I ./include -o $(executable) $(sourcefiles) $(LIBS)

socketcandcl: $(sourcefiles_cl)
	$(CC) $(CFLAGS) $(DEFS) $(CPPFLAGS) $(LDFLAGS) -I . -I ./include -o $(executable_cl) $(sourcefiles_cl) 

socketbridge: $(sourcefiles_br)	
	$(CC) $(CFLAGS) $(DEFS) $(CPPFLAGS) $(LDFLAGS) -I . -I ./include -o $(executable_br) $(sourcefiles_br) 

clean:
	rm -f $(executable) $(executable_cl) $(executable_br) *.o

distclean:
	rm -rf $(executable) $(executable_cl) *.o *~ Makefile config.h debian_pack configure config.log config.status autom4te.cache socketcand_*.deb

install: socketcand
	mkdir -p $(DESTDIR)$(sysroot)$(bindir)
	cp $(srcdir)/socketcand $(DESTDIR)$(sysroot)$(bindir)/
	cp $(srcdir)/socketcandcl $(DESTDIR)$(sysroot)$(bindir)/
	mkdir -p $(DESTDIR)$(sysroot)$(mandir)
	cp $(srcdir)/socketcand.1 $(DESTDIR)$(sysroot)$(mandir)/
	mkdir -p $(DESTDIR)$(sysroot)/etc/
	install -m 0644 $(srcdir)/etc/socketcand.conf $(DESTDIR)$(sysroot)/etc/
	if [ $(init_script) = yes ]; then mkdir -p $(DESTDIR)$(sysroot)/etc/init.d; install --mode=755 $(srcdir)/init.d/socketcand $(DESTDIR)$(sysroot)/etc/init.d/socketcand; fi
	if [ $(rc_script) = yes ]; then install --mode=755 $(srcdir)/rc.d/socketcand $(DESTDIR)$(sysroot)/etc/rc.d/socketcand; fi
