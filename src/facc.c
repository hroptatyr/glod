#include <stdio.h>
#include <stdio_ext.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

/* types and shit, take them from the helper */
typedef long unsigned int __facc_bmsk_t;
#include "ftab.h"

#define bmsk_t		__facc_bmsk_t
#define UNUSED(x)	x __attribute__((unused))

typedef __facc_bmsk_t __full_btbl_t[256];

/* super table struct */
typedef struct ftctx_s {
	int pos;
	int pat;
	__full_btbl_t tbl[MAX_LENGTH];
	char *spstr[64];
} *ftctx_t;


/* helper non-sense */
static __facc_ltbl_t __facc_ltbl = {0};

#if defined OPTIM_SIZE
static __facc_meta_t __facc_meta = {0};
static unsigned char __facc_meta_cnt = 1;

static inline unsigned int
facc_set_meta(unsigned char c)
{
	unsigned char d = __facc_meta_cnt++;
	__facc_meta[c] = d;
	return d;
}

#else  /* !OPTIM_SIZE */
static const unsigned char __facc_meta_cnt = FACC_LST - FACC_1ST;

static inline unsigned int
facc_set_meta(unsigned char c)
{
	return facc_get_meta(c);
}

#endif	/* OPTIM_SIZE */

static inline void
facc_or_bmsk(__facc_btbl_t tbl, unsigned char c, __facc_bmsk_t msk)
{
	unsigned int d = facc_get_meta(c);
	if (!d) {
		d = facc_set_meta(c);
	}
	tbl[d] |= msk;
	return;
}

static inline void
facc_or_lmsk(size_t len, __facc_bmsk_t msk)
{
	__facc_ltbl[len] |= msk;
	return;
}

static int
new_spstr(ftctx_t ctx, int n, const char *str)
{
	ctx->spstr[n] = strdup(str);
	return n;
}


/* parser goodness */
static int
pspec(ftctx_t ctx, char spc)
{
#define BTBL(a)			(ctx->tbl[ctx->pos+a])
#define OR_BMSK(a, b, c)	(facc_or_bmsk(BTBL(a), (b), (c)))
	bmsk_t pat = (1 << ctx->pat);
	int res = 0;

	switch (spc) {
	case 'Y':
		/* emit ^19[0-9][0-9]|20[0-3][0-9]$ */
		fputs("4dg, 1902 to 2038\n", stdout);
		/* first can be 1 or 2 */
		OR_BMSK(0, '1', pat);
		OR_BMSK(0, '2', pat);
		/* second is 0 or 9 */
		OR_BMSK(1, '0', pat);
		OR_BMSK(1, '9', pat);
		/* third and fourth is anything */
		for (char c = '0'; c <= '9'; c++) {
			OR_BMSK(2, c, pat);
			OR_BMSK(3, c, pat);
		}
		res = 4;
		break;
	case 'y':
		fputs("2dg, 00 to 99\n", stdout);
		/* first and second is anything */
		for (char c = '0'; c <= '9'; c++) {
			OR_BMSK(0, c, pat);
			OR_BMSK(1, c, pat);
		}
		res = 2;
		break;
	case 'm':
		fputs("2dg, 01,02,...,11,12\n", stdout);
		OR_BMSK(0, '0', pat);
		OR_BMSK(0, '1', pat);
		for (char c = '0'; c <= '9'; c++) {
			OR_BMSK(1, c, pat);
		}
		res = 2;
		break;
	case 'd':
		fputs("2dg, 01,02,...,31\n", stdout);
		OR_BMSK(0, '0', pat);
		OR_BMSK(0, '1', pat);
		OR_BMSK(0, '2', pat);
		OR_BMSK(0, '3', pat);
		for (char c = '0'; c <= '9'; c++) {
			OR_BMSK(1, c, pat);
		}
		res = 2;
		break;
	case 'q':
		fputs("1dg, 1,2,3,4\n", stdout);
		OR_BMSK(0, '1', pat);
		OR_BMSK(0, '2', pat);
		OR_BMSK(0, '3', pat);
		OR_BMSK(0, '4', pat);
		res = 1;
		break;
	default:
		fputs("unk\n", stdout);
		return -1;
	}
	ctx->pos += res;
	return res;
}

static int
chline(const char *line, void *clo)
{
	int len = 0;
	ftctx_t ctx = clo;

	fputs("date format spec:\n", stdout);
	ctx->pos = 0;
	for (const char *p = line; *p != '\0'; p++) {
		switch (*p) {
		case '%': {
			/* must be followed by a spec */
			int pclen = pspec(ctx, *++p);
			if (pclen == -1) {
				return -1;
			}
			len += pclen;
			break;
		}
		default:
			fprintf(stdout, "1ch, %c\n", *p);
			facc_or_bmsk(ctx->tbl[ctx->pos++], *p, (1 << ctx->pat));
			len++;
			break;
		}
	}
	facc_or_lmsk(len, (1 << ctx->pat));
	fprintf(stdout, "total length: %d, patno %d\n\n", len, ctx->pat);
	new_spstr(ctx, ctx->pat++, line);
	return len;
}

