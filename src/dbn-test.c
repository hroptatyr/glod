#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <tgmath.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "rand.h"
#include "nifty.h"

#define PREFER_NUMERICAL_STABILITY_OVER_SPEED	1

#if defined __INTEL_COMPILER
# pragma warning (disable:1911)
#endif	/* __INTEL_COMPILER */

static float
factorialf(uint8_t n)
{
	static const float table[] = {
		1., 1., 2., 6., 24., 120., 720., 5040.,
		40320., 362880., 3628800., 39916800.,
		479001600., 6227020800., 87178291200., 1307674368000.,
	};
	float res;

	if (LIKELY(n < countof(table))) {
		return table[n];
	}

	/* otherwise proceed from 16! */
	res = table[15U];
	n -= 15U;
	for (float x = 16.; n > 0; n--, x += 1.0) {
		res *= x;
	}
	return res;
}

static double
factorial(uint8_t n)
{
	static const double table[] = {
		1., 1., 2., 6., 24., 120., 720., 5040.,
		40320., 362880., 3628800., 39916800.,
		479001600., 6227020800., 87178291200., 1307674368000.,
	};
	double res;

	if (LIKELY(n < countof(table))) {
		return table[n];
	}

	/* otherwise proceed from 16! */
	res = table[15U];
	n -= 15U;
	for (double x = 16.; n > 0; n--, x += 1.0) {
		res *= x;
	}
	return res;
}

static float
poissf(float lambda, uint8_t n)
{
	float res;

	res = exp(-lambda);
	for (uint8_t i = n; i > 0; i--) {
		res *= lambda;
	}
	res /= factorialf(n);
	return res;
}

static long double
factoriall(uint8_t n)
{
	static const long double table[] = {
		1.l, 1.l, 2.l, 6.l, 24.l, 120.l, 720.l, 5040.l,
		40320.l, 362880.l, 3628800.l, 39916800.l,
		479001600.l, 6227020800.l, 87178291200.l, 1307674368000.l,
	};
	long double res;

	if (LIKELY(n < countof(table))) {
		return table[n];
	}

	/* otherwise proceed from 16! */
	res = table[15U];
	n -= 15U;
	for (long double x = 16.; n > 0; n--, x += 1.0) {
		res *= x;
	}
	return res;
}

static double
poiss(double lambda, uint8_t n)
{
	double res;

	res = exp(-lambda);
	for (uint8_t i = n; i > 0; i--) {
		res *= lambda;
	}
	res /= factorial(n);
	return res;
}

static long double
poissl(long double lambda, uint8_t n)
{
	long double res;

	res = exp(-lambda);
	for (uint8_t i = n; i > 0; i--) {
		res *= lambda;
	}
	res /= factoriall(n);
	return res;
}

static float
sigmaf(float x)
{
#if PREFER_NUMERICAL_STABILITY_OVER_SPEED
	return (1.f + tanh(x / 2.f)) / 2.f;
#else
	return 1.f / (1.f + exp(-x));
#endif
}

static double
sigma(double x)
{
#if PREFER_NUMERICAL_STABILITY_OVER_SPEED
	return (1. + tanh(x / 2.)) / 2.;
#else
	return 1. / (1. + exp(-x));
#endif
}

static long double
sigmal(long double x)
{
#if PREFER_NUMERICAL_STABILITY_OVER_SPEED
	return (1.L + tanh(x / 2.L)) / 2.L;
#else
	return 1.L / (1.L + exp(-x));
#endif
}

/* my own tgmaths */
#define poiss(x, n)	__TGMATH_BINARY_FIRST_REAL_ONLY(x, n, poiss)
#define sigma(x)	__TGMATH_UNARY_REAL_ONLY(x, sigma)


/* mmapping, adapted from fops.h */
typedef struct glodf_s glodf_t;
typedef struct glodfn_s glodfn_t;

struct glodf_s {
	size_t z;
	void *d;
};

struct glodfn_s {
	int fd;
	struct glodf_s fb;
};

