#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef SIMD
#include "simd.h"
#endif

#define EPS 1e-3

#ifdef SIMD
float * hist_build_tree(float * _boundary, unsigned int _bin_count)
{
				int i, j, simd=0;


				int N2bin_count; // complete binary tree size

				for(N2bin_count=1; N2bin_count < _bin_count; N2bin_count *= VLEN); 

				float * boundary = _mm_malloc(sizeof(float) * N2bin_count, 64); 
				memcpy(boundary, _boundary, sizeof(float) * _bin_count);

				float largest = _boundary[_bin_count] + EPS;
				for(i = _bin_count; i < N2bin_count; i++) //init the empty tree element with the largest bin boundary
								boundary[i] = largest;	

				float * simd_pack = _mm_malloc(sizeof(float) * N2bin_count * 2, 4096); //simd packed tree
				memset(simd_pack, 0, sizeof(float) * N2bin_count * 2); //size of simd packed tree is larger than bin.

				int H_OFFSET; //height of small tree with VLEN - 1 element
				for(H_OFFSET = 0, i=1; i != VLEN; i *= 2, H_OFFSET++); 

				int middle = N2bin_count / 2; // root idx

				//first, make heap-like BST


				int HEIGHT; // height of BST
				for(HEIGHT = 0, i=N2bin_count; i > 0; i /= 2, HEIGHT++); 


				unsigned int sortidx[VLEN];
#ifdef SSE
				sortidx[0] = 1;
				sortidx[1] = 0;
				sortidx[2] = 2;
#elif AVX
				sortidx[0] = 3;
				sortidx[1] = 1;
				sortidx[2] = 5;
				sortidx[3] = 0;
				sortidx[4] = 2;
				sortidx[5] = 4;
				sortidx[6] = 6;
#else // MIC
				sortidx[0] = 7;
				sortidx[1] = 3;
				sortidx[2] = 11;
				sortidx[3] = 1;
				sortidx[4] = 5;
				sortidx[5] = 9;
				sortidx[6] = 13;
				sortidx[7] = 0;
				sortidx[8] = 2;
				sortidx[9] = 4;
				sortidx[10] = 6;
				sortidx[11] = 8;
				sortidx[12] = 10;
				sortidx[13] = 12;
				sortidx[14] = 14;
#endif


//#define DEBUG_TREE_BUILD
				int global_height, global_offset, global_from, global_count, global_by;
				int local_height, local_offset, local_from, local_to, local_by, local_count, li;

				for(global_height = 0, global_offset = middle, global_from = middle, global_count = 1, global_by = global_offset * 2;
												global_height < HEIGHT - 1;
												global_height += H_OFFSET, global_by /= VLEN, global_count *= VLEN, global_offset /= VLEN
					 )//set vars for pointing subtrees
				{
								for(i = global_from; i < global_from + global_by * global_count; i += global_by) // point 1 subtree 
								{
												li = 0;

												for(local_height = 0, local_from = i, local_by = global_by, local_count = 1;
																				local_height < H_OFFSET;
																				local_height++, local_by /= 2, local_from -= local_by / 2, local_count *= 2) // add one row per loop
												{
																for(j = local_from; j < local_from + local_count * local_by; j += local_by)
																{
																				simd_pack[simd + sortidx[li++]] = boundary[j];
#ifdef DEBUG_TREE_BUILD
																				printf("li=%d : from %d to %d %f\n",li,j,simd+sortidx[li-1], boundary[j]);
																				fflush(stdout);
#endif
																				boundary[j] = 0;
																}
												}

												simd_pack[simd + li++] = largest; // for aligning
#ifdef DEBUG_TREE_BUILD
												printf("!!\n");
#endif
												simd+=VLEN;
								}
								
								for(i = VLEN; i > 1; i /=2)
												global_from -= global_offset / i;
				}


#ifdef DEBUG_TREE_BUILD
				for(i=0;i<N2bin_count;i++)
				{
								printf("%d %f \n", i, simd_pack[i]);
				}
#endif

				_mm_free (boundary);

				return simd_pack;

}
int hist_binary_float_simd 
(
 float * data, //data should be aligned
 float * _boundary,
 unsigned int count,
 unsigned int * bin,
 unsigned int _bin_count,
 float * simd_pack
 )
{
				// First, make binary tree structure


				//unsigned int bin[1024]={0};
				//printf("%llx\n", bin);

#ifndef USE_SAME_TREE
				simd_pack = hist_build_tree(_boundary, _bin_count);
#endif
				int i, j, simd=0;

				int N2bin_count; // complete binary tree size

				for(N2bin_count=1; N2bin_count < _bin_count; N2bin_count *= VLEN); 

		//		printf("N2bin_count = %d, count=%d\n", N2bin_count, count);
				

				float * boundary = _mm_malloc(sizeof(float) * N2bin_count, 64); 
				memcpy(boundary, _boundary, sizeof(float) * _bin_count);

				float largest = _boundary[_bin_count] + EPS;
				for(i = _bin_count; i < N2bin_count; i++) //init the empty tree element with the largest bin boundary
								boundary[i] = largest;	


				int H_OFFSET; //height of small tree with VLEN - 1 element
				for(H_OFFSET = 0, i=1; i != VLEN; i *= 2, H_OFFSET++); 

				int HEIGHT; // height of BST
				for(HEIGHT = 0, i=N2bin_count; i > 0; i /= 2, HEIGHT++); 


			
#ifdef SSE
				unsigned int table[8]={0};
				table[0] = 0;
				table[1] = 1;
				table[3] = 2;
				table[7] = 3;
#elif AVX
				unsigned int table[128];
				table[0] = 0;
				table[1] = 1;
				table[3] = 2;
				table[7] = 3;
				table[15] = 4;
				table[31] = 5;
				table[63] = 6;
				table[127] = 7;
				/*
				table[0] = 0;
				table[8] = 1;
				table[10] = 2;
				table[26] = 3;
				table[27] = 4;
				table[59] = 5;
				table[63] = 6;
				table[127] = 7;
				*/
#else // MIC
				/*
				unsigned int table[256];
				table[0] = 0;
				table[1] = 1;
				table[3] = 2;
				table[7] = 3;
				table[15] = 4;
				table[31] = 5;
				table[63] = 6;
				table[127] = 7;
				table[255] = 8;
				*/
				unsigned int table[32768];
				table[0] = 0;
				table[1] = 1;
				table[3] = 2;
				table[7] = 3;
				table[15] = 4;
				table[31] = 5;
				table[63] = 6;
				table[127] = 7;
				table[255] = 8;
				table[511] = 9;
				table[1023] = 10;
				table[2047] = 11;
				table[4095] = 12;
				table[8191] = 13;
				table[16383] = 14;
				table[32767] = 15;
#endif

				//#define DEBUG_BIN_SEARCH
				unsigned int idx, tst, curidx, tmpForReduction = 0 ;
				//#define PRECALCULATED_IDX
#ifdef PRECALCULATED_IDX
				unsigned int tsts[16];
				for(i = 0, tst=0 ; i < HEIGHT / H_OFFSET; i++)
				{
						tst = VLEN * tst + VLEN;
						tsts[i] = tst;
				}
#endif
#ifdef DEBUG_BIN_SEARCH
				printf("%d iteration per element\n", HEIGHT / H_OFFSET);
#endif
				float x;
//#pragma noprefetch data
//#pragma unroll(2)
#define MANUAL_UNROLL
#ifdef MANUAL_UNROLL
				float x1, x2, x3, x4;
				unsigned int idx1, idx2, idx3, idx4;
				unsigned int curidx1, curidx2, curidx3, curidx4;
				_VECTOR xmm_range = _MM_LOAD(simd_pack);
				for(i = 0; i + 3 < count; i+=4)
				{
						x1 = data[i];
						x2 = data[i+1];
						x3 = data[i+2];
						x4 = data[i+3];
						_VECTOR item1 = _MM_SET1(x1);
						_VECTOR item2 = _MM_SET1(x2);
						_VECTOR item3 = _MM_SET1(x3);
						_VECTOR item4 = _MM_SET1(x4);
						tst = VLEN;
#ifdef __MIC__
						curidx1 = _mm512_cmp_ps_mask(item1, xmm_range , _CMP_GE_OS);
						idx1 = curidx1 = table[curidx1];
						curidx2 = _mm512_cmp_ps_mask(item2, xmm_range , _CMP_GE_OS);
						idx2 = curidx2 = table[curidx2];
						curidx3 = _mm512_cmp_ps_mask(item3, xmm_range , _CMP_GE_OS);
						idx3 = curidx3 = table[curidx3];
						curidx4 = _mm512_cmp_ps_mask(item4, xmm_range , _CMP_GE_OS);
						idx4 = curidx4 = table[curidx4];

#else
#ifdef SSE
						curidx1 = _mm_movemask_ps( _mm_cmpge_ps(item1, xmm_range ));
						curidx2 = _mm_movemask_ps( _mm_cmpge_ps(item2, xmm_range ));
						curidx3 = _mm_movemask_ps( _mm_cmpge_ps(item3, xmm_range ));
						curidx4 = _mm_movemask_ps( _mm_cmpge_ps(item4, xmm_range ));
#elif AVX
						curidx1 = _mm256_movemask_ps( _mm256_cmp_ps(item1, xmm_range , _CMP_GE_OS));
						curidx2 = _mm256_movemask_ps( _mm256_cmp_ps(item2, xmm_range , _CMP_GE_OS));
						curidx3 = _mm256_movemask_ps( _mm256_cmp_ps(item3, xmm_range , _CMP_GE_OS));
						curidx4 = _mm256_movemask_ps( _mm256_cmp_ps(item4, xmm_range , _CMP_GE_OS));
#endif
						idx1 = curidx1 = table[curidx1];
						idx2 = curidx2 = table[curidx2];
						idx3 = curidx3 = table[curidx3];
						idx4 = curidx4 = table[curidx4];
#endif
						for(j = 1; j < HEIGHT / H_OFFSET; j++)
						{
#ifdef __MIC__
								_VECTOR xmm_range1 = _MM_LOAD(simd_pack + tst + idx1 * VLEN);
								_VECTOR xmm_range2 = _MM_LOAD(simd_pack + tst + idx2 * VLEN);
								_VECTOR xmm_range3 = _MM_LOAD(simd_pack + tst + idx3 * VLEN);
								_VECTOR xmm_range4 = _MM_LOAD(simd_pack + tst + idx4 * VLEN);
								tst = VLEN * tst + VLEN;
								curidx1 = _mm512_cmp_ps_mask(item1, xmm_range1, _CMP_GE_OS);
								curidx1 = table[curidx1];
								curidx2 = _mm512_cmp_ps_mask(item2, xmm_range2, _CMP_GE_OS);
								idx1 = VLEN * idx1 + curidx1;
								curidx2 = table[curidx2];
								curidx3 = _mm512_cmp_ps_mask(item3, xmm_range3, _CMP_GE_OS);
								idx2 = VLEN * idx2 + curidx2;
								curidx3 = table[curidx3];
								curidx4 = _mm512_cmp_ps_mask(item4, xmm_range4, _CMP_GE_OS);
								idx3 = VLEN * idx3 + curidx3;
								curidx4 = table[curidx4];
								idx4 = VLEN * idx4 + curidx4;

#else
								_VECTOR xmm_range1 = _MM_LOAD(simd_pack + tst + idx1 * VLEN);
								_VECTOR xmm_range2 = _MM_LOAD(simd_pack + tst + idx2 * VLEN);
								_VECTOR xmm_range3 = _MM_LOAD(simd_pack + tst + idx3 * VLEN);
								_VECTOR xmm_range4 = _MM_LOAD(simd_pack + tst + idx4 * VLEN);
#ifdef SSE
								curidx1 = _mm_movemask_ps( _mm_cmpge_ps(item1, xmm_range1));
								curidx2 = _mm_movemask_ps( _mm_cmpge_ps(item2, xmm_range2));
								curidx3 = _mm_movemask_ps( _mm_cmpge_ps(item3, xmm_range3));
								curidx4 = _mm_movemask_ps( _mm_cmpge_ps(item4, xmm_range4));
#elif AVX
								curidx1 = _mm256_movemask_ps( _mm256_cmp_ps(item1, xmm_range1, _CMP_GE_OS));
								curidx2 = _mm256_movemask_ps( _mm256_cmp_ps(item2, xmm_range2, _CMP_GE_OS));
								curidx3 = _mm256_movemask_ps( _mm256_cmp_ps(item3, xmm_range3, _CMP_GE_OS));
								curidx4 = _mm256_movemask_ps( _mm256_cmp_ps(item4, xmm_range4, _CMP_GE_OS));
#endif
								curidx1 = table[curidx1];
								curidx2 = table[curidx2];
								curidx3 = table[curidx3];
								curidx4 = table[curidx4];
								tst = VLEN * tst + VLEN;
								idx1 = VLEN * idx1 + curidx1;
								idx2 = VLEN * idx2 + curidx2;
								idx3 = VLEN * idx3 + curidx3;
								idx4 = VLEN * idx4 + curidx4;
#endif
						}
						bin[idx1]++;
						bin[idx2]++;
						bin[idx3]++;
						bin[idx4]++;
				}
#else
				for(i = 0; i < count; i++)
				{
					   x = data[i];
						 //if(!(i & 0xf))
						 	//	_mm_prefetch((char const *)(data + i + 512), _MM_HINT_T0 );
						// x =  (float) ((i * 47) % 256);
						_VECTOR item = _MM_SET1(x);
						idx = tst = curidx = 0;
#ifdef DEBUG_BIN_SEARCH
						printf("%dth input %f\n",i,x);
#endif
						for(j = 0; j < HEIGHT / H_OFFSET; j++)
						{
								_VECTOR xmm_range = _MM_LOAD(simd_pack + tst + idx * VLEN);
								//printf("hello world %f %f %f %f\n", simd_pack[0], simd_pack[1], simd_pack[2], simd_pack[3]);
								//printf("tree idx : %d, data %f, idx=%d, %f\n", tst + idx*VLEN, x,idx, simd_pack[tst + idx*VLEN]);
#ifdef SSE
								curidx = _mm_movemask_ps( _mm_cmpge_ps(item, xmm_range));
#elif AVX
								curidx = _mm256_movemask_ps( _mm256_cmp_ps(item, xmm_range, _CMP_GE_OS));
#else
								curidx = _mm512_cmp_ps_mask(item, xmm_range, _CMP_GE_OS);
#endif
#ifdef DEBUG_BIN_SEARCH
								printf("%d : cmpresult=%d\n",j,curidx);
#endif
								/*
#ifdef __MIC__
curidx = table[curidx>>8] + table[curidx % 256];
#else
*/
								curidx = table[curidx];
								//#endif
#ifdef DEBUG_BIN_SEARCH
								printf("%d : localidx=%d\n",j,curidx);
#endif

#ifdef PRECALCULATED_IDX
								tst = tsts[j];
#else
								tst = VLEN * tst + VLEN;
#endif
								idx = VLEN * idx + curidx;
#ifdef DEBUG_BIN_SEARCH
								printf("%d : globalidx=%d\n",j,idx);
								fflush(stdout);
#endif
						}
#ifdef NO_UPDATE
						tmpForReduction += idx;
#else
						//printf("idx : %d\n", idx);
						bin[idx]++;
#endif
						//if(i==65536)break;
				}
#ifdef NO_UPDATE
				bin[0] = tmpForReduction;
#endif
#endif
				

#ifdef MANUAL_UNROLL
				for(; i < count; i++)
				{
								unsigned int h = _bin_count;
								unsigned  l = 0;
								float d = data[i];
								while(h - l != 1) // binary search
								{
												int middle = (h - l) / 2 + l;
												if(boundary[middle] > d) h = middle;
												else l = middle;
								}
								bin[l]++;
				}
#endif

			//	printf("last element %d\n", bin[0]);

				_mm_free(boundary);

#ifndef USE_SAME_TREE
				_mm_free(simd_pack);
#endif
				return 0;
}

#endif

int hist_binary_float 
(
 float * data, //data should be aligned
 float * boundary,
 unsigned int count,
 unsigned int * bin,
 unsigned int bin_count
 )
{
		unsigned low = 0, high = bin_count;
		int i;
		for(i = 0; i < count; i++)
		{
				unsigned int h = high;
				unsigned  l = low;
			float d = data[i];
			while(h - l != 1) // binary search
			{
				int middle = (h - l) / 2 + l;
				if(boundary[middle] > d) h = middle;
				else l = middle;
			}
			bin[l]++;
		}
	return 0;
}

