// Spooky Hash
// A 128-bit noncryptographic hash, for checksums and table lookup
// By Bob Jenkins.  Public domain.
//   Oct 31 2010: published framework, disclaimer ShortHash isn't right
//   Nov 7 2010: disabled ShortHash
//   Oct 31 2011: replace End, ShortMix, ShortEnd, enable ShortHash again

#include <memory.h>
#include "spooky.h"

#define ALLOW_UNALIGNED_READS 1

//
// left rotate a 64-bit value by k bytes
//
static inline uint64 Rot64(uint64 x, int k)
{
        return (x << k) | (x >> (64 - k));
}

//
// This is used if the input is 96 bytes long or longer.
//
// The internal state is fully overwritten every 96 bytes.
// Every input bit appears to cause at least 128 bits of entropy
// before 96 other bytes are combined, when run forward or backward
//   For every input bit,
//   Two inputs differing in just that input bit
//   Where "differ" means xor or subtraction
//   And the base value is random
//   When run forward or backwards one Mix
// I tried 3 pairs of each; they all differed by at least 212 bits.
//
static inline void Mix(
        const uint64 *data, 
        uint64 *s0, uint64 *s1, uint64 *s2, uint64 *s3,
        uint64 *s4, uint64 *s5, uint64 *s6, uint64 *s7,
        uint64 *s8, uint64 *s9, uint64 *s10,uint64 *s11)
{
	*s0 += data[0];    *s2 ^= *s10;    *s11 ^= *s0;    *s0 = Rot64(*s0,11);    *s11 += *s1;
	*s1 += data[1];    *s3 ^= *s11;    *s0 ^= *s1;    *s1 = Rot64(*s1,32);    *s0 += *s2;
	*s2 += data[2];    *s4 ^= *s0;    *s1 ^= *s2;    *s2 = Rot64(*s2,43);    *s1 += *s3;
	*s3 += data[3];    *s5 ^= *s1;    *s2 ^= *s3;    *s3 = Rot64(*s3,31);    *s2 += *s4;
	*s4 += data[4];    *s6 ^= *s2;    *s3 ^= *s4;    *s4 = Rot64(*s4,17);    *s3 += *s5;
	*s5 += data[5];    *s7 ^= *s3;    *s4 ^= *s5;    *s5 = Rot64(*s5,28);    *s4 += *s6;
	*s6 += data[6];    *s8 ^= *s4;    *s5 ^= *s6;    *s6 = Rot64(*s6,39);    *s5 += *s7;
	*s7 += data[7];    *s9 ^= *s5;    *s6 ^= *s7;    *s7 = Rot64(*s7,57);    *s6 += *s8;
	*s8 += data[8];    *s10 ^= *s6;    *s7 ^= *s8;    *s8 = Rot64(*s8,55);    *s7 += *s9;
	*s9 += data[9];    *s11 ^= *s7;    *s8 ^= *s9;    *s9 = Rot64(*s9,54);    *s8 += *s10;
	*s10 += data[10];    *s0 ^= *s8;    *s9 ^= *s10;    *s10 = Rot64(*s10,22);    *s9 += *s11;
	*s11 += data[11];    *s1 ^= *s9;    *s10 ^= *s11;    *s11 = Rot64(*s11,46);    *s10 += *s0;
}

//
// Mix all 12 inputs together so that h0, h1 are a hash of them all.
//
// For two inputs differing in just the input bits
// Where "differ" means xor or subtraction
// And the base value is random, or a counting value starting at that bit
// The final result will have each bit of h0, h1 flip
// For every input bit,
// with probability 50 +- .3%
// For every pair of input bits,
// with probability 50 +- 3%
//
// This does not rely on the last Mix() call having already mixed some.
// Two iterations was almost good enough for a 64-bit result, but a
// 128-bit result is reported, so End() does three iterations.
//
static inline void EndPartial(
        uint64 *h0, uint64 *h1, uint64 *h2, uint64 *h3,
        uint64 *h4, uint64 *h5, uint64 *h6, uint64 *h7, 
        uint64 *h8, uint64 *h9, uint64 *h10,uint64 *h11)
{
        *h11+= *h1;    *h2 ^= *h11;   *h1 = Rot64(*h1,44);
	*h0 += *h2;    *h3 ^= *h0;    *h2 = Rot64(*h2,15);
	*h1 += *h3;    *h4 ^= *h1;    *h3 = Rot64(*h3,34);
	*h2 += *h4;    *h5 ^= *h2;    *h4 = Rot64(*h4,21);
	*h3 += *h5;    *h6 ^= *h3;    *h5 = Rot64(*h5,38);
	*h4 += *h6;    *h7 ^= *h4;    *h6 = Rot64(*h6,33);
	*h5 += *h7;    *h8 ^= *h5;    *h7 = Rot64(*h7,10);
	*h6 += *h8;    *h9 ^= *h6;    *h8 = Rot64(*h8,13);
	*h7 += *h9;    *h10^= *h7;    *h9 = Rot64(*h9,38);
	*h8 += *h10;   *h11^= *h8;    *h10= Rot64(*h10,53);
	*h9 += *h11;   *h0 ^= *h9;    *h11= Rot64(*h11,42);
	*h10+= *h0;    *h1 ^= *h10;   *h0 = Rot64(*h0,54);
}

