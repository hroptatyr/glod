#ifndef __PERF_LEVENSHTEIN_H
#define __PERF_LEVENSHTEIN_H

extern int
levenshtein(
	const char *string1, const char *string2,
	int swap_penalty, int substition_penalty,
	int insertion_penalty, int deletion_penalty);

#define init_levenshtein()
#define fini_levenshtein()	(void)levenshtein(NULL, NULL, 0, 0, 0, 0)

#endif /* __PERF_LEVENSHTEIN_H */
