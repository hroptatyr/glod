#!/bin/sh

## Usage: terms FILEs... | tf-idf.sh CORPUS

$(dirname "${0}")/build-df.sh | \
	join -t'	' -j2 - "${1}" | \
	awk 'BEGIN{FS=OFS="\t"}{print sprintf("%.6f", $2 / $3), $1;}' | \
	sort -nr
