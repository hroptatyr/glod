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
AM_LDFLAGS = $(XCCLDFLAGS)

bin_PROGRAMS =
noinst_PROGRAMS =
lib_LTLIBRARIES =
noinst_LTLIBRARIES =
pkglib_LTLIBRARIES =
noinst_HEADERS =
pkginclude_HEADERS =
BUILT_SOURCES =
EXTRA_DIST = $(BUILT_SOURCES)
CLEANFILES = 

CLEANFILES += version.c
EXTRA_DIST += version.c.in

noinst_LTLIBRARIES += libglod.la
libglod_la_SOURCES =
libglod_la_SOURCES += nifty.h
libglod_la_SOURCES += mem.h
libglod_la_SOURCES += fops.h
libglod_la_SOURCES += boobs.h
libglod_la_SOURCES += alrt.c alrt.h
libglod_la_SOURCES += glep.c glep.h
libglod_la_SOURCES += wu-manber-guts.c
libglod_la_SOURCES += version.c version.h

noinst_LTLIBRARIES += libcorpus.la
libcorpus_la_SOURCES =
libcorpus_la_SOURCES += corpus.c corpus.h
libcorpus_la_SOURCES += doc.c doc.h
libcorpus_la_CPPFLAGS = $(AM_CPPFLAGS)
libcorpus_la_CPPFLAGS += $(tokyocabinet_CFLAGS)
libcorpus_la_LDFLAGS = $(AM_LDFLAGS)
libcorpus_la_LDFLAGS += $(tokyocabinet_LIBS)

noinst_LTLIBRARIES += libcoru.la
libcoru_la_SOURCES =
EXTRA_libcoru_la_SOURCES =
libcoru_la_SOURCES += coru/cocore.c coru/cocore.h
libcoru_la_SOURCES += coru/switch.c coru/switch.h
libcoru_la_SOURCES += coru/platform.h
libcoru_la_SOURCES += coru/valgrind.h
libcoru_la_SOURCES += coru/memcheck.h
EXTRA_libcoru_la_SOURCES += coru/switch-arm.c
EXTRA_libcoru_la_SOURCES += coru/switch-ppc_osx.c
EXTRA_libcoru_la_SOURCES += coru/switch-x86.c
EXTRA_libcoru_la_SOURCES += coru/switch-x86_64.c


bin_PROGRAMS += glod-charstat
glod_charstat_SOURCES = glod-charstat.c
glod_charstat_SOURCES += glod-charstat.ggo
glod_charstat_LDADD = libglod.la
BUILT_SOURCES += glod-charstat.x glod-charstat.xh

bin_PROGRAMS += glod-classify
glod_classify_SOURCES = glod-classify.c
glod_classify_SOURCES += glod-classify.ggo
glod_classify_LDADD = libglod.la
BUILT_SOURCES += glod-classify.x glod-classify.xh

bin_PROGRAMS += glod-alert
glod_alert_SOURCES = glod-alert.c
glod_alert_SOURCES += glod-alert.ggo
glod_alert_LDADD = libglod.la
BUILT_SOURCES += glod-alert.x glod-alert.xh

bin_PROGRAMS += glep
glep_SOURCES = glep.c
glep_SOURCES += glep.ggo
glep_CPPFLAGS = $(AM_CPPFLAGS)
glep_CPPFLAGS += -DSTANDALONE
glep_LDADD = libglod.la
BUILT_SOURCES += glep.x glep.xh

bin_PROGRAMS += terms
terms_SOURCES = terms.c
terms_SOURCES += terms.ggo
terms_LDADD = libglod.la
terms_LDADD += libcoru.la
BUILT_SOURCES += terms.x terms.xh
EXTRA_terms_SOURCES =
EXTRA_terms_SOURCES += alpha.2.cases
EXTRA_terms_SOURCES += alpha.3.cases
EXTRA_terms_SOURCES += alpha.4.cases
EXTRA_terms_SOURCES += numer.2.cases
EXTRA_terms_SOURCES += numer.3.cases
EXTRA_terms_SOURCES += numer.4.cases
EXTRA_terms_SOURCES += punct.2.cases
EXTRA_terms_SOURCES += punct.3.cases
EXTRA_terms_SOURCES += punct.4.cases

