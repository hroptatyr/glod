#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include "bitmap.h"


static __attribute__((const)) size_t
__get_pgsz(void)
{
	static size_t pgsz;

	if (pgsz) {
		return pgsz;
	}
	return pgsz = sysconf(_SC_PAGESIZE);
}

static void*
alloc_dirty_page_bitmap(size_t len)
{
	const size_t _pgsz = __get_pgsz();
	const size_t z = (len + _pgsz - 1) / _pgsz / BITS_PER_UF8;

	return calloc(sizeof(uint_fast8_t), z);
}

/**
 * Flushes out a single page that is dirty
 */
static int
flush_page(bloom_bitmap *map, idx_t page, size_t size, idx_t max_page)
{
	const size_t _pgsz = __get_pgsz();
	idx_t off = page * _pgsz;
	const int fd = map->fileno;
	/* still to write */
	ssize_t twr;

	if (page < max_page || !(size % _pgsz)) {
		twr = _pgsz;
	} else {
		/* last page may need extra treatment */
		twr = size % _pgsz;
	}

	for (ssize_t nwr, tot = 0; tot < twr; tot += nwr) {
		nwr = pwrite(fd, map->mmap + off + tot, twr - tot, off + tot);

		if (nwr < 0 && errno != EINTR) {
			break;
		} else if (nwr < 0) {
			nwr = 0;
		}
	}
	return 0;
}

/**
 * Flushes all the dirty pages of the bitmap. We just
 * scan the dirty_pages bitfield and flush every 4K
 * block that is considered dirty. As a bit of a jank hack,
 * we always flush the first block, since it contains headers,
 * and is not reliably marked as dirty.
 */
static int
flush_dirty_pages(bloom_bitmap *map)
{
/**
 * The dirty page bitmap is a problematic
 * shared data structure since reads and writes are
 * byte aligned. This means when we set a single bit,
 * we are doing a read/update/write on the whole byte.
 * Because of this, concurrent updates are not safe.
 * To solve this, we allocate a new fresh bitmap, and
 * swap the old one out. This allows the other threads
 * to mark bits as dirty, while we go through and flush.
 * At the end, we free our old version. */
	const size_t _pgsz = __get_pgsz();
	const size_t mpsz = map->size;
	const size_t npg = (mpsz - 1U) / _pgsz;
	uint_fast8_t *old_dirty;
	int rc = 0;

	if (mpsz == 0U) {
		return 0;
	} else if ((old_dirty = map->dirty_pages) == NULL) {
		return 0;
	}
	/* swap out dirty_pages slot */
	map->dirty_pages = alloc_dirty_page_bitmap(mpsz);

	/* first page gets always flushed */
	if (flush_page(map, 0U, mpsz, npg) < 0) {
		rc = -1;
		goto LEAVE;
	}
	for (size_t i = 1U; i <= npg; i++) {
		if (_uf8_getbit(old_dirty, i)) {
			/* flush the page */
			if (flush_page(map, i, mpsz, npg) < 0) {
				rc = -1;
				break;
			}
		}
	}
LEAVE:
	/* clean up */
	free(old_dirty);
	return rc;
}

/*
 * Populates a buffer with the contents of a file
 */
static int
fill_buffer(int fileno, unsigned char *restrict buf, size_t len)
{
	for (ssize_t tot = 0, nrd; (size_t)tot < len; tot += nrd) {
		nrd = pread(fileno, buf + tot, len - tot, tot);

		if (nrd == 0) {
			break;
		} else if (nrd < 0 && errno != EINTR) {
			return -1;
		} else if (nrd < 0) {
			nrd = 0;
		}
	}
	return 0;
}


/* public API here */
/**
 * Returns a bloom_bitmap pointer from a file handle
 * that is already opened with read/write privileges.
 * @arg fileno The fileno
 * @arg len The length of the bitmap in bytes.
 * @arg mode The mode to use for the bitmap.
 * @arg map The output map. Will be initialized.
 * @return 0 on success. Negative on error.
 */
