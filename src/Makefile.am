### Makefile.am -- glod src/ make file
##
## Copyright (C) 2010-2013 Sebastian Freundt
##
## Author:  Sebastian Freundt <hroptatyr@fresse.org>
##
## This file is part of glod.
## 
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
##
## 1. Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
##
## 2. Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
##
## 3. Neither the name of the author nor the names of any contributors
##    may be used to endorse or promote products derived from this
##    software without specific prior written permission.
##
## THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
## IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
## WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
## DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
## FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
## CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
## SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
## BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
## WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
## OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
## IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##
###
include $(top_builddir)/version.mk

AM_CFLAGS = $(EXTRA_CFLAGS)
AM_CPPFLAGS = -D_GNU_SOURCE -D_POSIX_C_SOURCE=201001L -D_XOPEN_SOURCE=800 -D_BSD_SOURCE
AM_LDFLAGS = $(XCCLDFLAGS) -static

bin_PROGRAMS =
noinst_PROGRAMS =
lib_LIBRARIES =
noinst_LIBRARIES =
pkglib_LIBRARIES =
noinst_HEADERS =
pkginclude_HEADERS =
BUILT_SOURCES =
EXTRA_DIST = $(BUILT_SOURCES)
CLEANFILES = 

CLEANFILES += version.c
EXTRA_DIST += version.c.in

noinst_PROGRAMS += facc
facc_SOURCES = facc.c ftab.h
BUILT_SOURCES += date.tab

noinst_LIBRARIES += libglod.a
libglod_a_SOURCES =
libglod_a_SOURCES += nifty.h
libglod_a_SOURCES += mem.h
libglod_a_SOURCES += prchunk.c prchunk.h
libglod_a_SOURCES += gsep.c gsep.h
libglod_a_SOURCES += gtype.c gtype.h
libglod_a_SOURCES += gtype-int.c gtype-int.h
libglod_a_SOURCES += gtype-flt.c gtype-flt.h
libglod_a_SOURCES += gtype-date.c gtype-date.h
libglod_a_SOURCES += strptime.c strptime.h
libglod_a_SOURCES += gtype-na.c gtype-na.h
libglod_a_SOURCES += gtype-str.h
libglod_a_SOURCES += version.c version.h

bin_PROGRAMS += glod
glod_SOURCES = glod.c
glod_LDADD = libglod.a

bin_PROGRAMS += glod-charstat
glod_charstat_SOURCES = glod-charstat.c
glod_charstat_SOURCES += glod-charstat-clo.h glod-charstat-clo.ggo
glod_charstat_LDADD = libglod.a
BUILT_SOURCES += glod-charstat-clo.c glod-charstat-clo.h


version.c: version.c.in $(top_builddir)/version.mk
	sed -e 's,[@]VERSION[@],$(VERSION),g' <$(srcdir)/$@.in >$@

## for dist or dist check we need binaries, so help2man can work
dist-hook: $(bin_PROGRAMS)


## rule for our own compiler
%.tab: $(top_srcdir)/tmpl/%.tmpl
	$(MAKE) $(AM_MAKEFLAGS) facc$(EXEEXT)
	$(AM_V_GEN) $(builddir)/facc$(EXEEXT) $< $@ || $(RM) $@

## gengetopt rule with sed hack to avoid using VERSION and config.h
## depend on Makefile.am to survive a git pull
%.c %.h: %.ggo Makefile.am
	$(AM_V_GEN) gengetopt -l -i $< -F $* \
	&& sed \
	  -e 's,config.h,version.h,g' \
	  -e 's,\(.*\)_PARSER_VERSION VERSION,\1_PARSER_VERSION $(PACKAGE)_version_string,g' \
	  $*.h >$*.h-t \
	&& mv $*.h-t $*.h

## Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
