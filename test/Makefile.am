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

## summon our helper
include clitoris.am
AM_CLIT_LOG_FLAGS = -v --builddir $(top_builddir)/src
TESTS_ENVIRONMENT = root=$(top_srcdir)

## the actual tests here
terms_TESTS =
TESTS += $(terms_TESTS)
terms_TESTS += terms.01.clit
terms_TESTS += terms.03.clit
terms_TESTS += terms.05.clit
terms_TESTS += terms.06.clit
terms_TESTS += terms.07.clit
terms_TESTS += terms.08.clit
terms_TESTS += terms.09.clit
terms_TESTS += terms.10.clit
terms_TESTS += terms.11.clit
terms_TESTS += terms.12.clit
terms_TESTS += terms.13.clit
terms_TESTS += terms.14.clit
terms_TESTS += terms.15.clit
EXTRA_DIST += 1206796.txt
EXTRA_DIST += 12abrdg.txt
EXTRA_DIST += punct-terms.txt
EXTRA_DIST += utf8-terms.txt

fastterms_TESTS =
if HAVE_INTRIN
TESTS += $(fastterms_TESTS)
endif  HAVE_INTRIN
EXTRA_DIST += $(fastterms_TESTS)
fastterms_TESTS += fastterms.01.clit
fastterms_TESTS += fastterms.02.clit
fastterms_TESTS += fastterms.03.clit
EXTRA_DIST += ft_bug_01.txt

ldmatrix_TESTS =
TESTS += $(ldmatrix_TESTS)
ldmatrix_TESTS += ldmatrix.01.clit
EXTRA_DIST += ldtest1.txt
EXTRA_DIST += ldtest2.txt

unwrap_TESTS =
TESTS += $(unwrap_TESTS)
unwrap_TESTS += unwrap_test_01.clit
unwrap_TESTS += unwrap_test_02.clit
EXTRA_DIST += unwrap_test_01.txt
EXTRA_DIST += unwrap_test_02.txt

unpgbrk_TESTS =
TESTS += $(unpgbrk_TESTS)
unpgbrk_TESTS += unpgbrk_test_01.clit
unpgbrk_TESTS += unpgbrk_test_02.clit
EXTRA_DIST += xetra.txt

uncol_TESTS =
TESTS += $(uncol_TESTS)
uncol_TESTS += uncol.01.clit
uncol_TESTS += uncol.02.clit
uncol_TESTS += uncol.03.clit
uncol_TESTS += uncol.04.clit
uncol_TESTS += uncol.05.clit
uncol_TESTS += uncol.06.clit
EXTRA_DIST += dividend-info.txt
EXTRA_DIST += dividend-inf2.txt

if HAVE_GLEP_REQS
TESTS += $(glep_TESTS)
endif  HAVE_GLEP_REQS

glep_TESTS =
glep_TESTS += glep.01.clit
glep_TESTS += glep.02.clit
glep_TESTS += glep.03.clit
glep_TESTS += glep.04.clit
glep_TESTS += glep.05.clit
glep_TESTS += glep.15.clit
glep_TESTS += glep.16.clit
EXTRA_DIST += words.alrt
EXTRA_DIST += xmpl.01.txt
EXTRA_DIST += xmpl.02.txt
EXTRA_DIST += dax-news.txt
EXTRA_DIST += foo-news.txt
EXTRA_DIST += case-insensitive.alrt
EXTRA_DIST += case-sensitive.alrt

glep_TESTS += glep.06.clit
glep_TESTS += glep.07.clit
glep_TESTS += glep.08.clit
EXTRA_DIST += utf8.alrt utf8.txt
EXTRA_DIST += latin1.alrt latin1.txt
EXTRA_DIST += ascii.alrt ascii.txt

glep_TESTS += glep.09.clit
glep_TESTS += glep.10.clit
EXTRA_DIST += tilm.alrt
EXTRA_DIST += til2.alrt
EXTRA_DIST += tilm.txt

glep_TESTS += glep.11.clit
glep_TESTS += glep.12.clit
glep_TESTS += glep.13.clit
glep_TESTS += glep.14.clit
EXTRA_DIST += stops.alrt

## noyield fuckup
glep_TESTS += glep.17.clit
glep_TESTS += glep.18.clit
EXTRA_DIST += noyield.alrt

## boundary fuckup
glep_TESTS += glep.19.clit
glep_TESTS += glep.20.clit
glep_TESTS += glep.21.clit
glep_TESTS += glep.22.clit
EXTRA_DIST += words.alrt
EXTRA_DIST += short.alrt

## xfix matching for short patterns
glep_TESTS += glep.23.clit
glep_TESTS += glep.24.clit
glep_TESTS += glep.25.clit
glep_TESTS += glep.26.clit
glep_TESTS += small.clit
EXTRA_DIST += short-xfix.txt
EXTRA_DIST += short-prefix.alrt
EXTRA_DIST += short-infix.alrt
EXTRA_DIST += short-suffix.alrt
EXTRA_DIST += small.pats
EXTRA_DIST += small.txt

## match wildcard AND igncase modifier
glep_TESTS += glep.27.clit
glep_TESTS += glep.28.clit
EXTRA_DIST += modci.alrt
EXTRA_DIST += modci.txt

## tk discovered this one
glep_TESTS += glep.29.clit
EXTRA_DIST += tk.alrt
EXTRA_DIST += tk.news

## tk discovered this one
glep_TESTS += glep.30.clit
EXTRA_DIST += tk2.pats
EXTRA_DIST += tk2.news

## tk also discovered this one
glep_TESTS += glep.31.clit
EXTRA_DIST += tk3.pats
EXTRA_DIST += tk3.news


enum_TESTS =
TESTS += $(enum_TESTS)
enum_TESTS += enum.01.clit
enum_TESTS += enum.02.clit
enum_TESTS += enum.03.clit
enum_TESTS += enum.04.clit
enum_TESTS += enum.05.clit
enum_TESTS += enum.06.clit
enum_TESTS += enum.07.clit
enum_TESTS += enum.08.clit
EXTRA_DIST += utf8.txt


clean-local:
	-rm -rf *.tmpd

.NOTPARALLEL:

## Makefile.am ends here
