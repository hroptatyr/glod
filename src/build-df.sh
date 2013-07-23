#!/bin/sh

## Usage: terms FILEs | build-rel

sort | uniq -c | \
	awk 'BEGIN{OFS="\t"}{print $1,$2}' | \
	sort -t'	' -k2,2
