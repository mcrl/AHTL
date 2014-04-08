#define _GNU_SOURCE
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <float.h>
#include <unistd.h>
#include "hist_method.h"
#include "simd.h"
#include "/opt/intel/ipp/include/ipp.h"



#define MAX(x, y) ((x>y)?x:y) 
#ifndef __MIC__
#define NUMA_AWARE
#endif
//#define SLEEP


static int reduction(unsigned int ** bins, unsigned int bin_size, unsigned int tid, unsigned int thread_num)
{
	unsigned int remainder = bin_size % thread_num;
	unsigned int chunk = bin_size / thread_num;
	unsigned int from = (tid >= remainder) ? tid * chunk + remainder : tid * (chunk + 1);
	unsigned int size = (tid >= remainder) ? chunk : chunk + 1;
	unsigned int i,j;
	unsigned int * bin = bins[0];
	for(i = 1; i < thread_num; i++)
	{
					unsigned int * curbin = bins[i];
					for(j = from; j < from + size; j++)
					{
									bin[j] += curbin[j];
					}
	}
	return 0;
}
//
// opcode
// 1 : shared uniform
// 2 : private uniform
// 3 : linear
// 4 : binary
// 5 : sorting
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
								)
{
#ifdef SLEEP
				usleep(1000000);
#endif
#ifdef GET_TIME
				timer_start(t, ALL);
#endif
				unsigned int i;
				float * newboundary = NULL;
				unsigned int ** bins = (unsigned int **)malloc(sizeof(int *) * thread_num);
				bins[0] = bin;
				float * shared_tree = NULL;
				if(opcode == 4)
				{
								timer_start(t, HISTOGRAM_CORE);
								shared_tree = hist_build_tree(boundary, bin_size);
								timer_stop(t, HISTOGRAM_CORE);
				}
#ifdef __MIC__
#ifdef SIMD
				if(opcode == 6) // boundary extend
				{
						newboundary = malloc(sizeof(float) *( bin_size + VLEN + 1));
						memcpy(newboundary, boundary, sizeof(float) * (bin_size + 1));

						float big = newboundary[bin_size] + 0.1;
						int i;
						for(i=bin_size + 1; i < bin_size + VLEN + 1; i++)newboundary[i] = big;
							
				}
#endif
#endif
				unsigned int remainder = count % thread_num;
				unsigned int chunk = count / thread_num;
				omp_set_num_threads(thread_num);
#pragma omp parallel
				{
								int tid = omp_get_thread_num();
								if(tid != 0){
																bins[tid] =  _mm_malloc(sizeof(unsigned int) * MAX(bin_size, 128), BIN_ALIGN);
																memset(bins[tid],0,sizeof(unsigned int) * bin_size);
								}
								
//#define READ_FIRST
#ifdef READ_FIRST
								int x;
#ifdef USE_SAME_TREE
#ifdef SIMD
								float tmp1 = 0;
								for(x = 0; x < bin_size; x++ ) tmp1 += shared_tree[x];
								if(tmp1 == 0)
								{
												printf("Bad boundaries\n");
												exit(-1);
								}

#endif
#endif
								int tmp2 = 0;
								for(x = 0; x < bin_size; x++) tmp2 += bins[tid][x];
								if(tmp2 != 0)
								{
												printf("Bad bin\n");
												exit(-1);
								}
#endif

#pragma omp barrier
#ifdef GET_TIME
								if(tid == 0)timer_start(t, HISTOGRAM_CORE);
#endif
#ifdef ALL_THREAD_TIME
								timer_start(t, tid+100);
#endif
								unsigned int from = (tid >= remainder) ? tid * chunk + remainder : tid * (chunk + 1);
								unsigned int size = (tid >= remainder) ? chunk : chunk + 1;
								float * local_data = data + from;
								unsigned int local_count = size;
								switch(opcode)
								{
												case 1 :
																hist_uniform_float_atomic(local_data, base, width, local_count, bin, bin_size);
																break;
#ifdef SIMD
												case 2 :
																hist_uniform_float_simd(local_data, base, width, local_count, bins[tid], bin_size);
																break;
												case 3 :
																hist_linear_float_simd(local_data, boundary, local_count, bins[tid], bin_size);
																break;
												case 4 :
#ifdef USE_GATHER_SCATTER
																hist_binary_float_simd_sg(local_data, boundary, local_count, bins[tid], bin_size, shared_tree);
#else
																hist_binary_float_simd(local_data, boundary, local_count, bins[tid], bin_size, shared_tree);
#endif
																
																break;
												case 5 :
																hist_sorting_float_simd(local_data, boundary, local_count, bins[tid], bin_size);
																break;
												case 6 :
																hist_quicksort_float_simd(local_data, newboundary, local_count, bins[tid], bin_size);
																break;
#else
												case 2 :
																hist_uniform_float(local_data, base, width, local_count, bins[tid], bin_size);
																break;
												case 3 :
																hist_linear_float(local_data, boundary, local_count, bins[tid], bin_size);
																break;
												case 4 :
																hist_binary_float(local_data, boundary, local_count, bins[tid], bin_size, shared_tree);
																break;

												case 6 :
																hist_quicksort_float(local_data, boundary, local_count, bins[tid], bin_size);
																break;
												case 7 :
																hist_ipp_float(local_data, boundary, local_count, bins[tid], bin_size);
																break;


#endif
												default : 
																hist_uniform_float(local_data, base, width, local_count, bins[tid], bin_size);
																break;
								}
#ifdef ALL_THREAD_TIME
								timer_stop(t, tid+100);
#endif
#pragma omp barrier
#ifdef GET_TIME
								if(tid == 0){
												timer_stop(t, HISTOGRAM_CORE);
												timer_start(t, REDUCTION);
								}
#endif
								if(opcode != 1)
								{
												reduction(bins, bin_size, tid, thread_num);
								}
#ifdef GET_TIME
								if(tid == 0)timer_stop(t, REDUCTION);
#endif
								if(tid != 0)
								{
										_mm_free(bins[tid]);
								}
				}
				if(opcode == 4)
				{
								_mm_free(shared_tree);
				}

				free(bins);
#ifdef __MIC__
				if(opcode == 6) free(newboundary);
#endif
#ifdef GET_TIME
				timer_stop(t, ALL);
#endif
				return 0;
}


