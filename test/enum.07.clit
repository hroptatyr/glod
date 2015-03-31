#!/usr/bin/clitoris  ## -*- shell-script -*-

$ cat "${srcdir}/1206796.txt" | terms | head | enum -s >/dev/null
$ cp .enum.st .enum.ref
$ cat "${srcdir}/1206796.txt" | terms | enum -s -n >/dev/null
$ hxdiff .enum.ref .enum.st
$ rm -f -- .enum.ref .enum.st
$
