### Makefile.am -- glod master make file
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

## overcome NLS stupidity, who needs more than 1 language anyway?
LANG = C
LC_ALL = C

ACLOCAL_AMFLAGS = -I m4

SUBDIRS =
SUBDIRS += src
SUBDIRS += test

EXTRA_DIST =
DISTCLEANFILES =

EXTRA_DIST += build-aux/git-version-gen

DISTCLEANFILES += version.mk

## stuff from the tmpl/ dir
EXTRA_DIST += tmpl/date.tmpl

EXTRA_DIST += README.md

dist-hook:
	echo $(VERSION) > $(distdir)/version \
	&& touch -r $(top_builddir)/version.mk $(distdir)/version

## Makefile.am ends here
