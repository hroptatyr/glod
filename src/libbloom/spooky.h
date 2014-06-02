//
// SpookyHash: a 128-bit noncryptographic hash function
// By Bob Jenkins, public domain
//   Oct 31 2010: alpha, framework + SpookyHash::Mix appears right
//   Oct 31 2011: alpha again, Mix only good to 2^^69 but rest appears right
//   Dec 31 2011: beta, improved Mix, tested it for 2-bit deltas
//   Feb  2 2012: production, same bits as beta
//   Feb  5 2012: adjusted definitions of uint* to be more portable
// 
// Up to 4 bytes/cycle for long messages.  Reasonably fast for short messages.
// All 1 or 2 bit deltas achieve avalanche within 1% bias per output bit.
//
// This was developed for and tested on 64-bit x86-compatible processors.
// It assumes the processor is little-endian.  There is a macro
// controlling whether unaligned reads are allowed (by default they are).
// This should be an equally good hash on big-endian machines, but it will
// compute different results on them than on little-endian machines.
//
// Google's CityHash has similar specs to SpookyHash, and CityHash is faster
// on some platforms.  MD4 and MD5 also have similar specs, but they are orders
// of magnitude slower.  CRCs are two or more times slower, but unlike 
// SpookyHash, they have nice math for combining the CRCs of pieces to form 
// the CRCs of wholes.  There are also cryptographic hashes, but those are even 
// slower than MD5.
//

#include <stddef.h>

#ifdef _MSC_VER
# define INLINE __forceinline
  typedef  unsigned __int64 uint64;
  typedef  unsigned __int32 uint32;
  typedef  unsigned __int16 uint16;
  typedef  unsigned __int8  uint8;
#else
# include <stdint.h>
# define INLINE static inline
  typedef  uint64_t  uint64;
  typedef  uint32_t  uint32;
  typedef  uint16_t  uint16;
  typedef  uint8_t   uint8;
#endif

// number of uint64's in internal state
#define sc_numVars	12U

struct SpookyHash {
    uint64 m_data[2*sc_numVars];   // unhashed data, for partial messages
    uint64 m_state[sc_numVars];  // internal state of the hash
    size_t m_length;             // total length of the input so far
    uint8  m_remainder;          // length of unhashed data stashed in m_data
};

//
// SpookyHash: hash a single message in one call, produce 128-bit output
//
extern void SpookyHash128(
        const void *message,  // message to hash
        size_t length,        // length of message in bytes
        uint64 *hash1,        // in/out: in seed 1, out hash value 1
        uint64 *hash2);       // in/out: in seed 2, out hash value 2

//
// SpookyHash64: hash a single message in one call, return 64-bit output
//
INLINE uint64 SpookyHash64(
        const void *message,  // message to hash
        size_t length,        // length of message in bytes
        uint64 seed)          // seed
{
	uint64 hash1 = seed;
	SpookyHash128(message, length, &hash1, &seed);
	return hash1;
}

//
// SpookyHash32: hash a single message in one call, produce 32-bit output
//
INLINE uint32 SpookyHash32(
        const void *message,  // message to hash
        size_t length,        // length of message in bytes
        uint32 seed)          // seed
{
	uint64 hash1 = seed, hash2 = seed;
	SpookyHash128(message, length, &hash1, &hash2);
	return (uint32)hash1;
}


