# Help the Developers and yourself. Just use the C locale and settings
# for the compilation. They can still be overriden by make LANG=<whatever>
# but that is general a not very good idea
include $(top_builddir)/version.mk

LANG = C
LC_ALL = C

AM_CFLAGS = $(EXTRA_CFLAGS)
AM_CPPFLAGS = -D_POSIX_C_SOURCE=200112L -D_XOPEN_SOURCE=600 -D_BSD_SOURCE
AM_CPPFLAGS += -DTEST

EXTRA_DIST = $(BUILT_SOURCES) $(TESTS)
TESTS =
TEST_EXTENSIONS =
BUILT_SOURCES =
dt_tests =
bin_tests =
batch_tests =
check_PROGRAMS =
CLEANFILES = $(check_PROGRAMS)
SUFFIXES =

TEST_EXTENSIONS += .tst
TESTS_ENVIRONMENT = root=$(top_srcdir)
TST_LOG_COMPILER = $(builddir)/clitoris
AM_TST_LOG_FLAGS = -v --builddir $(top_builddir)/src
AM_LOG_COMPILER = false

## the actual tests here
terms_TESTS =
TESTS += $(terms_TESTS)
terms_TESTS += terms.01.tst
terms_TESTS += terms.03.tst
terms_TESTS += terms.05.tst
terms_TESTS += terms.06.tst
terms_TESTS += terms.07.tst
terms_TESTS += terms.08.tst
terms_TESTS += terms.09.tst
terms_TESTS += terms.10.tst
terms_TESTS += terms.11.tst
terms_TESTS += terms.12.tst
terms_TESTS += terms.13.tst
terms_TESTS += terms.14.tst
terms_TESTS += terms.15.tst
EXTRA_DIST += 1206796.txt
EXTRA_DIST += 12abrdg.txt
EXTRA_DIST += punct-terms.txt
EXTRA_DIST += utf8-terms.txt

fastterms_TESTS =
if HAVE_INTRIN
TESTS += $(fastterms_TESTS)
endif  HAVE_INTRIN
EXTRA_DIST += $(fastterms_TESTS)
fastterms_TESTS += fastterms.01.tst
fastterms_TESTS += fastterms.02.tst
fastterms_TESTS += fastterms.03.tst
EXTRA_DIST += ft_bug_01.txt

ldmatrix_TESTS =
TESTS += $(ldmatrix_TESTS)
ldmatrix_TESTS += ldmatrix.01.tst
EXTRA_DIST += ldtest1.txt
EXTRA_DIST += ldtest2.txt

unwrap_TESTS =
TESTS += $(unwrap_TESTS)
unwrap_TESTS += unwrap_test_01.tst
unwrap_TESTS += unwrap_test_02.tst
EXTRA_DIST += unwrap_test_01.txt
EXTRA_DIST += unwrap_test_02.txt

unpgbrk_TESTS =
TESTS += $(unpgbrk_TESTS)
unpgbrk_TESTS += unpgbrk_test_01.tst
unpgbrk_TESTS += unpgbrk_test_02.tst
EXTRA_DIST += xetra.txt

if HAVE_GLEP_REQS
TESTS += $(glep_TESTS)
endif  HAVE_GLEP_REQS

glep_TESTS =
glep_TESTS += glep.01.tst
glep_TESTS += glep.02.tst
glep_TESTS += glep.03.tst
glep_TESTS += glep.04.tst
glep_TESTS += glep.05.tst
glep_TESTS += glep.15.tst
glep_TESTS += glep.16.tst
EXTRA_DIST += words.alrt
EXTRA_DIST += xmpl.01.txt
EXTRA_DIST += xmpl.02.txt
EXTRA_DIST += dax-news.txt
EXTRA_DIST += foo-news.txt
EXTRA_DIST += case-insensitive.alrt
EXTRA_DIST += case-sensitive.alrt

glep_TESTS += glep.06.tst
glep_TESTS += glep.07.tst
glep_TESTS += glep.08.tst
EXTRA_DIST += utf8.alrt utf8.txt
EXTRA_DIST += latin1.alrt latin1.txt
EXTRA_DIST += ascii.alrt ascii.txt

glep_TESTS += glep.09.tst
glep_TESTS += glep.10.tst
EXTRA_DIST += tilm.alrt
EXTRA_DIST += til2.alrt
EXTRA_DIST += tilm.txt

glep_TESTS += glep.11.tst
glep_TESTS += glep.12.tst
glep_TESTS += glep.13.tst
glep_TESTS += glep.14.tst
EXTRA_DIST += stops.alrt

## noyield fuckup
glep_TESTS += glep.17.tst
glep_TESTS += glep.18.tst
EXTRA_DIST += noyield.alrt

## boundary fuckup
glep_TESTS += glep.19.tst
glep_TESTS += glep.20.tst
glep_TESTS += glep.21.tst
glep_TESTS += glep.22.tst
EXTRA_DIST += words.alrt
EXTRA_DIST += short.alrt

## xfix matching for short patterns
glep_TESTS += glep.23.tst
glep_TESTS += glep.24.tst
glep_TESTS += glep.25.tst
glep_TESTS += glep.26.tst
glep_TESTS += small.tst
EXTRA_DIST += short-xfix.txt
EXTRA_DIST += short-prefix.alrt
EXTRA_DIST += short-infix.alrt
EXTRA_DIST += short-suffix.alrt
EXTRA_DIST += small.pats
EXTRA_DIST += small.txt

## match wildcard AND igncase modifier
glep_TESTS += glep.27.tst
glep_TESTS += glep.28.tst
EXTRA_DIST += modci.alrt
EXTRA_DIST += modci.txt

## tk discovered this one
glep_TESTS += glep.29.tst
EXTRA_DIST += tk.alrt
EXTRA_DIST += tk.news

## tk discovered this one
glep_TESTS += glep.30.tst
EXTRA_DIST += tk2.pats
EXTRA_DIST += tk2.news

## tk also discovered this one
glep_TESTS += glep.31.tst
EXTRA_DIST += tk3.pats
EXTRA_DIST += tk3.news


enum_TESTS =
TESTS += $(enum_TESTS)
enum_TESTS += enum.01.tst
enum_TESTS += enum.02.tst
enum_TESTS += enum.03.tst
enum_TESTS += enum.04.tst
enum_TESTS += enum.05.tst
enum_TESTS += enum.06.tst
enum_TESTS += enum.07.tst
enum_TESTS += enum.08.tst
EXTRA_DIST += utf8.txt


## our friendly helpers
check_PROGRAMS += clitoris
clitoris_CPPFLAGS = $(AM_CPPFLAGS)
clitoris_CPPFLAGS += -D_GNU_SOURCE
clitoris_LDFLAGS = $(AM_LDFLAGS) -static
clitoris_LDADD = -lutil
BUILT_SOURCES += clitoris.yucc

check_PROGRAMS += hxdiff
hxdiff_CPPFLAGS = $(AM_CPPFLAGS)
hxdiff_LDFLAGS = $(AM_LDFLAGS) -static
BUILT_SOURCES += hxdiff.yucc

## yuck rule
SUFFIXES += .yuck
SUFFIXES += .yucc
.yuck.yucc:
	$(AM_V_GEN) PATH=$(top_builddir)/build-aux:"$${PATH}" \
		yuck$(EXEEXT) gen -o $@ $<

clean-local:
	-rm -rf *.tmpd

.NOTPARALLEL:

## Makefile.am ends here
