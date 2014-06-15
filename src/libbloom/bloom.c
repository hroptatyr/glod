#include <math.h>
#include <iso646.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include "bloom.h"
#include "murmur.h"
#include "spooky.h"

#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */

/**
 * We use a magic header to identify the bloom filters.
 */
struct bloom_filter_header {
	uint32_t magic;     // Magic 4 bytes
	uint32_t k_num;     // K_num value
	uint64_t count;     // Count of items
	char __buf[496];    // Pad out to 512 bytes
} __attribute__((packed));


/*
 * Static definitions
 */
static const uint32_t MAGIC_HEADER = 0xCB1005DD;  // Vaguely like CBLOOMDD

static const double log2sq = 0.6931471805599453 * 0.6931471805599453;

// Computes our hashes
static void
bf_compute_hashes(uint64_t *restrict hashes, unsigned int k_num, char *key)
{
	/**
	 * We use the results of
	 * 'Less Hashing, Same Performance: Building a Better Bloom Filter'
	 * http://www.eecs.harvard.edu/~kirsch/pubs/bbbf/esa06.pdf, to use
	 * g_i(x) = h1(u) + i * h2(u) mod m'
	 *
	 * This allows us to only use 2 hash functions h1, and h2 but generate
	 * k unique hashes using linear combinations. This is a vast speedup
	 * over our previous technique of 4 hashes, that used double hashing.
	 *
	 */

	// Get the length of the key
	size_t len = strlen(key);

	// Compute the first hash
	uint64_t out[2];
	MurmurHash3_x64_128(key, len, 0, out);

	// Copy these out
	hashes[0] = out[0];  // Upper 64bits of murmur
	hashes[1] = out[1];  // Lower 64bits of murmur

	// Compute the second hash
	uint64_t *hash1 = out + 0;
	uint64_t *hash2 = out + 1;
	spooky_hash128(key, len, hash1, hash2);

	// Copy these out
	hashes[2] = out[0];   // Use the upper 64bits of Spooky
	hashes[3] = out[1];   // Use the lower 64bits of Spooky

	// Compute an arbitrary k_num using a linear combination
	// Add a mod by the largest 64bit prime. This only reduces the
	// number of addressable bits by 54 but should make the hashes
	// a bit better.
	for (size_t i = 4U; i < k_num; i++) {
		hashes[i] = hashes[1] +
			((i * hashes[3]) % 18446744073709551557ULL);
	}
	return;
}

/**
 * Internal bf_contains method.
 * @arg filter The filter
 * @arg key The key to check
 * @arg hashes Contains at least K num hashes
 * @return 0 if not contained, 1 if contained.
 */
static int
bf_internal_contains(bloom_filter *filter, const uint64_t *hashes)
{
	const uint64_t m = filter->offset;

	for (size_t i = 0; i < filter->header->k_num; i++) {
		/* get hash value */
		const uint64_t h = hashes[i];
		/* and partition offset */
		const uint64_t offset =
			BITS_PER_UF8 * sizeof(*filter->header) + i * m;
		/* Compute the bit offset */
		const idx_t bit = offset + (h % m);

		if (!bitmap_getbit(filter->map, bit)) {
			return 0;
		}
	}
	return 1;
}


/* public API */
/**
 * Initialises filter internals using a given bitmap and k-value.
 * @arg map A bloom_bitmap pointer.
 * @arg k_num If non-zero set the number of hash functions to use.
 * @return 0 for success. Negative for error.
 */
int
bf_init(bloom_bitmap *map, unsigned int k_num)
{
	struct bloom_filter_header h = {
		MAGIC_HEADER,
		k_num,
		0U,
	};

	/* check values for k_num and the size of the map */
	if (map == NULL || k_num < 1) {
		errno = EINVAL;
		return -1;
	} else if (map->size < sizeof(h)) {
		errno = ENOMEM;
		return -1;
	}

	/* set header in map */
	memcpy(map->mmap, &h, sizeof(h));

	/* Since this is a new filter, force a flush of
	 * the headers. This mainly affects bitmaps that
	 * are in the PERSIST mode. Since no flush happens
	 * until the first key is set, it can cause filters
	 * to be created that have no headers, and thus cannot
	 * be loaded. */
	bitmap_flush(map);
	return 0;
}

/**
 * Creates a new bloom filter using a given bitmap and k-value.
 * @arg map A bloom_bitmap pointer.
 * @arg filter The filter to setup
 * @return 0 for success. Negative for error.
 */
int
bf_from_bitmap(bloom_bitmap *map, bloom_filter *filter)
{
	// Check our args
	if (map == NULL) {
		return -EINVAL;
	} else if (map->size < sizeof(*filter->header)) {
		return -ENOMEM;
	}

	// Setup the pointers
	filter->map = map;
	filter->header = (void*)map->mmap;

	// Get the bitmap size
	filter->bitmap_size = (map->size - sizeof(*filter->header)) * 8U;

	/* check header */
	if (filter->header->magic != MAGIC_HEADER) {
		errno = EBADMSG;
		return -1;
	}

	// Setup the offset
	filter->offset = filter->bitmap_size / filter->header->k_num;

	// Done, return
	return 0;
}

/**
 * Adds a new key to the bloom filter.
 * @arg filter The filter to add to
 * @arg key The key to add
 * @returns 1 if the key was added, 0 if present. Negative on failure.
 */
