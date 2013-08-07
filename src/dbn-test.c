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
#include <assert.h>
#include "rand.h"
#include "nifty.h"

/* blas */
#if defined USE_BLAS
# include <mkl_cblas.h>
#endif	/* USE_BLAS */

#define PREFER_NUMERICAL_STABILITY_OVER_SPEED	1

/* pick an implementation */
#if !defined SALAKHUTDINOV && !defined GEHLER
#define GEHLER		1
#endif	/* !SALAKHUTDINOV && !GEHLER */

#if defined __INTEL_COMPILER
# pragma warning (disable:1911)
#endif	/* __INTEL_COMPILER */

#define PI		3.141592654f
#define E		2.718281828f


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


/* my alibi blas */
#if !defined USE_BLAS
typedef long int MKL_INT;

static inline float
cblas_sdot(
	const MKL_INT N,
	const float *X, const MKL_INT incX,
	const float *Y, const MKL_INT incY)
{
	float sum = 0.f;

	for (MKL_INT i = 0; i < N; i++, X += incX, Y += incY) {
		sum += *X * *Y;
	}
	return sum;
}
#endif	/* !USE_BLAS */


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
		const float norm = 1 / (float)z;
		for (size_t i = 0; i < z; i++) {
			xp[i] = norm * dr_rand_norm();
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

static __attribute__((unused)) float
poiss_lambda_ui8(const uint8_t *n, size_t z)
{
	/* N is the number of total incidents (occurence times count) */
	long unsigned int N = 0UL;

	/* calc N and n */
	for (size_t i = 0; i < z; i++) {
		N += n[i];
	}
	return (float)N / (float)z;
}

static __attribute__((unused)) float
poiss_lambda_f(const float *v, size_t z)
{
	/* incident number */
	long unsigned int N = 0UL;

	for (size_t i = 0; i < z; i++) {
		N += (long int)v[i];
	}
	return log((float)N) / (float)z;
}

static float
binom1_rnd(float /*ex*/p/*ectation*/)
{
	float rnd = dr_rand_uni();

	if (p > rnd) {
		return 1.f;
	}
	return 0.f;
}

static float
binom_rnd(unsigned int n, float /*ex*/p/*ectation*/)
{
/* flip N coins and sum up their faces */
	float res = 0.f;

	while (n--) {
		res += binom1_rnd(p);
	}
	return res;
}

static float
dr_rand_gamma(float k)
{
/* New version based on Marsaglia and Tsang, "A Simple Method for
 * generating gamma variables", ACM Transactions on Mathematical
 * Software, Vol 26, No 3 (2000), p363-372.
 *
 * gsl's take on it. */

	static float gamma_large(float k)
	{
		const float third = 1.f / 3.f;
		float d = k - third;
		float c = third / sqrt(d);
		float v;

		while (1) {
			float u;
			float x;

			do {
				x = dr_rand_norm();
				v = 1.f + c * x;
			} while (v <= 0);

			v = v * v * v;
			while (UNLIKELY((u = dr_rand_uni()) <= 0.f));

			if (u < 1.f - 0.0331f * x * x * x * x) {
				break;
			}
			with (float lu = log(u), lv = log(v)) {
				if (lu < 0.5f * x * x + d * (1.f - v + lv)) {
					break;
				}
			}
		}
		return d * v;
	}

	static float gamma_small(float k)
	{
		float u;
		float scal;

		while (UNLIKELY((u = dr_rand_uni()) <= 0.f));
		scal = pow(u, 1.f / k);
		return gamma_large(k + 1.f) * scal;
	}


	if (UNLIKELY(k < 1.f)) {
		return gamma_small(k);
	}
	return gamma_large(k);
}

static float
poiss_rnd(float lambda)
{
	static float poiss_rnd_small(float lambda)
	{
		const float lexp = exp(-lambda);
		float p = 1.f;
		uint8_t k = 0U;

		while ((p *= dr_rand_uni()) > lexp) {
			k++;
		}
		return (float)k;
	}

	static float poiss_rnd_ad(float lambda)
	{
		/* Ahrens/Dieter algo */
		const float m = floor(7.f / 8.f * lambda);
		const float x = dr_rand_gamma(m);

		if (LIKELY(x <= lambda)) {
			return m + poiss_rnd(lambda - x);
		} else if ((unsigned int)m - 1U < 1048576U) {
			return binom_rnd((unsigned int)m - 1U, lambda / x);
		}
		return m;
	}

	if (UNLIKELY(lambda < 0.f)) {
		return NAN;
	} else if (UNLIKELY(isinf(lambda))) {
		return INFINITY;
	} else if (LIKELY(lambda < 15.f)) {
		return poiss_rnd_small(lambda);
	}
	return poiss_rnd_ad(lambda);
}


#if !defined NDEBUG
#if 0
static void
dump_layer(const char *pre, const float *x, size_t z)
{
	fputs(pre, stdout);
	putchar(' ');

	if (z > 20U) {
		z = 20U;
	}
	for (size_t i = 0; i < z; i++) {
		printf("%.6g ", x[i]);
	}
	putchar('\n');
	return;
}
#else
static void
dump_layer(const char *pre, const float *x, size_t z)
{
	float minx = INFINITY;
	float maxx = -INFINITY;

	for (size_t i = 0; i < z; i++) {
		if (x[i] < minx) {
			minx = x[i];
		}
		if (x[i] > maxx) {
			maxx = x[i];
		}
	}
	printf("%s (%.6g  %.6g)\n", pre, minx, maxx);
	return;
}
#endif

static size_t
count_layer(const float *x, size_t z)
{
	size_t sum = 0U;

	for (size_t i = 0; i < z; i++) {
		if (x[i] > 0.f) {
			sum++;
		}
	}
	return sum;
}

static float
integ_layer(const float *x, size_t z)
{
	float sum = 0U;

	for (size_t i = 0; i < z; i++) {
		if (x[i] > 0.f) {
			sum += x[i];
		}
	}
	return sum;
}
#else  /* NDEBUG */
static void
dump_layer(const char *UNUSED(pre), const float *UNUSED(x), size_t UNUSED(z))
{
	return;
}
#endif	/* !NDEBUG */


static int
prop_up(float *restrict h, dl_rbm_t m, const float vis[static m->nvis])
{
/* propagate visible units activation upwards to the hidden units (recon) */
	const size_t nvis = m->nvis;
	const size_t nhid = m->nhid;
	const float *w = m->w;
	const float *b = m->hbias;

#define w(i, j)		(w + i * nhid + j)
	for (size_t j = 0; j < nhid; j++) {
		h[j] = b[j] + cblas_sdot(nvis, w(0U, j), nhid, vis, 1);
	}
#undef w
	return 0;
}

static int
expt_hid(float *restrict h, dl_rbm_t m, const float hid[static m->nhid])
{
	const size_t nhid = m->nhid;

	dump_layer("Ha", hid, nhid);

	for (size_t j = 0; j < nhid; j++) {
		h[j] = sigma(hid[j]);
	}
	return 0;
}

static int
smpl_hid(float *restrict h, dl_rbm_t m, const float hid[static m->nhid])
{
/* infer hidden unit states given vis(ible units) */
	const size_t nhid = m->nhid;

	dump_layer("He", hid, nhid);

	for (size_t j = 0; j < nhid; j++) {
		/* just flip a coin */
		h[j] = binom1_rnd(hid[j]);
	}

	dump_layer("Hs", h, nhid);
	return 0;
}

static int
prop_down(float *restrict v, dl_rbm_t m, const float hid[static m->nhid])
{
/* propagate hidden units activation downwards to the visible units */
	const size_t nvis = m->nvis;
	const size_t nhid = m->nhid;
	const float *w = m->w;
	const float *b = m->vbias;

#define w(i, j)		(w + i * nhid + j)
	for (size_t i = 0; i < nvis; i++) {
		v[i] = b[i] + cblas_sdot(nhid, w(i, 0U), 1U, hid, 1U);
	}
#undef w
	return 0;
}

static int
expt_vis(float *restrict v, dl_rbm_t m, const float vis[static m->nvis])
{
	const size_t nvis = m->nvis;
#if defined SALAKHUTDINOV
	float nor = 0.f;
	float N = 0.f;
#endif	/* SALAKHUTDINOV */

	dump_layer("Va", vis, nvis);

#if defined SALAKHUTDINOV
	/* calc N */
	for (size_t i = 0; i < nvis; i++) {
		N += vis[i];
	}
	/* calc \sum exp(v) */
	for (size_t i = 0; i < nvis; i++) {
		nor += v[i] = exp(vis[i]);
	}
	with (const float norm = (float)106 / nor) {
		for (size_t i = 0; i < nvis; i++) {
			v[i] = v[i] * norm;
		}
	}
#elif defined GEHLER
	for (size_t i = 0; i < nvis; i++) {
		v[i] = exp(vis[i]);
	}
#endif	/* impls */
	return 0;
}

static int
smpl_vis(float *restrict v, dl_rbm_t m, const float vis[static m->nvis])
{
/* infer visible unit states given hid(den units) */
	const size_t nvis = m->nvis;

	dump_layer("Ve", vis, nvis);

	/* vis is expected to contain the lambda values */
	for (size_t i = 0; i < nvis; i++) {
		v[i] = poiss_rnd(vis[i]);
	}

	dump_layer("Vs", vis, nvis);
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
#if !defined NDEBUG
	float *hs = calloc(nh, sizeof(*hs));
#endif	/* !NDEBUG */

	/* populate from input */
	popul_ui8(vo, v, nv);

	/* vh gibbs */
	prop_up(ho, m, vo);
	expt_hid(ho, m, ho);
	/* don't sample into ho, use hr instead, we want the activations */
	smpl_hid(hr, m, ho);
#if !defined NDEBUG
	size_t nho = count_layer(hr, nh);
#endif	/* !NDEBUG */

	/* hv gibbs */
	prop_down(vr, m, hr);
	expt_vis(vr, m, vr);
	smpl_vis(vr, m, vr);
	/* vh gibbs */
	prop_up(hr, m, vr);
	expt_hid(hr, m, hr);

#if !defined NDEBUG
	smpl_hid(hs, m, hr);
	size_t nhr = count_layer(hs, nh);
#endif	/* !NDEBUG */

	/* we won't sample the h reconstruction as we want to use the
	 * the activations directly */

#if !defined NDEBUG
	size_t nso = count_layer(vo, nv);
	size_t nsr = count_layer(vr, nv);
	float Nso = integ_layer(vo, nv);
	float Nsr = integ_layer(vr, nv);
	printf("|vo| %zu  |vr| %zu  Nvo %.6g  Nvr %.6g\n", nso, nsr, Nso, Nsr);
	printf("|ho| %zu  |hr| %zu\n", nho, nhr);

	printf("using %.6g (@%p) for learning rate\n", eta, &eta);
#endif	/* !NDEBUG */

	/* bang <v_i h_j> into weights and biasses */
	with (float *w = m->w, *vb = m->vbias, *hb = m->hbias) {
#define w(i, j)		w[i * nh + j]
#if !defined NDEBUG
		float mind;
		float maxd;

		mind = INFINITY;
		maxd = -INFINITY;
#endif	/* !NDEBUG */
		for (size_t i = 0; i < nv; i++) {
			for (size_t j = 0; j < nh; j++) {
				float vho = vo[i] * ho[j];
				float vhr = vr[i] * hr[j];
				float dw = eta * (vho - vhr);

#if !defined NDEBUG
				if (dw < mind) {
					mind = dw;
				}
				if (dw > maxd) {
					maxd = dw;
				}
#endif	/* !NDEBUG */
				w(i, j) += dw;
			}
		}
#if !defined NDEBUG
		printf("dw (%.6g  %.6g)\n", mind, maxd);
		mind = INFINITY;
		maxd = -INFINITY;
#endif	/* !NDEBUG */
#undef w

		/* bias update */
		for (size_t i = 0; i < nv; i++) {
			float dw = eta * (vo[i] - vr[i]);

#if !defined NDEBUG
			if (dw < mind) {
				mind = dw;
			}
			if (dw > maxd) {
				maxd = dw;
			}
#endif	/* !NDEBUG */
			vb[i] += dw;
		}
#if !defined NDEBUG
		printf("dv (%.6g  %.6g)\n", mind, maxd);
		mind = INFINITY;
		maxd = -INFINITY;
#endif	/* !NDEBUG */

		for (size_t j = 0; j < nh; j++) {
			float dw = eta * (ho[j] - hr[j]);

#if !defined NDEBUG
			if (dw < mind) {
				mind = dw;
			}
			if (dw > maxd) {
				maxd = dw;
			}
#endif	/* !NDEBUG */
			hb[j] += dw;
		}
#if !defined NDEBUG
		printf("dh (%.6g  %.6g)\n", mind, maxd);

		dump_layer("h", m->hbias, nh);
		dump_layer("v", m->vbias, nv);
		printf("s(w)\n");
#define w(i, j)		m->w[i * nh + j]
		for (size_t j = 0; j < nh; j++) {
			float min = INFINITY;
			float max = -INFINITY;
			for (size_t i = 0; i < nv; i++) {
				if (w(i, j) > max) {
					max = w(i, j);
				}
				if (w(i, j) < min) {
					min = w(i, j);
				}
			}
			printf("  %zu (%.6g %.6g)\n", j, min, max);
		}
#undef w
#endif	/* !NDEBUG */
	}

	free(vo);
	free(vr);
	free(ho);
	free(hr);
#if !defined NDEBUG
	free(hs);
#endif	/* !NDEBUG */
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

	vo = calloc(nv, sizeof(*vo));
	vr = calloc(nv, sizeof(*vr));
	ho = calloc(nh, sizeof(*ho));

	/* populate from input */
	popul_ui8(vo, v, nv);

	/* vhv gibbs */
	prop_up(ho, m, vo);
	expt_hid(ho, m, ho);
	smpl_hid(ho, m, ho);
	/* hv gibbs */
	prop_down(vr, m, ho);
	expt_vis(vr, m, vr);
	smpl_vis(vr, m, vr);

	for (size_t i = 0; i < nv; i++) {
		uint8_t vi = (uint8_t)(int)vr[i];

		if (UNLIKELY(vi)) {
			printf("%zu\t%u\n", i, (unsigned int)vi);
		}
	}

	free(vo);
	free(vr);
	free(ho);
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

			for (size_t i = 0; i < 200U; i++) {
				train(m, v);
			}
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
