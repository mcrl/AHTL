
#ifdef GET_TIME
#include "timer.h"
#endif
float * hist_build_tree
(
 float * _boundary, 
 unsigned int _bin_count
 );
#ifdef SIMD

int hist_uniform_float_simd 
(
 float * data, //data should be aligned
 float base, 
 float width,
 unsigned int count,
 unsigned int * bin,
 unsigned int bin_count
 );

int hist_uniform_float_simd_unrolled 
(
 float * data, //data should be aligned
 float base, 
 float width,
 unsigned int count,
 unsigned int * bin,
 unsigned int bin_count
 );

int hist_linear_float_simd 
(
 float * data, //data should be aligned
 float * boundary,
 unsigned int count,
 unsigned int * bin,
 unsigned int bin_count
);
int hist_binary_float_simd 
(
 float * data, //data should be aligned
 float * boundary,
 unsigned int count,
 unsigned int * bin,
 unsigned int bin_count,
 float * simd_pack
);
int hist_binary_float_simd_sg 
(
 float * data, //data should be aligned
 float * boundary,
 unsigned int count,
 unsigned int * bin,
 unsigned int bin_count,
 float * tree
 );

int hist_sorting_float_simd 
(
 float * data, //data should be aligned
 float * boundary,
 unsigned int count,
 unsigned int * bin,
 unsigned int bin_count
);
int hist_quicksort_float_simd 
(
 float * data, //data should be aligned
 float * boundary,
 unsigned int count,
 unsigned int * bin,
 unsigned int bin_count
);
#endif
int hist_uniform_float 
(
 float * data,
 float base, 
 float width,
 unsigned int count,
 unsigned int * bin, 
 unsigned int bin_count
 );
int hist_uniform_short 
(
 short * data,
 short base, 
 short width,
 unsigned int count,
 unsigned int * bin, 
 unsigned int bin_count
 );
int hist_uniform_float_atomic 
(
 float * data,
 float base, 
 float width,
 unsigned int count,
 unsigned int * bin,
 unsigned int bin_count
 );

int hist_linear_float 
(
 float * data,
 float * boundary,
 unsigned int count,
 unsigned int * bin,
 unsigned int bin_count
);

int hist_binary_float 
(
 float * data,
 float * boundary,
 unsigned int count,
 unsigned int * bin,
 unsigned int bin_count,
 float * tree
);

int hist_sorting_float 
(
 float * data,
 float * boundary,
 unsigned int count,
 unsigned int * bin,
 unsigned int bin_count
);
int hist_float_omp(
								float * data, 
								unsigned int count, //assume 2^n
								float * boundary, 
								unsigned int bin_size, //assume 2^n for sorting, 8^n binary
								float base, 
								float width, 
								unsigned int * bin,
								unsigned int thread_num, 
								unsigned int opcode
#ifdef GET_TIME
								,timer *t  
#endif
								);

int hist_quicksort_float 
(
 float * data, //data should be aligned
 float * boundary,
 unsigned int count,
 unsigned int * bin,
 unsigned int bin_count
);
