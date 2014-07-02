#if !defined INCLUDED_glep_simd_guts_h_
#define INCLUDED_glep_simd_guts_h_

#include "glep.h"

extern glepcc_t glep_simd_cc(glod_pats_t);
extern int glep_simd_gr(gcnt_t *restrict, glepcc_t, const char *b, size_t z);
extern void glep_simd_fr(glepcc_t);

#endif	/* INCLUDED_glep_simd_guts_h_ */
