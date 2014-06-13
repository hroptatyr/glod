### Makefile.am -- glod src/ make file
##
## Copyright (C) 2010-2014 Sebastian Freundt
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
SUFFIXES =

CLEANFILES += version.c
EXTRA_DIST += version.c.in

noinst_LTLIBRARIES += libglod.la
libglod_la_SOURCES =
libglod_la_SOURCES += nifty.h
libglod_la_SOURCES += mem.h
libglod_la_SOURCES += fops.c fops.h
libglod_la_SOURCES += boobs.h
libglod_la_SOURCES += intern.c intern.h
libglod_la_SOURCES += glep.c glep.h
libglod_la_SOURCES += wu-manber-guts.c
libglod_la_SOURCES += levenshtein.c levenshtein.h
libglod_la_SOURCES += version.c version.h

if HAVE_TCBDB
noinst_LTLIBRARIES += libcorpus.la
libcorpus_la_SOURCES =
libcorpus_la_SOURCES += corpus.c corpus.h
libcorpus_la_SOURCES += doc.c doc.h
libcorpus_la_CPPFLAGS = $(AM_CPPFLAGS)
libcorpus_la_CPPFLAGS += $(tokyocabinet_CFLAGS)
libcorpus_la_LDFLAGS = $(AM_LDFLAGS)
libcorpus_la_LDFLAGS += $(tokyocabinet_LIBS)
endif  HAVE_TCBDB

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

noinst_LTLIBRARIES += libbloom.la
libbloom_la_SOURCES =
libbloom_la_SOURCES += libbloom/bloom.c libbloom/bloom.h
libbloom_la_SOURCES += libbloom/bitmap.c libbloom/bitmap.h
libbloom_la_SOURCES += libbloom/murmur.c libbloom/murmur.h
libbloom_la_SOURCES += libbloom/spooky.c libbloom/spooky.h
libbloom_la_CPPFLAGS = $(AM_CPPFLAGS)
libbloom_la_CPPFLAGS += -I$(srcdir)/libbloom


bin_PROGRAMS += glod-charstat
glod_charstat_SOURCES = glod-charstat.c
glod_charstat_SOURCES += glod-charstat.yuck
glod_charstat_LDADD = libglod.la
BUILT_SOURCES += glod-charstat.yucc

bin_PROGRAMS += glod-classify
glod_classify_SOURCES = glod-classify.c
glod_classify_SOURCES += glod-classify.yuck
glod_classify_LDADD = libglod.la
BUILT_SOURCES += glod-classify.yucc

bin_PROGRAMS += glep
glep_SOURCES = glep.c
glep_SOURCES += glep.yuck
glep_CPPFLAGS = $(AM_CPPFLAGS)
glep_CPPFLAGS += -DSTANDALONE
glep_LDADD = libglod.la
BUILT_SOURCES += glep.yucc

bin_PROGRAMS += terms
terms_SOURCES = terms.c
terms_SOURCES += terms.yuck
terms_LDADD = libglod.la
terms_LDADD += libcoru.la
BUILT_SOURCES += terms.yucc
EXTRA_terms_SOURCES =
EXTRA_terms_SOURCES += unicode.bf
EXTRA_terms_SOURCES += unicode.cm

if HAVE_TCBDB
bin_PROGRAMS += tf
tf_SOURCES = tf.c
tf_SOURCES += tf.yuck
tf_CPPFLAGS = $(AM_CPPFLAGS)
tf_CPPFLAGS += -D_GNU_SOURCE
tf_LDADD = libglod.la
tf_LDADD += libcorpus.la
tf_LDADD += libcoru.la
BUILT_SOURCES += tf.yucc
endif  HAVE_TCBDB

noinst_PROGRAMS += porter-stemmer
porter_stemmer_CPPFLAGS = $(AM_CPPFLAGS)
porter_stemmer_CPPFLAGS += -D_GNU_SOURCE

noinst_PROGRAMS += gencls
gencls_SOURCES = gencls.c
gencls_SOURCES += gencls.yuck
gencls_CPPFLAGS = $(AM_CPPFLAGS)
gencls_CPPFLAGS += -D_GNU_SOURCE
BUILT_SOURCES += gencls.yucc

noinst_PROGRAMS += ldmatrix
ldmatrix_SOURCES = ldmatrix.c
ldmatrix_SOURCES += ldmatrix.yuck
ldmatrix_CPPFLAGS = $(AM_CPPFLAGS)
ldmatrix_CPPFLAGS += -D_GNU_SOURCE
ldmatrix_LDADD = libglod.la
BUILT_SOURCES += ldmatrix.yucc

