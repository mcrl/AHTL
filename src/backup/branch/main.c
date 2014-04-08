#include <stdio.h>
#include <unistd.h>
#include <float.h>
#include "hist_method.h"
#include "simd.h"
#include <math.h>
#include <stdlib.h>
#include <assert.h>


#define MAX(x, y) ((x>y)?x:y) 
int verbose = 0;

static void printOption(char * bin){
				printf("Usage : %s [OPTIONS]\n "
												"OPTION list \n"
												"-f [FILENAME] : 'f'ile to read. 'input.raw' by default. \n"
												"-H [TYPE] : 'H'istogram function 0=Auto(default)\n1=shared uniform\n2=private uniform\n3=linear\n4=binary\n5=sorting\n" 
												"-n [NUMBER] : 'n'umber of input data. 2^27 by default. \n" 
												"-N [NUMBER] : 'N'umber of bins. 8 by default. \n" 
												"-b [NUMBER] : 'b'ase of bin. 0 by default. \n" 
												"-w [NUMBER] : 'w'idth of each bin. 1 by default. \n" 
												"-t [NUMBER] : number of 't'hreads. 1 by default. \n"
												"-i [NUMBER] : 'i'terate the experiments & get the mean. 1 by default \n"
												"-v [0/1] : print 'v'erbosely. off by default. \n"
												"-c [0/1] : check the 'c'orrectness. off by default. \n"
												"-p [0/1] : 'p'rint histogram results on stdout. off by default. \n"
												"-T [TYPE] : data 'T'ype : 0 : float(default), 1 : short\n"
												"-h : print this message\n"
												,
												bin
							);
}