int
bitmap_from_file(int fileno, size_t len, bitmap_mode mode, bloom_bitmap *map)
{
	bool new_bmp = false;
	void *m = NULL;
	void *dirty = NULL;
	int rc = 0;
	int flags;
	int prot;
	int fd;

	/* Hack for old kernels and bad length checking */
	if (len == 0) {
		return -1;
	}

	/* check for and clear NEW_BITMAP from the mode */
	if (mode & NEW_BITMAP) {
		new_bmp = true;
		mode = (bitmap_mode)(mode & ~NEW_BITMAP);
	}

	/* handle modes */
	switch (mode) {
		int tmp;
	case SHARED:
	case PERSISTENT:
		if ((tmp = fcntl(fileno, F_GETFL)) < 0) {
			return -1;
		} else if ((fd = dup(fileno)) < 0) {
			return -1;
		}
		switch (mode) {
		case SHARED:
			flags = MAP_SHARED;
			prot = PROT_READ | ((tmp & O_RDWR) ? PROT_WRITE : 0);
			break;
		case PERSISTENT:
			flags = MAP_ANON | MAP_PRIVATE;
			prot = PROT_READ | PROT_WRITE;
			break;
		}
		break;

	case ANONYMOUS:
		flags = MAP_ANON | MAP_PRIVATE;
		prot = PROT_READ | PROT_WRITE;
		fd = -1;
		break;

	default:
		errno = EINVAL;
		return -1;
	}

	if ((m = mmap(NULL, len, prot, flags, fd, 0)) == MAP_FAILED) {
		m = NULL;
		goto fail_out;
	}

	if (mode == SHARED) {
		/* provide some advise on how the memory will be used */
		rc += madvise(m, len, MADV_WILLNEED);
		rc += madvise(m, len, MADV_RANDOM);
	}

	if (mode == PERSISTENT) {
		/* For the PERSISTENT case, we manually track
		 * dirty pages, and need a bit field for this */
		if ((dirty = alloc_dirty_page_bitmap(len)) == NULL) {
			goto fail_out;
		}

		/* For existing bitmaps we need to read in the data
		 * since we cannot use the kernel to fault it in */
		if (!new_bmp && (rc = fill_buffer(fd, m, len))) {
			goto fail_out;
		}
	}

	/* fill in user provided MAP param */
	map->mode = mode;
	map->fileno = fd;
	map->size = len;
	map->mmap = m;
	map->dirty_pages = dirty;
	return 0;

fail_out:;
	int eno = errno;

	if (dirty != NULL) {
		free(dirty);
	}
	if (m != NULL) {
		munmap(m, len);
	}
	if (fd >= 0) {
		close(fd);
	}
	errno = eno;
	return -1;
}

/**
 * Flushes the bitmap back to disk. This is
 * a syncronous operation. It is a no-op for
 * ANONYMOUS bitmaps.
 * @arg map The bitmap
 * @returns 0 on success, negative failure.
 */
int bitmap_flush(bloom_bitmap *map)
{
	int rc;

	/* Return if there is no map provided */
	if (map == NULL) {
		errno = EINVAL;
		return -1;
	} else if (map->mmap == NULL) {
		/* trivial */
		return 0;
	}

	switch (map->mode) {
	default:
	case ANONYMOUS:
		return 0;
	case SHARED:
		/* use an msync to let the kernel deal with it */
		rc = msync(map->mmap, map->size, MS_SYNC);
		break;
	case PERSISTENT:
		rc = flush_dirty_pages(map);
		break;
	}

	/* in case there's file backing */
	rc += fsync(map->fileno);
	return rc;
}

/**
 * Closes and flushes the bitmap. This is
 * a syncronous operation. It is a no-op for
 * ANONYMOUS bitmaps. The caller should free()
 * the structure after.
 * @arg map The bitmap
 * @returns 0 on success, negative on failure.
 */
int bitmap_close(bloom_bitmap *map)
{
	int rc = 0;

	if (map == NULL) {
		errno = EINVAL;
		return -1;
	}

	/* clean up the main map first */
	if (map->mmap != NULL) {
		rc += bitmap_flush(map);

		/* definitely try and unmap the file */
		rc += munmap(map->mmap, map->size);

		map->mmap = NULL;
	}

	/* close the file descriptor if file backed */
	if (map->mode != ANONYMOUS) {
		rc += close(map->fileno);
		map->fileno = -1;
	}

	/* deal with the dirty bitfield if any */
	if (map->dirty_pages) {
		free(map->dirty_pages);
		map->dirty_pages = NULL;
	}
	return rc;
}

void
bitmap_set_dirty(bloom_bitmap *map, idx_t i)
{
	const size_t pgsz = __get_pgsz();
	idx_t page = (i / BITS_PER_UF8) / pgsz;

	_uf8_setbit(map->dirty_pages, page);
	return;
}

/* bitmap.c ends here */