static void
line_by_line(FILE *fp, int(*cb)(const char *line, void *clo), void *clo)
{
	char *line;
	size_t len;
	int lno = 0;
	ftctx_t ctx = clo;

	/* no threads reading this stream */
	__fsetlocking(fp, FSETLOCKING_BYCALLER);
	/* set the pattern counter */
	ctx->pat = 0;
	/* loop over the lines */
	for (line = NULL; !feof_unlocked(fp); lno++) {
		ssize_t n;

		n = getline(&line, &len, fp);
		if (n < 0) {
			break;
		}
		/* terminate the string accordingly */
		line[n - 1] = '\0';
		/* process line, check if it's a comment first */
		if (line[0] == '#' || line[0] == '\0') {
			;
		} else if (cb(line, clo) < 0) {
			;
		}
	}
	/* get rid of resources */
	free(line);
	return;
}


/* output non-sense */
#include <math.h>

static void
emit_ftbl(FILE *of, ftctx_t ctx)
{
	int msk_sz = 1 << (ilogb(ctx->pat) + 1);
	int pch_sz = 1 << (ilogb(__facc_meta_cnt) + 1);

	fputs("/* auto-generated, fuck off please */\n", of);
	switch (msk_sz) {
	case 2:
	case 4:
	case 8:
		fputs("typedef unsigned char __facc_bmsk_t;\n", of);
		break;
	case 16:
		fputs("typedef short unsigned int __facc_bmsk_t;\n", of);
		break;
	case 32:
		fputs("typedef unsigned int __facc_bmsk_t;\n", of);
		break;
	case 64:
		fputs("typedef unsigned int __facc_bmsk_t;\n", of);
		break;
	default:
		fputs("no support for more than 64 patterns\n", stderr);
		return;
	}
	fputs("#include \"ftab.h\"\n\n", of);

	/* populate meta table */
#if defined OPTIM_SIZE
	fputs("\
static __facc_meta_t __facc_meta = {\n", of);
	for (unsigned char c = FACC_1ST; c < FACC_LST; c++) {
		unsigned int d = facc_get_meta(c);
		if (d) {
			fprintf(of, "\t['%c'] = %u,\n", c, d);
		}
	}
	fputs("};\n", of);
#endif	/* OPTIM_SIZE */

	/* populate length table */
	fputs("\
static __facc_ltbl_t __facc_ltbl = {\n", of);
	for (int i = 0; i < MAX_LENGTH + 1; i++) {
		bmsk_t pat = facc_get_lmsk(i);
		if (pat) {
			fprintf(of, "\t[%d] = 0x%lx,\n", i, pat);
		}
	}
	fputs("};\n", of);

	/* populate bit mask table */
	fprintf(of, "\
typedef __facc_bmsk_t __ftab_btbl_t[%d];\n\
static __ftab_btbl_t __facc_ftbl[%d] = {\n", pch_sz, MAX_LENGTH);
	for (int i = 0; i < MAX_LENGTH; i++) {
		for (unsigned char c = FACC_1ST; c < FACC_LST; c++) {
			bmsk_t pat = facc_get_bmsk(ctx->tbl[i], c);
			if (pat != 0) {
				fprintf(of, "\t[%d][%u] = 0x%lx,\n",
					i, facc_get_meta(c), pat);
			}
		}
	}
	fputs("};\n", of);

	/* and the actual specs */
	fprintf(of, "static const char *__facc_spec[%d] = {\n", ctx->pat);
	for (int i = 0; i < ctx->pat; i++) {
		fprintf(of, "\t\"%s\",\n", ctx->spstr[i]);
	}
	fputs("};\n", of);
	return;
}


int
main(int argc, char *argv[])
{
	FILE *fp = fopen(argv[1], "rc");
	FILE *of = fopen("ftab.c", "wc");
	/* super table */
	struct ftctx_s ctx[1];

	if (fp == NULL) {
		return 1;
	} else if (of == NULL) {
		fclose(fp);
		return 1;
	}
	/* init the table and context */
	memset(ctx, 0, sizeof(*ctx));
	/* compile the bollocks */
	line_by_line(fp, chline, ctx);
	/* emit compilable code */
	emit_ftbl(of, ctx);
	/* and out */
	fclose(fp);
	/* close the output file */
	fclose(of);
	return 0;
}

/* facc.c ends here */
