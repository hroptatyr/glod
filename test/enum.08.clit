#!/usr/bin/clitoris  ## -*- shell-script -*-

$ cat "${srcdir}/1206796.txt" | terms | head | enum -s >/dev/null
$ cp .enum.st .enum.ref
$ cat "${srcdir}/1206796.txt" | terms | enum -s -N | tail -n3
0
0
0
$ hxdiff .enum.ref .enum.st
$ rm -f -- .enum.ref .enum.st
$
