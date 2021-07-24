#
# Makefile.  Generated from Makefile.in by configure.
#

sourcefiles = $(srcdir)/can-server.c \
	$(srcdir)/state_raw.c \
	$(srcdir)/can-os.c \
	$(srcdir)/can-so.c \
	$(srcdir)/extract-line.c 

executable = can-server

sourcefiles_cl = $(srcdir)/can-client.c \
	$(srcdir)/can-os.c \
	$(srcdir)/can-so.c \
	$(srcdir)/extract-line.c 

sourcefiles_br = $(srcdir)/can-bridge.c 

executable_cl = can-client

executable_br = can-bridge

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

all: can-server can-client can-bridge

can-server: $(sourcefiles)
	$(CC) $(CFLAGS) $(DEFS) $(CPPFLAGS) $(LDFLAGS) -I . -I ./include -o $(executable) $(sourcefiles) $(LIBS)

can-client: $(sourcefiles_cl)
	$(CC) $(CFLAGS) $(DEFS) $(CPPFLAGS) $(LDFLAGS) -I . -I ./include -o $(executable_cl) $(sourcefiles_cl) 

can-bridge: $(sourcefiles_br)	
	$(CC) $(CFLAGS) $(DEFS) $(CPPFLAGS) $(LDFLAGS) -I . -I ./include -o $(executable_br) $(sourcefiles_br) 

clean:
	rm -f $(executable) $(executable_cl) $(executable_br) *.o

distclean:
	rm -rf $(executable) $(executable_cl) *.o *~ Makefile config.h debian_pack configure config.log config.status autom4te.cache socketcand_*.deb

install: can-server
	mkdir -p $(DESTDIR)$(sysroot)$(bindir)
	cp $(srcdir)/can-server $(DESTDIR)$(sysroot)$(bindir)/
	cp $(srcdir)/can-client $(DESTDIR)$(sysroot)$(bindir)/
	mkdir -p $(DESTDIR)$(sysroot)$(mandir)
	cp $(srcdir)/can-server.1 $(DESTDIR)$(sysroot)$(mandir)/
	mkdir -p $(DESTDIR)$(sysroot)/etc/
	install -m 0644 $(srcdir)/etc/can-server.conf $(DESTDIR)$(sysroot)/etc/
	if [ $(init_script) = yes ]; then mkdir -p $(DESTDIR)$(sysroot)/etc/init.d; install --mode=755 $(srcdir)/init.d/can-server $(DESTDIR)$(sysroot)/etc/init.d/can-server; fi
	if [ $(rc_script) = yes ]; then install --mode=755 $(srcdir)/rc.d/can-server $(DESTDIR)$(sysroot)/etc/rc.d/can-server; fi
