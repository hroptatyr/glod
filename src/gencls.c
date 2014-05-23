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
	static const struct mb_s null_mb = {};
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

static void
cases(size_t width_filter)
{
	char *line = NULL;
	size_t llen = 0U;

	for (ssize_t nrd; (nrd = getline(&line, &llen, stdin)) > 0;) {
		long unsigned int x = strtoul(line, NULL, 16);
		struct mb_s r = xwctomb(x);

		if (!r.w) {
			/* error or so? */
			continue;
		} else if (width_filter && r.w != width_filter) {
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
	return;
}

static void
lower(size_t width_filter)
{
	char *line = NULL;
	size_t llen = 0U;

	for (ssize_t nrd; (nrd = getline(&line, &llen, stdin)) > 0;) {
		char *next;
		long unsigned int x = strtoul(line, &next, 16U);
		long unsigned int y;
		struct mb_s r;
		struct mb_s s;

		if (*next++ != ';') {
			continue;
		} else if (!(y = strtoul(next, NULL, 16U))) {
			continue;
		} else if (!(r = xwctomb(x)).w) {
			/* error or so? */
			continue;
		} else if (width_filter && r.w != width_filter) {
			/* we want a specific width */
			continue;
		} else if (!(s = xwctomb(y)).w) {
			/* error or so? */
			continue;
		}
#if 0
		/* only works with icc :( */
		printf("\t\tcase U%zu(\"\\u%04lx\"):\n", r.w, x);
#else  /* !0 */
		printf("\t\tcase 0x%lxU/*U%zu(\"\\u%04lx\")*/:\n", r.x, r.w, x);
#endif	/* 0 */
		printf("\t\t\t/*U%zu(\"\\u%04lx\")*/;\n", s.w, y);
		switch (s.w) {
		case 4U:
			printf("\t\t\t*t++ = 0x%02lxU;\n", s.x >> 24U & 0xffU);
		case 3U:
			printf("\t\t\t*t++ = 0x%02lxU;\n", s.x >> 16U & 0xffU);
		case 2U:
			printf("\t\t\t*t++ = 0x%02lxU;\n", s.x >> 8U & 0xffU);
		case 1U:
			printf("\t\t\t*t++ = 0x%02lxU;\n", s.x >> 0U & 0xffU);
		default:
			puts("\t\t\tbreak;");
			break;
		}
	}
	free(line);
	return;
}


int
main(int argc, char *argv[])
{
	size_t argw = 0U;
	unsigned int lmapp = 0U;

	init_lohi();

	if (argc > 1) {
		argw = strtoul(argv[1U], NULL, 10);
		if (argc > 2) {
			lmapp = 1U;
		}
	}

	if (!lmapp) {
		cases(argw);
	} else {
		lower(argw);
	}
	return 0;
}
