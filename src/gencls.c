#include <stdlib.h>
#include <stdio.h>

int
main(int argc, char *argv[])
{
	char *line = NULL;
	size_t llen = 0U;
	ssize_t nrd;
	/* utf8 seq ranges */
	long unsigned int lohi[4U];
	size_t argw = 0U;

	for (size_t w = 0U; w < 4U; w++) {
		/* lo and hi values for char ranges wrt W */
		lohi[w] = 16 * (1U << ((w + 1U) * 4U - 1U));
	}

	if (argc > 1) {
		argw = strtoul(argv[1U], NULL, 10);
	}

	while ((nrd = getline(&line, &llen, stdin)) > 0) {
		long unsigned int x = strtoul(line, NULL, 16);
		size_t w;

		if (x < *lohi) {
			/* we treat ascii manually */
			continue;
		} else if (x < lohi[1U]) {
			w = 2U;
		} else if (x < lohi[2U]) {
			w = 3U;
		} else if (x < lohi[3U]) {
			w = 4U;
		}
		if (argw && w != argw) {
			/* we want a specific width */
			continue;
		}
		printf("\t\tcase U%zu(\"\\u%04lx\"):\n", w, x);
	}
	free(line);
	return 0;
}
