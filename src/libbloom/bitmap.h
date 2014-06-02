#ifndef BLOOM_BITMAP_H
#define BLOOM_BITMAP_H
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

typedef uint64_t idx_t;

typedef enum {
	SHARED      = 1, // MAP_SHARED mmap used, file backed.
	PERSISTENT  = 2, // MAP_ANONYMOUS used, file backed.
	ANONYMOUS   = 4, // MAP_ANONYMOUS mmap used. No file backing.
	NEW_BITMAP  = 8  // File contents not read. Used with PERSISTENT
} bitmap_mode;

typedef struct {
	bitmap_mode mode;
	int fileno;      // Underlying fileno
	size_t size;     // Size of bitmap in bytes
	uint_fast8_t *mmap; // Starting address of the bitmap region
	uint_fast8_t *dirty_pages; // Used for the PERSISTENT mode.
} bloom_bitmap;

#define BITS_PER_UF8	(sizeof(uint_fast8_t) * 8U)

/**
 * Returns a bloom_bitmap pointer from a file handle
 * that is already opened with read/write privileges.
 * @arg fileno The fileno
 * @arg len The length of the bitmap in bytes.
 * @arg mode The mode to use for the bitmap.
 * @arg map The output map. Will be initialized.
 * @return 0 on success. Negative on error.
 */
extern int
bitmap_from_file(int fd, size_t len, bitmap_mode mode, bloom_bitmap *map);

/**
 * Flushes the bitmap back to disk. This is
 * a syncronous operation. It is a no-op for
 * ANONYMOUS bitmaps.
 * @arg map The bitmap
 * @returns 0 on success, negative failure.
 */
extern int bitmap_flush(bloom_bitmap *map);

/**
 * * Closes and flushes the bitmap. This is
 * a syncronous operation. It is a no-op for
 * ANONYMOUS bitmaps. The caller should free()
 * the structure after.
 * @arg map The bitmap
 * @returns 0 on success, negative on failure.
 */
extern int bitmap_close(bloom_bitmap *map);

/* helper for dirty pages */
extern void bitmap_set_dirty(bloom_bitmap *map, idx_t i);


static inline __attribute__((pure, always_inline)) unsigned int
_uf8_getbit(uint_fast8_t *restrict arr, idx_t i)
{
	uint_fast8_t byte = arr[i / BITS_PER_UF8];
	uint_fast8_t boff = i % BITS_PER_UF8;
	return (byte >> boff) & 0x1U;
}

static inline __attribute__((always_inline)) void
_uf8_setbit(uint_fast8_t *restrict arr, idx_t i)
{
	uint_fast8_t byte = arr[i / BITS_PER_UF8];
	uint_fast8_t boff = i % BITS_PER_UF8;
	arr[i / BITS_PER_UF8] = (uint_fast8_t)(byte | (1U << boff));
	return;
}

/**
 * Returns the value of the bit at index idx for the
 * bloom_bitmap map
 */
static inline unsigned int bitmap_getbit(bloom_bitmap *map, idx_t i)
{
	return _uf8_getbit(map->mmap, i);
}

/*
 * Used to set a bit in the bitmap, and as a side affect,
 * mark the page as dirty if we are in the PERSISTENT mode
 */
static inline void bitmap_setbit(bloom_bitmap *map, idx_t i)
{
	_uf8_setbit(map->mmap, i);

	// Check if we need to dirty the page
	if (map->mode == PERSISTENT) {
		bitmap_set_dirty(map, i);
	}
	return;
}

#endif
