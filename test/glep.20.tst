#!/usr/bin/clitoris  ## -*- shell-script -*-

## we generate a pattern 11 mod 16 without hanging over
## at the buffer boundary:
## 00003ff0  |foo is bar.foo i|
$ i=0; while [ ${i} -lt 1500 ]; do \
	echo "foo is bar"; true $((i=i+1)); \
  done | \
	glep -c -S -f "${srcdir}/words.alrt"
foo	1500	<stdin>
bar	1500	<stdin>
$