static inline void End(
        uint64 *h0, uint64 *h1, uint64 *h2, uint64 *h3,
        uint64 *h4, uint64 *h5, uint64 *h6, uint64 *h7, 
        uint64 *h8, uint64 *h9, uint64 *h10,uint64 *h11)
{
        EndPartial(h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11);
        EndPartial(h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11);
        EndPartial(h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11);
}

//
// The goal is for each bit of the input to expand into 128 bits of 
//   apparent entropy before it is fully overwritten.
// n trials both set and cleared at least m bits of h0 h1 h2 h3
//   n: 2   m: 29
//   n: 3   m: 46
//   n: 4   m: 57
//   n: 5   m: 107
//   n: 6   m: 146
//   n: 7   m: 152
// when run forwards or backwards
// for all 1-bit and 2-bit diffs
// with diffs defined by either xor or subtraction
// with a base of all zeros plus a counter, or plus another bit, or random
//
static inline void ShortMix(uint64 *h0, uint64 *h1, uint64 *h2, uint64 *h3)
{
        *h2 = Rot64(*h2,50);  *h2 += *h3;  *h0 ^= *h2;
        *h3 = Rot64(*h3,52);  *h3 += *h0;  *h1 ^= *h3;
        *h0 = Rot64(*h0,30);  *h0 += *h1;  *h2 ^= *h0;
        *h1 = Rot64(*h1,41);  *h1 += *h2;  *h3 ^= *h1;
        *h2 = Rot64(*h2,54);  *h2 += *h3;  *h0 ^= *h2;
        *h3 = Rot64(*h3,48);  *h3 += *h0;  *h1 ^= *h3;
        *h0 = Rot64(*h0,38);  *h0 += *h1;  *h2 ^= *h0;
        *h1 = Rot64(*h1,37);  *h1 += *h2;  *h3 ^= *h1;
        *h2 = Rot64(*h2,62);  *h2 += *h3;  *h0 ^= *h2;
        *h3 = Rot64(*h3,34);  *h3 += *h0;  *h1 ^= *h3;
        *h0 = Rot64(*h0,5);   *h0 += *h1;  *h2 ^= *h0;
        *h1 = Rot64(*h1,36);  *h1 += *h2;  *h3 ^= *h1;
}

//
// Mix all 4 inputs together so that h0, h1 are a hash of them all.
//
// For two inputs differing in just the input bits
// Where "differ" means xor or subtraction
// And the base value is random, or a counting value starting at that bit
// The final result will have each bit of h0, h1 flip
// For every input bit,
// with probability 50 +- .3% (it is probably better than that)
// For every pair of input bits,
// with probability 50 +- .75% (the worst case is approximately that)
//
static inline void ShortEnd(uint64 *h0, uint64 *h1, uint64 *h2, uint64 *h3)
{
        *h3 ^= *h2;  *h2 = Rot64(*h2,15);  *h3 += *h2;
        *h0 ^= *h3;  *h3 = Rot64(*h3,52);  *h0 += *h3;
        *h1 ^= *h0;  *h0 = Rot64(*h0,26);  *h1 += *h0;
        *h2 ^= *h1;  *h1 = Rot64(*h1,51);  *h2 += *h1;
        *h3 ^= *h2;  *h2 = Rot64(*h2,28);  *h3 += *h2;
        *h0 ^= *h3;  *h3 = Rot64(*h3,9);   *h0 += *h3;
        *h1 ^= *h0;  *h0 = Rot64(*h0,47);  *h1 += *h0;
        *h2 ^= *h1;  *h1 = Rot64(*h1,54);  *h2 += *h1;
        *h3 ^= *h2;  *h2 = Rot64(*h2,32);  *h3 += *h2;
        *h0 ^= *h3;  *h3 = Rot64(*h3,25);  *h0 += *h3;
        *h1 ^= *h0;  *h0 = Rot64(*h0,63);  *h1 += *h0;
}

//
// Short is used for messages under 192 bytes in length
// Short has a low startup cost, the normal mode is good for long
// keys, the cost crossover is at about 192 bytes.  The two modes were
// held to the same quality bar.
// 
static void Short(
        const void *message,
        size_t length,
        uint64 *hash1,
        uint64 *hash2);


// size of the internal state
#define sc_blockSize	(sc_numVars*8U)

// size of buffer of unhashed data, in bytes
static const size_t sc_bufSize = 2*sc_blockSize;

//
// sc_const: a constant which:
//  * is not zero
//  * is odd
//  * is a not-very-regular mix of 1's and 0's
//  * does not need any other special mathematical properties
//
static const uint64 sc_const = 0xdeadbeefdeadbeefLL;

