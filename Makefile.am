# This is the auto-Makefile for Taylor UUCP
# This is processed by automake to create Makefile.in.

SUBDIRS = lib uuconf unix

bin_PROGRAMS = cu
info_TEXINFOS = uucp.texi
man_MANS = cu.1

AM_CFLAGS = $(WARN_CFLAGS)

LDADD = unix/libunix.a uuconf/libuuconf.a lib/libuucp.a

UUHEADERS = uucp.h uudefs.h uuconf.h policy.h system.h sysdep.h getopt.h

cu_SOURCES = cu.h cu.c prot.c log.c chat.c conn.c copy.c $(UUHEADERS)

EXTRA_DIST = cu.1 contrib sample

dist-hook:
	-rm -rf $(distdir)/contrib/CVS $(distdir)/sample/CVS

install-exec-hook:
	for f in cu; do \
	  chown $(OWNER) $(DESTDIR)$(bindir)/$${f}; \
	  chmod 4555 $(DESTDIR)$(bindir)/$${f}; \
	done