static inline glodf_t
mmap_fd(int fd, size_t fz)
{
	void *p;

	p = mmap(NULL, fz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (p == MAP_FAILED) {
		return (glodf_t){.z = 0U, .d = NULL};
	}
	return (glodf_t){.z = fz, .d = p};
}

static inline int
munmap_fd(glodf_t map)
{
	return munmap(map.d, map.z);
}

static glodfn_t
mmap_fn(const char *fn, int flags)
{
	struct stat st;
	glodfn_t res;

	if ((res.fd = open(fn, flags)) < 0) {
		;
	} else if (fstat(res.fd, &st) < 0) {
		res.fb = (glodf_t){.z = 0U, .d = NULL};
		goto clo;
	} else if ((res.fb = mmap_fd(res.fd, st.st_size)).d == NULL) {
	clo:
		close(res.fd);
		res.fd = -1;
	}
	return res;
}

static __attribute__((unused)) int
munmap_fn(glodfn_t f)
{
	int rc = 0;

	if (f.fb.d != NULL) {
		rc += munmap_fd(f.fb);
	}
	if (f.fd >= 0) {
		rc += close(f.fd);
	}
	return rc;
}


/* custom machine */
typedef struct dl_rbm_s *dl_rbm_t;

struct dl_rbm_s {
	size_t nvis;
	size_t nhid;
	float *vbias;
	float *hbias;
	float *w;

	void *priv;
};

struct dl_file_s {
	uint8_t magic[4U];
	uint8_t flags[4U];
	size_t nvis;
	size_t nhid;

	/* offset to the beginning */
	size_t off;
	float data[];
};

static dl_rbm_t
pump(const char *file)
{
	static struct dl_rbm_s res;
	static glodfn_t f;

	if (UNLIKELY((f = mmap_fn(file, O_RDWR)).fd < 0)) {
		goto out;
	} else if (UNLIKELY(f.fb.z < sizeof(struct dl_file_s))) {
		goto out;
	}
	with (struct dl_file_s *fl = f.fb.d) {
		float_t *dp = fl->data + fl->off;

		res.nvis = fl->nvis;
		res.vbias = dp;
		dp += fl->nvis;

		res.nhid = fl->nhid;
		res.hbias = dp;
		dp += fl->nhid;

		res.w = dp;
	}
	res.priv = &f;
	return &res;
out:
	/* and out are we */
	(void)munmap_fn(f);
	return NULL;
}

static int
dump(dl_rbm_t m)
{
	if (UNLIKELY(m == NULL)) {
		return 0;
	}
	return munmap_fn(*(glodfn_t*)m->priv);
}

static dl_rbm_t
crea(const char *file, struct dl_file_s fs)
{
	static glodfn_t f;
	size_t z;
	size_t fz;
	int fd;

	if (UNLIKELY((fd = open(file, O_CREAT | O_RDWR | O_TRUNC, 0666)) < 0)) {
		return NULL;
	}
	/* compute total file size */
	z = fs.nvis + fs.nhid + fs.nvis * fs.nhid;
	ftruncate(fd, fz = z * sizeof(float) + sizeof(fs));

	if (UNLIKELY((f.fb = mmap_fd(f.fd = fd, fz)).d == NULL)) {
		goto out;
	}
	/* just copy the file header over */
	fs.off = 0U;
	memcpy(f.fb.d, &fs, sizeof(fs));

	/* wobble */
	with (float *xp = ((struct dl_file_s*)f.fb.d)->data) {
		for (size_t i = 0; i < z; i++) {
			xp[i] = 0.25f * dr_rand_uni();
		}
	}

	munmap_fn(f);
	return pump(file);

out:
	close(fd);
	return NULL;
}


static uint8_t*
read_tf(const int fd, dl_rbm_t m)
{
	uint8_t *v;
	uint8_t *vp;
	size_t vz;

	/* now then */
	v = vp = malloc(vz = m->nvis * sizeof(*v));
	for (ssize_t nrd; (nrd = read(fd, vp, vz)) > 0; vp += nrd, vz -= nrd);
	return v;
}

static void
popul_ui8(float *restrict x, const uint8_t *n, size_t z)
{
	for (size_t i = 0; i < z; i++) {
		x[i] = (float)(int)n[i];
	}
	return;
}

static int
prop_up(float *restrict h, dl_rbm_t m, const float *vis)
{
/* propagate visible units activation upwards to the hidden units (recon) */
	const size_t nvis = m->nvis;
	const size_t nhid = m->nhid;
	const float *w = m->w;
	const float *b = m->hbias;

#define w(i, j)		w[i * nhid + j]
	for (size_t j = 0; j < nhid; j++) {
		float sum = 0.0;

		for (size_t i = 0; i < nvis; i++) {
			sum += w(i,j) * vis[i];
		}
		h[j] = b[j] + sum;
	}
#undef w
	return 0;
}

static __attribute__((unused)) int
expt_hid(float *restrict h, dl_rbm_t m, const float *hid)
{
	const size_t nhid = m->nhid;

	for (size_t j = 0; j < nhid; j++) {
		h[j] = sigma(hid[j]);
	}
	return 0;
}

static int
smpl_hid(float *restrict h, dl_rbm_t m, const float *hid)
{
/* infer hidden unit states given vis(ible units) */
	const size_t nhid = m->nhid;

	for (size_t j = 0; j < nhid; j++) {
		float rnd = dr_rand_uni();

		if (hid[j] > rnd) {
			h[j] = 0.0;
		} else {
			h[j] = 1.0;
		}
	}
	return 0;
}

static int
prop_down(float *restrict v, dl_rbm_t m, const float *hid)
{
/* propagate hidden units activation downwards to the visible units */
	const size_t nvis = m->nvis;
	const size_t nhid = m->nhid;
	const float *w = m->w;
	const float *b = m->vbias;

#define w(i, j)		w[i * nhid + j]
	for (size_t i = 0; i < nvis; i++) {
		float sum = 0.0;

		for (size_t j = 0; j < nhid; j++) {
			sum += w(i,j) * hid[j];
		}
		v[i] = b[i] + sum;
	}
#undef w
	return 0;
}

static __attribute__((unused)) int
expt_vis(float *restrict v, dl_rbm_t m, const float *vis, const uint8_t *n)
{
	const size_t nvis = m->nvis;
	float nor = 0.f;
	long unsigned int N;

	/* calc \sum exp(v) */
	for (size_t i = 0; i < nvis; i++) {
		nor += v[i] = exp(vis[i]);
	}
	/* calc N */
	for (size_t i = 0; i < nvis; i++) {
		N += n[i];
	}
	with (const float lambda = (float)N / nor) {
		for (size_t i = 0; i < nvis; i++) {
			v[i] = poiss(v[i] * lambda, n[i]);
		}
	}
	return 0;
}

static int
smpl_vis(float *restrict v, dl_rbm_t m, const float *vis, const uint8_t *n)
{
/* infer visible unit states given hid(den units) */
	const size_t nvis = m->nvis;
	float nor = 0.f;
	long unsigned int N;

	/* calc \sum exp(v) */
	for (size_t i = 0; i < nvis; i++) {
		nor += v[i] = exp(vis[i]);
	}
	/* calc N */
	for (size_t i = 0; i < nvis; i++) {
		N += n[i];
	}
	with (const float lambda = (float)N / nor, lexp = exp(-lambda)) {
		for (size_t i = 0; i < nvis; i++) {
			float p = 1.f;
			uint8_t k = 0U;

			while ((p *= dr_rand_uni()) > lexp) {
				k++;
			}
			v[i] = (float)k;
		}
	}
	return 0;
}


/* training and classifying modes */
static void
train(dl_rbm_t m, const uint8_t *v)
{
	const size_t nv = m->nvis;
	const size_t nh = m->nhid;
	float *vo;
	float *ho;
	float *vr;
	float *hr;

	vo = calloc(nv, sizeof(*vo));
	vr = calloc(nv, sizeof(*vr));
	ho = calloc(nh, sizeof(*ho));
	hr = calloc(nh, sizeof(*hr));

	/* populate from input */
	popul_ui8(vo, v, nv);

	/* vhv gibbs */
	prop_up(ho, m, vo);
	smpl_hid(hr, m, hr);
	prop_down(vr, m, hr);
	/* vh gibbs */
	smpl_vis(vr, m, vr, v);
	prop_up(hr, m, vr);
	smpl_hid(hr, m, hr);

	/* bang <v_i h_j> into weights and biasses */
	{
		/* learning rate */
		const float eta = 0.1f;
		float *w = m->w;
		float *vb = m->vbias;
		float *hb = m->hbias;

#define w(i, j)		w[i * nh + j]
		for (size_t i = 0; i < nv; i++) {
			for (size_t j = 0; j < nh; j++) {
				float vho = vo[i] * ho[j];
				float vhr = vr[i] * hr[j];

				w(i, j) += eta * (vho - vhr);
			}
		}
#undef w

		for (size_t i = 0; i < nv; i++) {
			vb[i] += eta * (vo[i] - vr[i]);
		}

		for (size_t j = 0; j < nh; j++) {
			hb[j] += eta * (ho[j] - hr[j]);
		}
	}

	free(vo);
	free(vr);
	free(ho);
	free(hr);
	return;
}

static void
dream(dl_rbm_t m, const uint8_t *v)
{
	const size_t nv = m->nvis;
	const size_t nh = m->nhid;
	float *vo;
	float *ho;
	float *vr;
	float *hr;

	vo = calloc(nv, sizeof(*vo));
	vr = calloc(nv, sizeof(*vr));
	ho = calloc(nh, sizeof(*ho));
	hr = calloc(nh, sizeof(*hr));

	/* populate from input */
	popul_ui8(vo, v, nv);

	/* vhv gibbs */
	prop_up(ho, m, vo);
	smpl_hid(hr, m, hr);
	prop_down(vr, m, hr);
	/* vh gibbs */
	smpl_vis(vr, m, vr, v);

	for (size_t i = 0; i < nv; i++) {
		uint8_t vi = (uint8_t)(int)vr[i];

		if (UNLIKELY(vi)) {
			printf("%zu\t%u\n", i, (unsigned int)vi);
		}
	}

	free(vo);
	free(vr);
	free(ho);
	free(hr);
	return;
}

static int
check(dl_rbm_t m)
{
	int res = 0;

	with (size_t nv = m->nvis, nh = m->nhid) {
		for (size_t i = 0; i < nv; i++) {
			if (UNLIKELY(isnan(m->vbias[i]))) {
				printf("VBIAS[%zu] <- NAN\n", i);
				res = 1;
			}
		}
		for (size_t j = 0; j < nh; j++) {
			if (UNLIKELY(isnan(m->hbias[j]))) {
				printf("HBIAS[%zu] <- NAN\n", j);
				res = 1;
			}
		}
		for (size_t i = 0; i < nv; i++) {
			for (size_t j = 0; j < nh; j++) {
				if (UNLIKELY(isnan(m->w[i * nh + j]))) {
					printf("W[%zu,%zu] <- NAN\n", i, j);
					res = 1;
				}
			}
		}
	}
	return res;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "dbn-test.xh"
#include "dbn-test.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct glod_args_info argi[1];
	dl_rbm_t m = NULL;
	int res = 0;

	if (glod_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	}

	init_rand();
	if (argi->create_given) {
		struct dl_file_s ini = {
			.nvis = 32768U + 4096U,
			.nhid = 256U,
		};

		/* wobble the matrices */
		if ((m = crea("test.rbm", ini)) == NULL) {
			res = 1;
		}
		goto wrout;
	}

	/* read the machine file */
	m = pump("test.rbm");

	if (argi->check_given) {
		res = check(m);
	} else if (argi->train_given) {
		if (!isatty(STDIN_FILENO)) {
			uint8_t *v = read_tf(STDIN_FILENO, m);

			train(m, v);
			free(v);
		}
	} else if (argi->dream_given) {
		if (!isatty(STDIN_FILENO)) {
			uint8_t *v = read_tf(STDIN_FILENO, m);

			dream(m, v);
			free(v);
		}
	}
wrout:
	deinit_rand();
	dump(m);
out:
	glod_parser_free(argi);
	return res;
}
