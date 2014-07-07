all:

stops.pats: $(wildcard stops/*)
	scripts/make-stops.awk $^ | sort > $@
