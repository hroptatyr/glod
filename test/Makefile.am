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
EXTRA_DIST += 1206796.txt
EXTRA_DIST += 12abrdg.txt

tf_TESTS =
TESTS += $(tf_TESTS)
tf_TESTS += tf.01.tst
tf_TESTS += tf.02.tst
tf_TESTS += tf.03.tst
tf_TESTS += tf.04.tst
tf_TESTS += tf.05.tst
tf_TESTS += tf.06.tst
tf_TESTS += tf.07.tst
tf_TESTS += tf.08.tst
tf_TESTS += tf.09.tst
tf_TESTS += tf.10.tst
tf_TESTS += tf.11.tst
tf_TESTS += tf.12.tst
tf_TESTS += tf.13.tst
tf_TESTS += tf.14.tst
tf_TESTS += tf.15.tst
tf_TESTS += tf.16.tst
tf_TESTS += tf.17.tst
tf_TESTS += tf.18.tst
tf_TESTS += tf.19.tst
tf_TESTS += tf.20.tst
tf_TESTS += tf.21.tst
tf_TESTS += tf.22.tst
tf_TESTS += tf.23.tst
tf_TESTS += tf.24.tst
tf_TESTS += tf.25.tst
tf_TESTS += tf.xx.tst

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

glep_TESTS =
TESTS += $(glep_TESTS)
glep_TESTS += glep.01.tst
glep_TESTS += glep.02.tst
glep_TESTS += glep.03.tst
glep_TESTS += glep.04.tst
glep_TESTS += glep.05.tst
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


## our friendly helpers
check_PROGRAMS += clitoris
clitoris_CPPFLAGS = $(AM_CPPFLAGS)
clitoris_CPPFLAGS += -D_GNU_SOURCE
clitoris_LDFLAGS = $(AM_LDFLAGS) -static
clitoris_LDADD = -lutil
BUILT_SOURCES += clitoris.yucc

## yuck rule
SUFFIXES += .yuck
SUFFIXES += .yucc
.yuck.yucc:
	$(AM_V_GEN) PATH=$(top_builddir)/build-aux:"$${PATH}" \
		yuck$(EXEEXT) gen -o $@ $<

clean-local:
	-rm -rf *.tmpd

## Makefile.am ends here
