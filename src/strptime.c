/*** strptime.c -- faster date parsing
 *
 * Copyright (C) 2009, 2010  Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of libffff.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/

#include <stdint.h>
#include <stdbool.h>
#define INCL_TBLS
#include "strptime.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

static const int tenners[10] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90};

/* week days of the years 1970 to 2038 */
static const uint8_t jan01_wday[] = {
	/* 1970 */
	4, 5, 6, 1, 2, 3, 4, 6,
	/* 1978 */
	0, 1, 2, 4, 5, 6, 0, 2,
	/* 1986 */
	3, 4, 5, 0, 1, 2, 3, 5,
	/* 1994 */
	6, 0, 1, 3, 4, 5, 6, 1,
	/* 2002 */
	2, 3, 4, 6, 0, 1, 2, 4,
	/* 2010 */
	5, 6, 0, 2, 3, 4, 5, 0,
	/* 2018 */
};
	
static int
get_year2dg(const char *buf)
{
/* we have to do year - 1900, so we just use the third and fourth char
 * and add 100 if it's <= 38 (for 2038) */
	int yy = tenners[(buf[0] - '0')] + (int)(buf[1] - '0');
	return yy <= 38 ? yy + 100 : yy;
}

static int
get_year(const char *buf)
{
/* we have to do year - 1900, so we just use the third and fourth char
 * and add 100 if buf[1] is '0' */
	return (buf[1] == '0' ? 100 : 0) +
		tenners[(buf[2] - '0')] + (int)(buf[3] - '0');
}

static int
get_month(const char *buf)
{
	return (buf[0] == '0' ? 0 : 10) + (int)(buf[1] - '0') - 1;
}

static int
get_day(const char *buf)
{
	return tenners[(buf[0] - '0')] + (int)(buf[1] - '0');
}

static int
get_qrt(const char *buf)
{
	int q = (buf[0] - '0');
	if (q > 4) {
		return 0;
	}
	return q * 3 - 1;
}

static int
get_hour(const char *buf)
{
	return tenners[(buf[0] - '0')] + (int)(buf[1] - '0');
}

static int
get_minute(const char *buf)
{
	return tenners[(buf[0] - '0')] + (int)(buf[1] - '0');
}

static int
get_second(const char *buf)
{
	return tenners[(buf[0] - '0')] + (int)(buf[1] - '0');
}

static int
yday(int y, int m, int d)
{
	return d + __mon_yday[__leapp(y)][m-1];
}


static int
pspec(const char *buf, char fmt, struct tm *restrict tm)
{
	switch (fmt) {
	case 'Y':
		tm->tm_year = get_year(buf);
		return 4;
	case 'y':
		tm->tm_year = get_year2dg(buf);
		return 2;
	case 'm':
		tm->tm_mon = get_month(buf);
		return 2;
	case 'd':
		tm->tm_mday = get_day(buf);
		return 2;
	case 'q':
		tm->tm_mon = get_qrt(buf);
		tm->tm_mday = 0;
		return 1;
	default:
		return -1;
	}
}

FDEFU int
glod_strptime(const char *buf, const char *fmt, struct tm *restrict tm)
{
/* like strptime() with %Y-%m-%d as format */
	for (; *fmt && *buf; fmt++) {
		int pclen;
		switch (*fmt) {
		case '%':
			/* must be a spec then */
			if ((pclen = pspec(buf, *++fmt, tm)) < 0) {
				return -1;
			}
			buf += pclen;
			break;
		default:
			/* just overread it */
			if (UNLIKELY(*fmt != *buf++)) {
				return -1;
			}
			break;
		}
	}
	/* need set this as timegm makes use of it */
	tm->tm_yday = yday(tm->tm_year, tm->tm_mon + 1, tm->tm_mday) - 1;
	/* indicate success */
	return 0;
}

/* strptime.c ends here */
