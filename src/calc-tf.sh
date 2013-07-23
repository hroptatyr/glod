#!/bin/sh

## Usage: tf-idf.sh CORPUS FILE

cat "${2}" | \
	$(dirname "${0}")/build-rel.sh | \
	join -t'	' -j2 - "${1}" | \
	awk 'BEGIN{FS=OFS="\t"}{print sprintf("%.6f", $2 / $3), $1;}' | \
	sort -nr
