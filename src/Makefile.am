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

noinst_LIBRARIES += libglod.a
libglod_a_SOURCES =
libglod_a_SOURCES += nifty.h
libglod_a_SOURCES += mem.h
libglod_a_SOURCES += fops.h
libglod_a_SOURCES += boobs.h
libglod_a_SOURCES += alrt.c alrt.h
libglod_a_SOURCES += glep.c glep.h
libglod_a_SOURCES += wu-manber-guts.c
libglod_a_SOURCES += corpus.c corpus.h
libglod_a_SOURCES += version.c version.h
libglod_a_CPPFLAGS = $(AM_CPPFLAGS)
libglod_a_CPPFLAGS += $(tokyocabinet_CFLAGS)

noinst_LIBRARIES += libdbn.a
libdbn_a_SOURCES =
libdbn_a_SOURCES += dbn-base.c dbn-base.h
libdbn_a_SOURCES += rand.c rand.h
libdbn_a_SOURCES += rand-ziggurat.c rand-ziggurat.h
libdbn_a_SOURCES += rand-taus.c rand-taus.h
libdbn_a_SOURCES += neuron.c neuron.h
libdbn_a_SOURCES += layer.c layer.h
libdbn_a_SOURCES += maths.c maths.h
libdbn_a_SOURCES += rbm.c rbm.h
libdbn_a_SOURCES += dbn.c dbn.h
libdbn_a_CPPFLAGS = $(AM_CPPFLAGS)
libdbn_a_CPPFLAGS += -fopenmp

noinst_LIBRARIES += librnd.a
librnd_a_SOURCES =
librnd_a_SOURCES += rand.c rand.h
librnd_a_SOURCES += rand-ziggurat.c rand-ziggurat.h
librnd_a_SOURCES += rand-taus.c rand-taus.h
librnd_a_CPPFLAGS = $(AM_CPPFLAGS)

bin_PROGRAMS += glod-charstat
glod_charstat_SOURCES = glod-charstat.c
glod_charstat_SOURCES += glod-charstat.ggo
glod_charstat_LDADD = libglod.a
BUILT_SOURCES += glod-charstat.x glod-charstat.xh

bin_PROGRAMS += glod-classify
glod_classify_SOURCES = glod-classify.c
glod_classify_SOURCES += glod-classify.ggo
glod_classify_LDADD = libglod.a
BUILT_SOURCES += glod-classify.x glod-classify.xh

bin_PROGRAMS += glod-alert
glod_alert_SOURCES = glod-alert.c
glod_alert_SOURCES += glod-alert.ggo
glod_alert_LDADD = libglod.a
BUILT_SOURCES += glod-alert.x glod-alert.xh

bin_PROGRAMS += glep
glep_SOURCES = glep.c
glep_SOURCES += glep.ggo
glep_CPPFLAGS = $(AM_CPPFLAGS)
glep_CPPFLAGS += -DSTANDALONE
glep_LDADD = libglod.a
BUILT_SOURCES += glep.x glep.xh

bin_PROGRAMS += terms
terms_SOURCES = terms.c
terms_SOURCES += terms.ggo
terms_LDADD = libglod.a
BUILT_SOURCES += terms.x terms.xh

noinst_PROGRAMS += tf
tf_SOURCES = tf.c
tf_SOURCES += tf.ggo
tf_CPPFLAGS = $(AM_CPPFLAGS)
tf_CPPFLAGS += -D_GNU_SOURCE
tf_LDFLAGS = $(AM_LDFLAGS)
tf_LDFLAGS += $(tokyocabinet_LIBS)
tf_LDADD = libglod.a
BUILT_SOURCES += tf.x tf.xh

noinst_PROGRAMS += dbn
dbn_SOURCES = dbn-test.c
dbn_CPPFLAGS = $(AM_CPPFLAGS)
dbn_LDFLAGS = $(AM_LDFLAGS)
dbn_LDFLAGS += -static
dbn_LDADD = libglod.a
dbn_LDADD += librnd.a
BUILT_SOURCES += dbn-test.x dbn-test.xh


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

## Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