int bf_add(bloom_filter *filter, char* key)
{
	// Allocate the hash space
	uint64_t hashes[filter->header->k_num];
	int rc;

	// Compute the hashes
	bf_compute_hashes(hashes, countof(hashes), key);

	// Check if the item exists
	if ((rc = bf_internal_contains(filter, hashes)) == 1) {
		/* Key already present, do not add. */
		return 0;
	}

	const uint64_t m = filter->offset;
	for (size_t i = 0U; i < filter->header->k_num; i++) {
		const uint64_t h = hashes[i];
		/* get the partition offset */
		const uint64_t offset =
			BITS_PER_UF8 * sizeof(*filter->header) + i * m;
		/* compute actual bit */
		const idx_t bit = offset + (h % m);

		/* set the bit */
		bitmap_setbit(filter->map, bit);
	}

	filter->header->count++;
	return 1;
}

/**
 * Checks the filter for a key
 * @arg filter The filter to check
 * @arg key The key to check
 * @returns 1 if present, 0 if not present, negative on error.
 */
int bf_contains(bloom_filter *filter, char* key)
{
	// Allocate the hash space
	uint64_t hashes[filter->header->k_num];

	// Compute the hashes
	bf_compute_hashes(hashes, countof(hashes), key);

	// Use the internal contains method
	return bf_internal_contains(filter, hashes);
}

/**
 * Returns the size of the bloom filter in item count
 */
uint64_t bf_size(bloom_filter *filter)
{
	// Read it from the file header directly
	return filter->header->count;
}

/**
 * Returns the number of hash functions used. */
unsigned int bf_k_num(bloom_filter *filter)
{
	return filter->header->k_num;
}


/**
 * Flushes the filter, and updates the metadata.
 * @return 0 on success, negative on failure.
 */
int bf_flush(bloom_filter *filter)
{
	// Flush the bitmap if we have one
	if (filter == NULL || filter->map == NULL) {
		return -1;
	}
	return bitmap_flush(filter->map);
}

/**
 * Flushes and closes the filter. Closes the underlying bitmap,
 * but does not free it.
 * @return 0 on success, negative on failure.
 */
int bf_close(bloom_filter *filter)
{
	// Make sure we have a filter
	if (filter == NULL || filter->map == NULL) {
		return -1;
	}

	// Flush first
	bf_flush(filter);

	// Clean up the map
	bitmap_close(filter->map);
	filter->map = NULL;

	// Clear all the fields
	filter->header = NULL;
	filter->offset = 0;
	filter->bitmap_size = 0;
	return 0;
}

/*
 * Utility methods
 */

/*
 * Expects capacity and probability to be set,
 * and sets the bytes and k_num that should be used.
 * @return 0 on success, negative on error.
 */
int bf_params_for_capacity(bloom_filter_params *params)
{
	// Sets the required size
	int res = bf_size_for_capacity_prob(params);

	if (res != 0) {
		return res;
	}

	// Sets the ideal k
	res = bf_ideal_k_num(params);
	if (res != 0) {
		return res;
	}
	// Adjust for the header size
	params->bytes += sizeof(struct bloom_filter_header);
	return 0;
}

/*
 * Expects capacity and probability to be set, computes the
 * minimum byte size required.
 * @return 0 on success, negative on error.
 */
int bf_size_for_capacity_prob(bloom_filter_params *params)
{
	uint64_t capacity = params->capacity;
	double fp_prob = params->fp_probability;

	if (capacity == 0 || fp_prob == 0.0) {
		return -1;
	}
	double bits = -(capacity * log(fp_prob) / log2sq);
	uint64_t whole_bits = ceil(bits);
	params->bytes = ceil(whole_bits / 8.0);
	return 0;
}

/*
 * Expects capacity and size to be set, computes the best
 * false positive probability given an ideal k.
 * @return 0 on success, negative on error.
 */
int bf_fp_probability_for_capacity_size(bloom_filter_params *params)
{
	uint64_t bits = params->bytes * 8;
	uint64_t capacity = params->capacity;
	double fp_prob;

	if (bits == 0 || capacity == 0) {
		return -1;
	}
	fp_prob = pow(M_E, -( (double)bits / (double)capacity) * log2sq);
	params->fp_probability = fp_prob;
	return 0;
}

/*
 * Expects bytes and probability to be set,
 * computes the expected capacity.
 * @return 0 on success, negative on error.
 */
int bf_capacity_for_size_prob(bloom_filter_params *params)
{
	uint64_t bits = params->bytes * 8;
	double prob = params->fp_probability;
	uint64_t capacity;

	if (bits == 0 || prob == 0.0) {
		return -1;
	}
	capacity = (uint64_t)-(bits / log(prob) * log2sq);
	params->capacity = capacity;
	return 0;
}

/*
 * Expects bytes and capacity to be set,
 * computes the ideal k num.
 * @return 0 on success, negative on error.
 */
int bf_ideal_k_num(bloom_filter_params *params)
{
	uint64_t bits = params->bytes * 8;
	uint64_t capacity = params->capacity;
	uint32_t ideal_k;

	if (bits == 0 || capacity == 0) {
		return -1;
	}
	ideal_k = (uint32_t)round(log(2) * bits / capacity);
	params->k_num = ideal_k;
	return 0;
}

/* bloom.c ends here */