bin_PROGRAMS += unwrap
unwrap_SOURCES = unwrap.c
unwrap_SOURCES += unwrap.yuck
unwrap_CPPFLAGS = $(AM_CPPFLAGS)
unwrap_CPPFLAGS += -D_GNU_SOURCE
BUILT_SOURCES += unwrap.yucc

bin_PROGRAMS += unpgbrk
unpgbrk_SOURCES = unpgbrk.c
unpgbrk_SOURCES += unpgbrk.yuck
unpgbrk_CPPFLAGS = $(AM_CPPFLAGS)
unpgbrk_CPPFLAGS += -D_GNU_SOURCE
BUILT_SOURCES += unpgbrk.yucc

bin_PROGRAMS += gloom
gloom_SOURCES = gloom.c
gloom_SOURCES += gloom.yuck
gloom_CPPFLAGS = $(AM_CPPFLAGS)
gloom_CPPFLAGS += -D_GNU_SOURCE
gloom_LDADD = libcoru.la
gloom_LDADD += libbloom.la -lm
BUILT_SOURCES += gloom.yucc

noinst_PROGRAMS += glid
glid_SOURCES = glid.c
glid_SOURCES += glid.yuck
glid_CPPFLAGS = $(AM_CPPFLAGS)
glid_CPPFLAGS += -D_GNU_SOURCE
glid_LDADD = libcoru.la
BUILT_SOURCES += glid.yucc

if HAVE_INTRIN
noinst_PROGRAMS += fastterms
fastterms_SOURCES = fastterms.c
fastterms_SOURCES += fastterms.yuck
fastterms_CFLAGS = $(AM_CFLAGS)
fastterms_CFLAGS += -msse2
fastterms_CPPFLAGS = $(AM_CPPFLAGS)
fastterms_LDADD = libcoru.la
endif  HAVE_INTRIN
BUILT_SOURCES += fastterms.yucc


## version rules
version.c: version.c.in $(top_builddir)/.version
	$(AM_V_GEN) PATH="$(top_builddir)/build-aux:$${PATH}" \
		yuck$(EXEEXT) scmver --ignore-noscm --force -o $@ \
			--use-reference --reference $(top_builddir)/.version $<

## for dist or dist check we need binaries, so help2man can work
dist-hook: $(bin_PROGRAMS)


## yuck rule
SUFFIXES += .yuck
SUFFIXES += .yucc
.yuck.yucc:
	$(AM_V_GEN) PATH=$(top_builddir)/build-aux:"$${PATH}" \
		yuck$(EXEEXT) gen -o $@ $<


## generate classifier from UnicodeData.txt
BUILT_SOURCES += UnicodeData.txt
BUILT_SOURCES += alpha.codes
BUILT_SOURCES += numer.codes
BUILT_SOURCES += punct.codes

FIELDS =
FIELDS += unicode.bf
FIELDS += unicode.cm

BUILT_SOURCES += $(FIELDS)
CLEANFILES += $(FIELDS)

UnicodeData.txt:
	curl -qgsfL -o $@ "http://www.unicode.org/Public/UNIDATA/$@"

alpha.codes: UnicodeData.txt
	grep ';L[ultmo];' $< > $@

numer.codes: UnicodeData.txt
	grep ';N[dlo];' $< > $@

punct.codes: UnicodeData.txt
	grep ';P[cdo];' $< > $@

unicode.bf: UnicodeData.txt $(builddir)/gencls$(EXEEXT)
	echo "/* autogenerated, do not modify */" > $@
	$(builddir)/gencls$(EXEEXT) -w1 --bitfields < $< >> $@
	$(builddir)/gencls$(EXEEXT) -w2 --bitfields < $< >> $@
	$(builddir)/gencls$(EXEEXT) -w3 --bitfields < $< >> $@

unicode.cm: UnicodeData.txt $(builddir)/gencls$(EXEEXT)
	echo "/* autogenerated, do not modify */" > $@
	$(builddir)/gencls$(EXEEXT) -w1 --bitfields --upper-lower-maps < $< >> $@
	$(builddir)/gencls$(EXEEXT) -w2 --bitfields --upper-lower-maps < $< >> $@
	$(builddir)/gencls$(EXEEXT) -w3 --bitfields --upper-lower-maps < $< >> $@

## Help the developers get nice post-processed source files

## Create preprocessor output (debugging purposes only)
.c.i:
	$(COMPILE) -E -o $@ $<

## Create assembler output (debugging purposes only)
.c.s:
	$(COMPILE) -S -c $(AM_CFLAGS) $<
