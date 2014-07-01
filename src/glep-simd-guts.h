#if !defined INCLUDED_glep_simd_guts_h_
#define INCLUDED_glep_simd_guts_h_

#include "glep.h"

extern int glep_simd_cc(gleps_t g);
extern int glep_simd_gr(gcnt_t *restrict, gleps_t, const char *buf, size_t bsz);

#endif	/* INCLUDED_glep_simd_guts_h_ */
