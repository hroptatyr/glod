#include <stdlib.h>
#include <stdio.h>

struct mb_s {
	size_t w;
	long unsigned int x;
};

/* utf8 seq ranges */
static long unsigned int lohi[4U];

static void
init_lohi(void)
{
	for (size_t w = 0U; w < 4U; w++) {
		/* lo and hi values for char ranges wrt W */
		lohi[w] = 16 * (1U << ((w + 1U) * 4U - 1U));
	}
	return;
}

static struct mb_s
xwctomb(long unsigned int wc)
{
	static const struct mb_s null_mb;
	long unsigned int x;
	size_t w;

	if (wc < *lohi) {
		/* we treat ascii manually */
		return null_mb;
	} else if (wc < lohi[1U]) {
		/* 110xxxxx 10xxxxxx */
		w = 2U;
		x = 0xc0U | (wc >> 6U) & ((1U << 5U) - 1U);
		x <<= 8U;
		x |= 0x80U | (wc >> 0U) & ((1U << 6U) - 1U);
	} else if (wc < lohi[2U]) {
		/* 1110xxxx 10xxxxxx 10xxxxxx */
		w = 3U;
		x = 0xe0U | (wc >> 12U) & ((1U << 4U) - 1U);
		x <<= 8U;
		x |= 0x80U | (wc >> 6U) & ((1U << 6U) - 1U);
		x <<= 8U;
		x |= 0x80U | (wc >> 0U) & ((1U << 6U) - 1U);
	} else if (wc < lohi[3U]) {
		/* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
		w = 4U;
		x = 0xe0U | (wc >> 18U) & ((1U << 3U) - 1U);
		x <<= 8U;
		x |= 0x80U | (wc >> 12U) & ((1U << 6U) - 1U);
		x <<= 8U;
		x |= 0x80U | (wc >> 6U) & ((1U << 6U) - 1U);
		x <<= 8U;
		x |= 0x80U | (wc >> 0U) & ((1U << 6U) - 1U);
	} else {
		return null_mb;
	}
	return (struct mb_s){w, x};
}

int
main(int argc, char *argv[])
{
	char *line = NULL;
	size_t llen = 0U;
	ssize_t nrd;
	size_t argw = 0U;

	init_lohi();

	if (argc > 1) {
		argw = strtoul(argv[1U], NULL, 10);
	}

	while ((nrd = getline(&line, &llen, stdin)) > 0) {
		long unsigned int x = strtoul(line, NULL, 16);
		struct mb_s r = xwctomb(x);

		if (!r.w) {
			/* error or so? */
			continue;
		} else if (argw && r.w != argw) {
			/* we want a specific width */
			continue;
		}
#if 0
		/* only works with icc :( */
		printf("\t\tcase U%zu(\"\\u%04lx\"):\n", r.w, x);
#else  /* !0 */
		printf("\t\tcase 0x%lxU/*U%zu(\"\\u%04lx\")*/:\n", r.x, r.w, x);
#endif	/* 0 */
	}
	free(line);
	return 0;
}
