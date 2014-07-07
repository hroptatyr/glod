#!/usr/bin/awk -f

{
	if (FILENAME != OLD_FN) {
		FN = substr(FILENAME, index(FILENAME, "/") + 1);
		OLD_FN = FILENAME;
	}
	if (lang[$0]) {
		lang[$0] = lang[$0] "," FN;
	} else {
		lang[$0] = FN;
	}
}

END {
	for (i in lang) {
		print "\"" i "\"i -> \"" lang[i] "\"";
	}
}

