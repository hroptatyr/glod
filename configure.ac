AC_PREREQ([2.64])
AC_INIT([glod], [0.1], [http://www.ga-group.nl/])
AC_CONFIG_AUX_DIR([.])
AC_CONFIG_HEADER([src/config.h])
AC_CONFIG_SRCDIR([src/])

AM_INIT_AUTOMAKE([foreign])

## compiler detection
CFLAGS="-g -O3"
AC_PROG_CC(["icc -static-intel" icc gcc clang cc])
AC_PROG_CPP
AC_HEADER_STDC
AC_PROG_CC_STDC
AC_PROG_RANLIB
AM_PROG_CC_C_O

## output
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([src/Makefile])
AC_OUTPUT

## configure.ac ends here