//
// short hash ... it could be used on any message, 
// but it's used by Spooky just for short messages.
//
static void Short(
    const void *message,
    size_t length,
    uint64 *hash1,
    uint64 *hash2)
{
    uint64 buf[sc_numVars];
    union 
    { 
        const uint8 *p8; 
        uint32 *p32;
        uint64 *p64; 
        size_t i; 
    } u;

    u.p8 = (const uint8 *)message;
    
    if (!ALLOW_UNALIGNED_READS && (u.i & 0x7))
    {
        memcpy(buf, message, length);
        u.p64 = buf;
    }

    size_t remainder = length%32;
    uint64 a=*hash1;
    uint64 b=*hash2;
    uint64 c=sc_const;
    uint64 d=sc_const;

    if (length > 15)
    {
        const uint64 *end = u.p64 + (length/32)*4;
        
        // handle all complete sets of 32 bytes
        for (; u.p64 < end; u.p64 += 4)
        {
            c += u.p64[0];
            d += u.p64[1];
            ShortMix(&a,&b,&c,&d);
            a += u.p64[2];
            b += u.p64[3];
        }
        
        //Handle the case of 16+ remaining bytes.
        if (remainder >= 16)
        {
            c += u.p64[0];
            d += u.p64[1];
            ShortMix(&a,&b,&c,&d);
            u.p64 += 2;
            remainder -= 16;
        }
    }
    
    // Handle the last 0..15 bytes, and its length
    d = ((uint64)length) << 56;
    switch (remainder)
    {
    case 15:
    d += ((uint64)u.p8[14]) << 48;
    case 14:
        d += ((uint64)u.p8[13]) << 40;
    case 13:
        d += ((uint64)u.p8[12]) << 32;
    case 12:
        d += u.p32[2];
        c += u.p64[0];
        break;
    case 11:
        d += ((uint64)u.p8[10]) << 16;
    case 10:
        d += ((uint64)u.p8[9]) << 8;
    case 9:
        d += (uint64)u.p8[8];
    case 8:
        c += u.p64[0];
        break;
    case 7:
        c += ((uint64)u.p8[6]) << 48;
    case 6:
        c += ((uint64)u.p8[5]) << 40;
    case 5:
        c += ((uint64)u.p8[4]) << 32;
    case 4:
        c += u.p32[0];
        break;
    case 3:
        c += ((uint64)u.p8[2]) << 16;
    case 2:
        c += ((uint64)u.p8[1]) << 8;
    case 1:
        c += (uint64)u.p8[0];
        break;
    case 0:
        c += sc_const;
        d += sc_const;
    }
    ShortEnd(&a,&b,&c,&d);
    *hash1 = a;
    *hash2 = b;
}




// do the whole hash in one call
void SpookyHash128(const void *message, size_t length, uint64 *hash1, uint64 *hash2)
{
	if (length < sc_bufSize)
	{
		Short(message, length, hash1, hash2);
		return;
	}

	uint64 h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11;
	uint64 buf[sc_numVars];
	uint64 *end;
	union 
	{ 
		const uint8 *p8; 
		uint64 *p64; 
		size_t i; 
	} u;
	size_t remainder;
    
	h0=h3=h6=h9  = *hash1;
	h1=h4=h7=h10 = *hash2;
	h2=h5=h8=h11 = sc_const;
    
	u.p8 = (const uint8 *)message;
	end = u.p64 + (length/sc_blockSize)*sc_numVars;

	// handle all whole sc_blockSize blocks of bytes
	if (ALLOW_UNALIGNED_READS || ((u.i & 0x7) == 0))
	{
		while (u.p64 < end)
		{ 
			Mix(u.p64, &h0,&h1,&h2,&h3,&h4,&h5,&h6,&h7,&h8,&h9,&h10,&h11);
			u.p64 += sc_numVars;
		}
	}
	else
	{
		while (u.p64 < end)
		{
			memcpy(buf, u.p64, sc_blockSize);
			Mix(buf, &h0,&h1,&h2,&h3,&h4,&h5,&h6,&h7,&h8,&h9,&h10,&h11);
			u.p64 += sc_numVars;
		}
	}

	// handle the last partial block of sc_blockSize bytes
	remainder = (length - ((const uint8 *)end-(const uint8 *)message));
	memcpy(buf, end, remainder);
	memset(((uint8 *)buf)+remainder, 0, sc_blockSize-remainder);
	((uint8 *)buf)[sc_blockSize-1] = remainder;
	Mix(buf, &h0,&h1,&h2,&h3,&h4,&h5,&h6,&h7,&h8,&h9,&h10,&h11);
    
	// do some final mixing 
	End(&h0,&h1,&h2,&h3,&h4,&h5,&h6,&h7,&h8,&h9,&h10,&h11);
	*hash1 = h0;
	*hash2 = h1;
}

