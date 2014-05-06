#!/usr/bin/clitoris  ## -*- shell-script -*-

$ cat <<EOF > "glep_01.res"
foo	${srcdir}/xmpl.01.txt
bar	${srcdir}/xmpl.02.txt
EOF
$ glep -f "${srcdir}/words.alrt" "${srcdir}/xmpl.01.txt" "${srcdir}/xmpl.02.txt"
< "glep_01.res"
$ rm -f "glep_01.res"
$