int hist_short_omp(
								unsigned short * data, 
								unsigned int count, //assume 2^n
								unsigned short * boundary, 
								unsigned int bin_size, //assume 2^n for sorting, 8^n binary
								unsigned short base, 
								unsigned short width, 
								unsigned int * bin,
								unsigned int thread_num, 
								unsigned int opcode
#ifdef GET_TIME
								,timer *t  
#endif
								)
{
				timer_start(t, ALL);
				unsigned int i;
				unsigned short * newboundary = NULL;
				unsigned int ** bins = (unsigned int **)malloc(sizeof(int *) * thread_num);
				bins[0] = bin;
				unsigned int remainder = count % thread_num;
				unsigned int chunk = count / thread_num;
				
#ifndef SIMD
#ifndef __MIC__
#ifdef IPP
				ippInit();
#endif
			//	 ippSetNumThreads(1);
#endif
#endif
				omp_set_num_threads(thread_num);

#pragma omp parallel
				{
								int tid = omp_get_thread_num();
								if(tid != 0){
																bins[tid] =  _mm_malloc(sizeof(unsigned int) * MAX(bin_size, 128), BIN_ALIGN);
																memset(bins[tid],0,sizeof(unsigned int) * bin_size);
								}
								
#pragma omp barrier
#ifdef GET_TIME
								if(tid == 0)timer_start(t, HISTOGRAM_CORE);
#endif
								unsigned int from = (tid >= remainder) ? tid * chunk + remainder : tid * (chunk + 1);
								unsigned int size = (tid >= remainder) ? chunk : chunk + 1;
								unsigned short * local_data = data + from;
								unsigned int local_count = size;
								switch(opcode)
								{
												case 1 :
																hist_uniform_short(local_data, base, width, local_count, bins[tid], bin_size);
																break;
												case 2 :
																hist_uniform_short_ipp(local_data, base, width, local_count, bins[tid], bin_size);
																break;
												default : 
																hist_uniform_short(local_data, base, width, local_count, bins[tid], bin_size);
																break;
								}
#pragma omp barrier
#ifdef GET_TIME
								if(tid == 0){
												timer_stop(t, HISTOGRAM_CORE);
												timer_start(t, REDUCTION);
								}
#endif
								if(tid != 0)
								{
										_mm_free(bins[tid]);
								}
				}
				int j;
				for(i=0;i<bin_size;i++)
						for(j=0;j<thread_num;j++)
								bin[i]+=bins[j][i];

				free(bins);
#ifdef GET_TIME
				timer_stop(t, ALL);
#endif
				return 0;
}

