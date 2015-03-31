#!/usr/bin/clitoris  ## -*- shell-script -*-

$ cat "${srcdir}/utf8.txt" | terms | awk 'BEGIN{n=1}{if (x[$0]) {print x[$0];}else{x[$0]=n;print n++;}}' > enum.03.ref
$ cat "${srcdir}/utf8.txt" | terms | enum
< enum.03.ref
$ rm -f -- enum.03.ref
$