int main(int argc, char * argv[]){

				char c;
				int type = 0;
				char inputFile[128] = "input.raw";
				FILE * input;
				int opcode = 0; // auto by default
				int aligned = 1; // align on by default
				unsigned int count = 134217728;
				unsigned int bin_size = 8;
				float base = 0.0; // align on by default
				float width = 1.0; // align on by default
				int threads = 1;
				int printOutput = 0;
				int iterate = 1;
				int check = 0;

				char * words;

				while ( (c = getopt(argc, argv, "f:H:n:N:b:w:t:i:v:p:c:h:T:")) != -1)
				{
								switch (c)
								{
												case 'f':
																sprintf(inputFile, "%s", optarg);
																break;
												case 'H':
																opcode = atoi(optarg);
																break;
												case 'n':
																count = atoi(optarg);
																break;
												case 'N':
																bin_size = atoi(optarg);
																break;
												case 'b':
																base = (float)atof(optarg);
																break;
												case 'w':
																width = (float)atof(optarg);
																break;
												case 't':
																threads = atoi(optarg);
																break;
												case 'c':
																check = atoi(optarg);
																break;
												case 'v':
																verbose = atoi(optarg);
																break;
												case 'p':
																printOutput = atoi(optarg);
																break;
												case 'i':
																iterate = atoi(optarg);
																break;

												case 'T':
																type = atoi(optarg);
																break;
												default :
																printOption(argv[0]);
																return 0;
																break;
								}
				}
				if(printOutput)
								verbose = 0; // disabe other outputs
				if(verbose)
				{
								printf("read file\n");
				}

if(type==1){
short * data = _mm_malloc(sizeof(short) * count, 64);
short base = 0;
short width = 1;
				input = fopen(inputFile, "r");
				assert(input);
				fread(data, sizeof(short), count, input);

				if(verbose)
				{
								printf("read file complete\n");
				}

				if(verbose)
				{
								printf("start histogram\n");
				}

				timer *histogramTime = malloc(sizeof(timer) * iterate);
				int i;

				//init Histogram
				short * boundary = _mm_malloc(sizeof(short) * (bin_size + 1), 64);
				
				unsigned * bin;
				for(i = 0; i < bin_size + 1; i++)
				{
								boundary[i] = base + width * i;
				}

				if(check == 3)
				{
								for(i = 0; i < count; i++)
								{
												if(boundary[0] > data[i])
												{
																fprintf(stderr, "%d'th data %d is smaller than base %d!\n", i, data[i], boundary[0]);
																exit(-1);
												}
												if(boundary[bin_size] < data[i])
												{
																fprintf(stderr, "%d'th data %d is bigger than max boundary %d!\n", i, data[i], boundary[bin_size]);
																exit(-1);
												}
								}
				}

				//main loop
				if(opcode != 4)
				{
				bin = _mm_malloc(sizeof(unsigned) * (MAX(bin_size, 128)) ,BIN_ALIGN);
				timer_init(&histogramTime[0]);
				hist_short_omp(data, count, boundary, bin_size, base, width, bin, threads, opcode, &histogramTime[0]);
				for(i = 0; i < iterate; i++)
				{
								//bin = _mm_malloc(sizeof(unsigned) * (MAX(bin_size, 128)) ,BIN_ALIGN);
								memset(bin, 0, sizeof(unsigned) * bin_size);
								timer_init(&histogramTime[i]);
								if(verbose)
								{
												printf("\niteration %d..\n", i);
								}
								hist_short_omp(data, count, boundary, bin_size, base, width, bin, threads, opcode, &histogramTime[i]);
								if(i == iterate - 1) break;
								//_mm_free(bin);
				}
				}
									if(printOutput)
				{
								printf("print!\n");
								for(i=0;i<bin_size;i++)printf("%d\n",bin[i]);
				}
				else{
								long long int cycles[500] = {0};
								double time[500] = {0.0};
								double timesq[500] = {0.0};
								double timestdev[500] = {0.0};

								for(i = 0; i < iterate; i++)
								{
												int x;
												for(x = 0; x < 500; x++)
												{
																cycles[x] += timer_get_cycle(histogramTime + i, (work)x);
																time[x] += timer_get_time(histogramTime + i, (work)x);
																timesq[x] += timer_get_time(histogramTime + i, (work)x) * timer_get_time(histogramTime + i, (work)x);
												}
								}
								for(i = 0; i < 500; i++)
								{
												cycles[i]/=iterate;
												time[i]/=iterate;
												timesq[i]/=iterate;
												timestdev[i]=sqrt(timesq[i] - time[i] * time[i]);
								}

								printf("CORE : %.5f +- %.5f sec, %.5f GUPS\n", time[(unsigned)HISTOGRAM_CORE], timestdev[(unsigned)HISTOGRAM_CORE], ((float)count /1000000000)/time[(unsigned)HISTOGRAM_CORE]);
								printf("REDUCTION : %.5f +- %.5f sec, %.5f GUPS\n", time[(unsigned)REDUCTION], timestdev[(unsigned)REDUCTION], ((float)count /1000000000)/time[(unsigned)REDUCTION]);
								printf("ALL : %.5f +- %.5f sec, %.5f GUPS\n", time[(unsigned)ALL], timestdev[(unsigned)ALL], ((float)count /1000000000)/time[(unsigned)ALL]);

				}

return 0;
}

				float * data = _mm_malloc(sizeof(float) * count, 64);
				input = fopen(inputFile, "r");
				assert(input);
				fread(data, sizeof(float), count, input);

				if(verbose)
				{
								printf("read file complete\n");
				}

				if(verbose)
				{
								printf("start histogram\n");
				}

				timer *histogramTime = malloc(sizeof(timer) * iterate);
				int i;

				//init Histogram
				float * boundary = _mm_malloc(sizeof(float) * (bin_size + 1), 64);
				
				unsigned * bin;
				for(i = 0; i < bin_size + 1; i++)
				{
								boundary[i] = base + width * i;
				}

				if(check == 3)
				{
								for(i = 0; i < count; i++)
								{
												if(boundary[0] > data[i])
												{
																fprintf(stderr, "%d'th data %lf is smaller than base %lf!\n", i, data[i], boundary[0]);
																exit(-1);
												}
												if(boundary[bin_size] < data[i])
												{
																fprintf(stderr, "%d'th data %lf is bigger than max boundary %lf!\n", i, data[i], boundary[bin_size]);
																exit(-1);
												}
								}
				}

				//main loop
				if(opcode != 4)
				{
				bin = _mm_malloc(sizeof(unsigned) * (MAX(bin_size, 128)) ,BIN_ALIGN);
				timer_init(&histogramTime[0]);
				hist_float_omp(data, count, boundary, bin_size, base, width, bin, threads, opcode, &histogramTime[0]);
				for(i = 0; i < iterate; i++)
				{
								//bin = _mm_malloc(sizeof(unsigned) * (MAX(bin_size, 128)) ,BIN_ALIGN);
								memset(bin, 0, sizeof(unsigned) * bin_size);
								timer_init(&histogramTime[i]);
								if(verbose)
								{
												printf("\niteration %d..\n", i);
								}
								hist_float_omp(data, count, boundary, bin_size, base, width, bin, threads, opcode, &histogramTime[i]);
								if(i == iterate - 1) break;
								//_mm_free(bin);
				}
				}
				else
				{
						bin = _mm_malloc(sizeof(unsigned) * (MAX(bin_size, 128)) ,BIN_ALIGN);
						timer_init(&histogramTime[0]);
						hist_float_omp(data, count, boundary, bin_size, base, width, bin, threads, opcode, &histogramTime[0]);
						for(i = 0; i < iterate; i++)
						{
							//	bin = _mm_malloc(sizeof(unsigned) * (MAX(bin_size, 128)) ,BIN_ALIGN);
								memset(bin, 0, sizeof(unsigned) * bin_size);
								timer_init(&histogramTime[i]);
								if(verbose)
								{
										printf("\niteration %d..\n", i);
								}
								hist_float_omp(data, count, boundary, bin_size, base, width, bin, threads, opcode, &histogramTime[i]);
						}
				}

				if(verbose)
				{
								printf("histogram finished\n");
				}

				if(printOutput)
				{
								printf("print!\n");
				}
				else{
								long long int cycles[500] = {0};
								double time[500] = {0.0};
								double timesq[500] = {0.0};
								double timestdev[500] = {0.0};

								for(i = 0; i < iterate; i++)
								{
												int x;
												for(x = 0; x < 500; x++)
												{
																cycles[x] += timer_get_cycle(histogramTime + i, (work)x);
																time[x] += timer_get_time(histogramTime + i, (work)x);
																timesq[x] += timer_get_time(histogramTime + i, (work)x) * timer_get_time(histogramTime + i, (work)x);
												}
								}
								for(i = 0; i < 500; i++)
								{
												cycles[i]/=iterate;
												time[i]/=iterate;
												timesq[i]/=iterate;
												timestdev[i]=sqrt(timesq[i] - time[i] * time[i]);
								}

								printf("CORE : (%.5f - %.5f) %.5f +- %.5f sec, %.5f GUPS\n", histogramTime->from[HISTOGRAM_CORE], histogramTime->to[HISTOGRAM_CORE], time[(unsigned)HISTOGRAM_CORE], timestdev[(unsigned)HISTOGRAM_CORE], ((float)count /1000000000)/time[(unsigned)HISTOGRAM_CORE]);
								printf("REDUCTION : %.5f +- %.5f sec, %.5f GUPS\n", time[(unsigned)REDUCTION], timestdev[(unsigned)REDUCTION], ((float)count /1000000000)/time[(unsigned)REDUCTION]);
								printf("ALL : %.5f +- %.5f sec, %.5f GUPS\n", time[(unsigned)ALL], timestdev[(unsigned)ALL], ((float)count /1000000000)/time[(unsigned)ALL]);

#ifdef ALL_THREAD_TIME
								for(i = 0; i < threads; i++)
								printf("thread %d : (%.5f - %.5f) %.5f +- %.5f sec, %.5f GUPS\n", i, histogramTime->from[i+100], histogramTime->to[i+100], time[i + 100], timestdev[i + 100], ((float)count /1000000000)/time[i + 100]);

#endif
				}



				if(check)
				{
								unsigned * bin_to_compare = _mm_malloc(sizeof(unsigned) * (bin_size), 64);
								memset(bin_to_compare, 0, sizeof(unsigned) * bin_size);
								//hist_uniform_float(data, base, width, count, bin, bin_size);
								hist_uniform_float(data, base, width, count, bin_to_compare, bin_size);
								for(i = 0; i< bin_size; i++)
								{
												if(bin[i] != bin_to_compare[i])
																break;
								}
								if(i != bin_size)
								{
												fprintf(stdout, "Incorrect! %dth bin should be %d but %d\n", i, bin_to_compare[i], bin[i]);
												
								}
								else
												fprintf(stdout, "Correct!\n");

								if(check == 2)
												{
																for(i = 0; i < bin_size; i++)
																				fprintf(stderr, "%d %d\n", bin_to_compare[i], bin[i]);
																unsigned int sum1=0, sum2=0;
																for(i = 0; i < bin_size; i++)
																{
																				sum1+=bin_to_compare[i];
																				sum2+=bin[i];
																}
																fprintf(stderr, "%d %d\n", sum1, sum2);
												}

				}


				return 0;				
}
