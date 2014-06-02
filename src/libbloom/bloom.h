#ifndef BLOOM_BLOOM_H
#define BLOOM_BLOOM_H
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <errno.h>
#include "bitmap.h"

/*
 * This is the struct we use to represent a bloom filter.
 */
typedef struct {
	// Pointer to the header in the bitmap region
	struct bloom_filter_header *header;
	// Underlying bitmap
	bloom_bitmap *map;
	// The offset size between hash regions
	uint64_t offset;
	// The size of the bitmap to use, minus buffers
	uint64_t bitmap_size;
} bloom_filter;

/*
 * Structure used to store the parameter information
 * for configuring bloom filters.
 */
typedef struct {
	uint64_t bytes;
	uint32_t k_num;
	uint64_t capacity;
	double   fp_probability;
} bloom_filter_params;


/**
 * Initialises filter internals using a given bitmap and k-value.
 * @arg map A bloom_bitmap pointer.
 * @arg k_num If non-zero set the number of hash functions to use.
 * @return 0 for success. Negative for error.
 */
extern int bf_init(bloom_bitmap *map, unsigned int k_num);

/**
 * Creates a new bloom filter using a given (initialised) bitmap.
 * @arg map A bloom_bitmap pointer.
 * @arg filter The filter to setup
 * @return 0 for success. Negative for error.
 */
extern int
bf_from_bitmap(bloom_bitmap *map, bloom_filter *filt);

/**
 * Adds a new key to the bloom filter.
 * @arg filter The filter to add to
 * @arg key The key to add
 * @returns 1 if the key was added, 0 if present. Negative on failure.
 */
extern int bf_add(bloom_filter *filter, char* key);

/**
 * Checks the filter for a key
 * @arg filter The filter to check
 * @arg key The key to check
 * @returns 1 if present, 0 if not present, negative on error.
 */
extern int bf_contains(bloom_filter *filter, char* key);

/**
 * Returns the size of the bloom filter in item count
 */
extern size_t bf_size(bloom_filter *filter);

/**
 * Flushes the filter, and updates the metadata.
 * @return 0 on success, negative on failure.
 */
extern int bf_flush(bloom_filter *filter);

/**
 * Flushes and closes the filter. Closes the underlying bitmap,
 * but does not free it.
 * @return 0 on success, negative on failure.
 */
extern int bf_close(bloom_filter *filter);

/*
 * Utility methods for computing parameters
 */

/*
 * Expects capacity and probability to be set,
 * and sets the bytes and k_num that should be used.
 * This byte size accounts for the headers we need.
 * @return 0 on success, negative on error.
 */
extern int bf_params_for_capacity(bloom_filter_params *params);

/*
 * Expects capacity and probability to be set, computes the
 * minimum byte size required. Does not include header size.
 * @return 0 on success, negative on error.
 */
extern int bf_size_for_capacity_prob(bloom_filter_params *params);

/*
 * Expects capacity and size to be set, computes the best
 * false positive probability given an ideal k.
 * @return 0 on success, negative on error.
 */
extern int bf_fp_probability_for_capacity_size(bloom_filter_params *params);

/*
 * Expects bytes and probability to be set,
 * computes the expected capacity.
 * @return 0 on success, negative on error.
 */
extern int bf_capacity_for_size_prob(bloom_filter_params *params);

/*
 * Expects bytes and capacity to be set,
 * computes the ideal k num.
 * @return 0 on success, negative on error.
 */
extern int bf_ideal_k_num(bloom_filter_params *params);

#endif