bin_PROGRAMS += tf
tf_SOURCES = tf.c
tf_SOURCES += tf.ggo
tf_CPPFLAGS = $(AM_CPPFLAGS)
tf_CPPFLAGS += -D_GNU_SOURCE
tf_LDADD = libglod.la
tf_LDADD += libcorpus.la
tf_LDADD += libcoru.la
BUILT_SOURCES += tf.x tf.xh

noinst_PROGRAMS += porter-stemmer
porter_stemmer_CPPFLAGS = $(AM_CPPFLAGS)
porter_stemmer_CPPFLAGS += -D_GNU_SOURCE

noinst_PROGRAMS += gencls
gencls_CPPFLAGS = $(AM_CPPFLAGS)
gencls_CPPFLAGS += -D_GNU_SOURCE


version.c: version.c.in $(top_builddir)/version.mk
	sed -e 's,[@]VERSION[@],$(VERSION),g' <$(srcdir)/$@.in >$@

## for dist or dist check we need binaries, so help2man can work
dist-hook: $(bin_PROGRAMS)


## gengetopt rule with sed hack to avoid using VERSION and config.h
## depend on Makefile.am to survive a git pull
%.x %.xh: %.ggo Makefile.am
	$(AM_V_GEN) gengetopt -l -c x -H xh -i $< -F $* \
	&& sed \
	  -e 's,config.h,version.h,g' \
	  -e 's,\(.*\)_PARSER_VERSION VERSION,\1_PARSER_VERSION $(PACKAGE)_version_string,g' \
	  $*.xh >$*.xh-t \
	&& mv $*.xh-t $*.xh

## generate classifier from UnicodeData.txt
BUILT_SOURCES += UnicodeData.txt
BUILT_SOURCES += alpha.codes
BUILT_SOURCES += numer.codes
BUILT_SOURCES += punct.codes

BUILT_SOURCES += alpha.2.cases
BUILT_SOURCES += alpha.3.cases
BUILT_SOURCES += alpha.4.cases
BUILT_SOURCES += numer.2.cases
BUILT_SOURCES += numer.3.cases
BUILT_SOURCES += numer.4.cases
BUILT_SOURCES += punct.2.cases
BUILT_SOURCES += punct.3.cases
BUILT_SOURCES += punct.4.cases

UnicodeData.txt:
	curl -qgsfL -o $@ "http://www.unicode.org/Public/UNIDATA/$@"

alpha.codes: UnicodeData.txt
	cut -d';' -f1,3 $< | grep ';L[ultmo]' | cut -d';' -f1 > $@

numer.codes: UnicodeData.txt
	cut -d';' -f1,3 $< | grep ';N[dlo]' | cut -d';' -f1  > $@

punct.codes: UnicodeData.txt
	cut -d';' -f1,3 $< | grep ';P[cdo]' | cut -d';' -f1  > $@

alpha.2.cases: alpha.codes $(builddir)/gencls
	$(builddir)/gencls 2 < $< > $@

alpha.3.cases: alpha.codes $(builddir)/gencls
	$(builddir)/gencls 3 < $< > $@

alpha.4.cases: alpha.codes $(builddir)/gencls
	$(builddir)/gencls 4 < $< > $@

numer.2.cases: numer.codes $(builddir)/gencls
	$(builddir)/gencls 2 < $< > $@

numer.3.cases: numer.codes $(builddir)/gencls
	$(builddir)/gencls 3 < $< > $@

numer.4.cases: numer.codes $(builddir)/gencls
	$(builddir)/gencls 4 < $< > $@

punct.2.cases: punct.codes $(builddir)/gencls
	$(builddir)/gencls 2 < $< > $@

punct.3.cases: punct.codes $(builddir)/gencls
	$(builddir)/gencls 3 < $< > $@

punct.4.cases: punct.codes $(builddir)/gencls
	$(builddir)/gencls 4 < $< > $@

## Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
