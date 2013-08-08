/*** rand.h -- strong randomness
 *
 * Copyright (C) 2008-2013 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@fresse.org>
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
#if !defined INCLUDED_rand_h_
#define INCLUDED_rand_h_

/* uniform stuff */
/**
 * Return a random signed char, uniformly distributed. */
extern char dr_rand_char(void);
/**
 * Return a random short int, uniformly distributed. */
extern short int dr_rand_short(void);
/**
 * Return a random int, uniformly distributed. */
extern int dr_rand_int(void);
/**
 * Return a random long int, uniformly distributed. */
extern long int dr_rand_long(void);
/**
 * Return a uniformly distributed random float in [0,1]. */
extern float dr_rand_uni(void);

/**
 * Return a sample drawn from a unit gaussian distribution. */
/* defined in rand-ziggurat.c */
extern float dr_rand_norm(void);
/**
 * Return a gaussian sample, centred at MU and with variance SIGMA. */
extern float dr_rand_gauss(float mu, float sigma);

/**
 * Return a binomial sample meeting expectation P. */
extern float dr_rand_binom1(float p);

/**
 * Return a binomial sample meeting expectation P. */
extern float dr_rand_binom(unsigned int n, float p);

/**
 * Return a unit-scaled gamma sampla with shape K. */
extern float dr_rand_gamma(float k);

/**
 * Return a sample from the Poisson distribution of shape LAMBDA. */
extern float dr_rand_poiss(float lambda);

/* initialiser */
/**
 * Initialise the rand subsystem, used for various kinds of randomness. */
extern void init_rand(void);
/**
 * Deinitialise the rand substem. */
extern void deinit_rand(void);

#endif	/* INCLUDED_rand_h_ */
