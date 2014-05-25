#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define countof(x)	(sizeof(x) / sizeof(*x))

struct mb_s {
	size_t w;
	long unsigned int x;
};

/* utf8 seq ranges */
static const long unsigned int lohi[4U] = {
	16U * (1U << (4U - 1U)),
	16U * (1U << (8U - 1U)),
	16U * (1U << (12U - 1U)),
	16U * (1U << (16U - 1U)),
};

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

static char*
skip_cols(const char *s, int c, unsigned int n)
{
/* like strchr() but N times */
	char *p = NULL;
	for (; n-- > 0 && (p = strchr(s, c)) != NULL; s = p + 1U);
	return p;
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
		} else if ((next = skip_cols(next, ';', 12U)) == NULL) {
			continue;
		} else if (!(y = strtoul(next + 1U, &next, 16U))) {
			continue;
		} else if (*next++ != ';') {
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


static uint_fast32_t *bf;
static size_t bz;
static const size_t bps = sizeof(*bf) * 8U / 2U;
static unsigned int last_off;

static struct mb_s
xwctowb(long unsigned int wc)
{
/* map to width and first wide-character in that width range (base) */
	static const struct mb_s null_mb = {};
	long unsigned int x;
	size_t w;

	if (wc < *lohi) {
		/* we treat ascii manually */
		w = 1U;
		x = 0U;
	} else if (wc < lohi[1U]) {
		/* 110xxxxx 10xxxxxx */
		w = 2U;
		x = lohi[0U];
	} else if (wc < lohi[2U]) {
		/* 1110xxxx 10xxxxxx 10xxxxxx */
		w = 3U;
		x = lohi[1U];
	} else if (wc < lohi[3U]) {
		/* 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
		w = 4U;
		x = lohi[2U];
	} else {
		return null_mb;
	}
	return (struct mb_s){w, x};
}

static void
bf_set(long unsigned int x, uint_fast32_t c, size_t wf)
{
	struct mb_s wb;
	unsigned int off;
	unsigned int mod;

	if (wf && (wb = xwctowb(x)).w != wf) {
		return;
	} else if (wf) {
		/* offset x by base*/
		x -= wb.x;
	}

	off = x / bps;
	mod = x % bps;
	if (off >= bz) {
		const size_t ol = bz;
		bz += (off / 64U + 1U) * 64U;
		bf = realloc(bf, bz * sizeof(*bf));
		memset(bf + ol, 0, (bz - ol) * sizeof(*bf));
	}
	bf[off] |= c << (2U * mod);
	last_off = off;
	return;
}

static void
bf_free(void)
{
	if (bf != NULL) {
		free(bf);
	}
	bf = NULL;
	bz = 0UL;
	last_off = 0U;
	return;
}

static int
fields(size_t width_filter)
{
	char *line = NULL;
	size_t llen = 0U;
	long unsigned int prev = 0U;
	long unsigned int x;

	if (width_filter > countof(lohi)) {
		return -1;
	}

	for (ssize_t nrd; (nrd = getline(&line, &llen, stdin)) > 0; prev = x) {
		char *next;
		uint_fast8_t c;

		x = strtoul(line, &next, 16U);

		if (*next++ != ';') {
			continue;
		} else if (prev > x) {
			fputs("Error: input file not sorted by code\n", stderr);
			return -1;
		} else if ((next = strchr(next, ';')) == NULL) {
			continue;
		}

		/* read class */
		switch (*++next) {
		case 'L':
			c = 0b10U;
			break;
		case 'N':
			c = 0b11U;
			break;
		case 'P':
			c = 0b01U;
			break;
		default:
			/* don't wanna know about it */
			c = 0b00U;
			continue;
		}

		bf_set(x, c, width_filter);
	}


	printf("static const uint_fast32_t gencls%zu[] = {\n", width_filter);
	for (unsigned int i = 0U; i <= last_off; i++) {
		const long long unsigned int c = bf[i];

		printf("\t0x%llxU,\n", c);
	}
	puts("};");
	bf_free();

	free(line);
	return 0;
}


#include "gencls.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	size_t argw = 0U;
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	}

	if (argi->width_arg) {
		argw = strtoul(argi->width_arg, NULL, 10);
	}

	if (argi->upper_lower_maps_flag) {
		lower(argw);
	} else if (argi->bitfields_flag) {
		fields(argw);
	} else {
		cases(argw);
	}

out:
	yuck_free(argi);
	return rc;
}
