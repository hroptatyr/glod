#!/usr/bin/clitoris  ## -*- shell-script -*-

$ cat "${srcdir}/utf8.txt" "${srcdir}/utf8.txt" "${srcdir}/1206796.txt" | terms | awk 'BEGIN{n=1}{if (x[$0]) {print x[$0];}else{x[$0]=n;print n++;}}' > enum.04.ref
$ cat "${srcdir}/utf8.txt" "${srcdir}/utf8.txt" "${srcdir}/1206796.txt" | terms | enum
< enum.04.ref
$ rm -f -- enum.04.ref
$
