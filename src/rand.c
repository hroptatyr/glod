/*** rand.c -- randomness
 *
 * Copyright (C) 2008-2013 Sebastian Freundt
 * Copyright (c) 1998-2000 The OpenSSL Project.  All rights reserved.
 *
 * Author:  Sebastian Freundt <freundt@fresse.org>
 *
 * Written by Ulf Moeller and Lutz Jaenicke for the OpenSSL project.
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
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 ***/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <sys/types.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#if defined HAVE_LIBGCRYPT && defined WITH_LIBGCRYPT
# include <gcrypt.h>
#endif	/* LIBGCRYPT */

#include "dbn-base.h"
#include "rand.h"
/* specific implementations */
#include "rand-ziggurat.h"
#include "rand-taus.h"

#if defined HAVE_EGD && defined WITH_EGD
static int _egd_sock = -1;
#endif	/* EGD */
#if defined HAVE_URANDOM
static int _urnd_sock = -1;
#endif

/*! \page rand Randomness of different qualities
 *
 * <P>
 * A lot to tell here ...
 * </P>
 */

/**
 * \addtogroup rand
 * \{ */

#if defined HAVE_EGD && defined WITH_EGD
static int
_open_egd_sock(const char *path)
{
	struct sockaddr_un addr;
	int fd = -1;
	size_t path_len = strlen(path);
	bool success = false;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	if (path_len >= sizeof(addr.sun_path)) {
		return -1;
	}
	strncpy(addr.sun_path, path, sizeof(addr.sun_path));
	path_len += offsetof(struct sockaddr_un, sun_path);
	if (UNLIKELY((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)) {
		return -1;
	}

	while (!success) {
		if (connect(fd, (struct sockaddr *)&addr, path_len) == 0) {
			success = true;
		} else {
			switch (errno) {
#ifdef EINTR
			case EINTR:
#endif
#ifdef EAGAIN
			case EAGAIN:
#endif
#ifdef EINPROGRESS
			case EINPROGRESS:
#endif
#ifdef EALREADY
			case EALREADY:
#endif
				/* No error, try again */
				break;
#ifdef EISCONN
			case EISCONN:
				success = true;
				break;
#endif
			default:
				goto err;	/* failure */
			}
		}
	}
	return fd;

err:
	if (LIKELY(fd != -1)) {
		close(fd);
	}
	return -1;
}

static void
_close_egd_sock(int *sock)
{
	if (*sock != -1) {
		close(*sock);
		*sock = -1;
	}
	return;
}

/**
 * Query NBYTES bytes of entropy form the egd-socket located at path and
 * will write them to BUF.
 * The number of bytes is not limited by the maximum chunk size of EGD,
 * which is 255 bytes. If more than 255 bytes are wanted, several chunks
 * of entropy bytes are requested. The connection is left open until the
 * query is completed.
 *
 * \return -1 if an error occurred during the transaction or
 *   num the number of bytes read from the EGD socket
 *   This number is either the number of bytes requested or smaller, if
 *   the EGD pool is drained and the daemon signals that the pool is empty. */
static int
_query_egd(void *buf, size_t bytes, int sock)
{
	int num, numbytes;
	char egdbuf[2];

	/* initial query, first we want something,
	 * second the number of bytes we want */
	egdbuf[0] = 1;
	egdbuf[1] = bytes < 255 ? bytes : 255;

	numbytes = 0;
	while (numbytes != 2) {
		num = write(sock, egdbuf + numbytes, 2 - numbytes);
		if (num >= 0) {
			numbytes += num;
		} else {
			switch (errno) {
#ifdef EINTR
			case EINTR:
#endif
#ifdef EAGAIN
			case EAGAIN:
#endif
				/* No error, try again */
				break;
			default:
				/* failure */
				return -1;
			}
		}
	}

	numbytes = 0;
	while (numbytes != 1) {
		num = read(sock, egdbuf, 1);
		if (LIKELY(num > 0)) {
			/* can only be 1 */
			break;
		} else if (UNLIKELY(num == 0)) {
			/* descriptor closed */
			return -1;
		} else {
			switch (errno) {
#ifdef EINTR
			case EINTR:
#endif
#ifdef EAGAIN
			case EAGAIN:
#endif
				/* No error, try again */
				break;
			default:
				/* failure */
				return -1;
			}
		}
	}

	if (UNLIKELY(egdbuf[0] == 0)) {
		/* no more data in the pool */
		return -1;
	}

	/* egdbuf[0] contains the numbers of bytes we can read */
	numbytes = 0;
	while (numbytes != egdbuf[0]) {
		num = read(sock, (char*)buf + numbytes, egdbuf[0] - numbytes);
		if (LIKELY(num > 0)) {
			numbytes += num;
		} else if (UNLIKELY(num == 0)) {
			/* descriptor closed */
			return -1;
		} else {
			switch (errno) {
#ifdef EINTR
			case EINTR:
#endif
#ifdef EAGAIN
			case EAGAIN:
#endif
				/* No error, try again */
				break;
			default:
				/* failure */
				return -1;
			}
		}
	}
	return (int)(egdbuf[0]);
}
#endif	/* EGD */


#if defined HAVE_LIBGCRYPT && defined WITH_LIBGCRYPT
static inline int
_query_gcry(void *buf, size_t bytes)
{
	gcry_randomize((unsigned char*)buf, bytes, GCRY_STRONG_RANDOM);
	return bytes;
}
#endif	/* LIBGCRYPT */


#if defined HAVE_URANDOM && 0
static int
_open_urnd_sock(const char *path)
{
	int fd;

	fd = open(path, O_RDONLY);
	if (fd == -1) {
		return -1;
	}

	return fd;
}

static void
_close_urnd_sock(int *sock)
{
	if (*sock != -1) {
		close(*sock);
		*sock = -1;
	}
	return;
}

static inline int
_query_urnd(void *buf, size_t bytes, int sock)
{
	int n = 0;

	do {
		n = read(sock, buf, bytes);
	} while (n == -1 && errno == EINTR);
	return n;
}
#endif	/* URANDOM */


/* high level functions */
/**
 * Macro to obtain a random number using the EGD API. */
#define _rand_fn_egd_sz(_name, _type, _sz)				\
	_type								\
	_name(void)							\
	{								\
		_type res;						\
		if (LIKELY(_query_egd(&res, (_sz), _egd_sock) != -1)) {	\
			return res;					\
		}							\
		return -1;						\
	}								\
	enum

/**
 * Macro to obtain a random number using the EGD API. */
#define _rand_fn_egd(_name, _type)			\
	_rand_fn_egd_sz(_name, _type, sizeof(_type))


/**
 * Macro to obtain a random number using the gcry API. */
#define _rand_fn_gcry_sz(_name, _type, _sz)				\
	_type								\
	_name(void)							\
	{								\
		_type res;						\
		(void)_query_gcry(&res, (_sz));				\
		return res;						\
	}								\
	enum

/**
 * Macro to obtain a random number using the gcry API. */
#define _rand_fn_gcry(_name, _type)			\
	_rand_fn_gcry_sz(_name, _type, sizeof(_type))

/**
 * Macro to obtain a random number using the /dev/urandom device. */
#define _rand_fn_urnd_sz(_name, _type, _sz)				\
	_type								\
	_name(void)							\
	{								\
		_type res;						\
		(void)_query_urnd(&res, (_sz), _urnd_sock);		\
		return res;						\
	}								\
	enum

/**
 * Macro to obtain a random number using the /dev/urandom device. */
#define _rand_fn_urnd(_name, _type)			\
	_rand_fn_urnd_sz(_name, _type, sizeof(_type))


/**
 * Macro to obtain a random number using the Tausworthe PRNG. */
#define _rand_fn_taus_sz(_name, _type, _sz)				\
	_type								\
	_name(void)							\
	{								\
		_type res;						\
		(void)_query_urnd(&res, (_sz), _urnd_sock);		\
		return res;						\
	}								\
	enum

/**
 * Macro to obtain a random number using the Tausworthe PRNG. */
#define _rand_fn_urnd(_name, _type)			\
	_rand_fn_urnd_sz(_name, _type, sizeof(_type))

#if defined HAVE_EGD && defined WITH_EGD
# define _rand_fn	_rand_fn_egd
# define _rand_fn_sz	_rand_fn_egd_sz
#elif defined HAVE_LIBGCRYPT && defined WITH_LIBGCRYPT
# define _rand_fn	_rand_fn_gcry
# define _rand_fn_sz	_rand_fn_gcry_sz
#elif defined HAVE_URANDOM && 0
# define _rand_fn	_rand_fn_urnd
# define _rand_fn_sz	_rand_fn_urnd_sz
#elif defined USE_TAUS_GENERATOR || 1
/**
 * Pseudo definition to poll a RNG device.
 * In the case of the Tausworthe PRNG this is not needed. */
# define _rand_fn(name, args...)	_rand_fn_sz(name, args)
/**
 * Pseudo definition to poll a RNG device.
 * In the case of the Tausworthe PRNG this is not needed. */
# define _rand_fn_sz(name, args...)	typedef void name ## _t
#else
# error "don't know how to create high-quality random numbers"
#endif


/* high level functions */
/**
 * Defines dr_rand_char() which returns a random signed char. */
_rand_fn(dr_rand_char, char);
/**
 * Defines dr_rand_short() which returns a random short int. */
_rand_fn(dr_rand_short, short int);
/**
 * Defines dr_rand_int() which returns a random int. */
_rand_fn(dr_rand_int, int);
/**
 * Defines dr_rand_long() which returns a random long int. */
_rand_fn(dr_rand_long, long int);

/**
 * Return a uniformly distributed random float in [0,1]. */
fpfloat_t
dr_rand_uni(void)
{
	unsigned int tmp = dr_rand_int();
	return (fpfloat_t)tmp / (fpfloat_t)((unsigned int)-1);
}


/* initialisers */
void
init_rand(void)
{
#if defined HAVE_EGD && defined WITH_EGD
	if (_egd_sock == -1) {
		_egd_sock = _open_egd_sock("/var/run/egd-pool");
	}
#endif	/* EGD */
#if defined HAVE_LIBGCRYPT && defined WITH_LIBGCRYPT
	gcry_check_version("1.4.0");
#endif	/* LIBGCRYPT */
#if defined HAVE_URANDOM && 0
	if (_urnd_sock == -1) {
		_urnd_sock = _open_urnd_sock("/dev/urandom");
	}
#endif	/* URANDOM */
	init_rand_ziggurat();
	init_rand_taus();
	return;
}

void
deinit_rand(void)
{
#if defined HAVE_EGD && defined WITH_EGD
	_close_egd_sock(&_egd_sock);
#endif	/* EGD */
#if defined HAVE_URANDOM && 0
	_close_urnd_sock(&_urnd_sock);
#endif	/* URANDOM */
	return;
}

/* rand.c ends here */
