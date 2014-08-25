#include "partition.h"
#include "simd.h"


#define _MM_MASK_USTORE(mt, mask, v1) \
    _mm512_mask_packstorelo_ps(mt, mask, v1); \
_mm512_mask_packstorehi_ps(mt+VLEN, mask, v1); 

#define _MM_ULOAD(v1, mt) \
    v1 = _mm512_loadunpacklo_ps(v1, mt); \
v1 = _mm512_loadunpackhi_ps(v1, mt+VLEN);

#define MAX(a, b) ((a>b)?a:b)

#define SMART_SCALAR

//#define USE_STACK
//#define WITHOUT_MEMCPY

#define MANUAL_UNROLL
#define PREFETCH
#define DIST 64
//#define DIST 1024

#define ADAPTIVE
#define PREPARTITION
#ifdef PREPARTITION
#ifndef ADAPTIVE
#define PFROM 100
#define PTO 101
#else
#define X 90
#endif
#endif

#define GET_NEW_RAND(i, n) ((unsigned int)(i * 1042331) % n)

//#define SINGLESTEP_TEST

#define BLOCK_SIZE 8192
//#define BLOCK_SIZE 65536
#define SMALL_CHUNK_KILL
//#define USE_BIN_SEARCH
//#define PROFILE
//#define PROFILE2
static int bin_size = 0;
#ifdef SIMD
#ifdef __MIC__

#ifdef SMART_SCALAR
__mmask16 front_mask[16];
__mmask16 back_mask[16];
#endif
static void printv_epi32(__m512i v, char *str)
{
    int i;
    __declspec(align(64)) int tmp[16];
    printf("%s:", str);
    _mm512_store_epi32(tmp, v);
    for(i=0; i < 16; i++)
    {
        tmp[0] = tmp[i];
        printf("[%d]=%d ", i, tmp[0]);
    }
    printf("\n");
}
static void printv(__m512 v, char * str)
{
    int i;
    __declspec(align(64)) float tmp[16];
    printf("%s:", str);
    _mm512_store_ps(tmp, v);
    for(i=0; i < 16; i++)
    {
        tmp[0] = tmp[i];
        printf("[%d]=%f ", i, tmp[0]);
    }
    printf("\n");
}
#endif
#endif

#ifdef PROFILE

int case1=0, case2=0, case3=0, case4=0, case5=0, case6=0;
#endif
#ifdef PROFILE2
int front1 = 0, main1 = 0, back1 = 0, front2 = 0, main2 = 0, back2 = 0, last1= 0, last2=0;
#endif
//#define DEBUG_PART
#ifdef SIMD
#ifdef __MIC__

static int pre_partition(float * bufs[2], _VECTOR * boundary_v, float * boundary, int * bin, int df, int dt, int b, int bMIN, int bMAX)
{
    float * from = bufs[0];
    float * to = bufs[1];
    int i;
    int tf = df, tt = dt;
    _VECTOR pi = boundary_v[b];
    float p = boundary[b];

    if(dt == df) return dt;
    if(b == bMIN) return df;
    if(b == bMAX) return dt;

    float * tmp = bufs[0];
    bufs[0] = bufs[1];
    bufs[1] = tmp;

    for(i = df; i < dt && (i % VLEN) != 0; i++)
    {
        int tmp;
        if((tmp = from[i]) >= p)
            to[--tt] = tmp;
        else
            to[tf++] = tmp;
    }

    //main loop
    for(; i + VLEN * 4 -1 < dt; i += VLEN * 4)
    {
        _VECTOR tmp1 = _MM_LOAD(from + i);
        _VECTOR tmp2 = _MM_LOAD(from + i + VLEN);
        _VECTOR tmp3 = _MM_LOAD(from + i + VLEN * 2);
        _VECTOR tmp4 = _MM_LOAD(from + i + VLEN * 3);
#ifdef PREFETCH
        _mm_prefetch((char const *)(from + i + DIST), _MM_HINT_NT);
        _mm_prefetch((char const *)(from + i + DIST + VLEN), _MM_HINT_NT);
        _mm_prefetch((char const *)(from + i + DIST + VLEN * 2), _MM_HINT_NT);
        _mm_prefetch((char const *)(from + i + DIST + VLEN * 3), _MM_HINT_NT);
        _mm_prefetch((char const *)(to + tt - DIST), _MM_HINT_NT);
        _mm_prefetch((char const *)(to + tt - DIST - VLEN), _MM_HINT_NT);
        _mm_prefetch((char const *)(to + tf + DIST), _MM_HINT_NT);
        _mm_prefetch((char const *)(to + tf + DIST + VLEN), _MM_HINT_NT);
#endif
        __mmask16 t1 = _mm512_cmp_ps_mask(tmp1, pi, _CMP_GE_OS);
        __mmask16 t2 = _mm512_cmp_ps_mask(tmp2, pi, _CMP_GE_OS);
        __mmask16 t3 = _mm512_cmp_ps_mask(tmp3, pi, _CMP_GE_OS);
        __mmask16 t4 = _mm512_cmp_ps_mask(tmp4, pi, _CMP_GE_OS);
        __mmask16 t1_ = _mm512_knot(t1);
        __mmask16 t2_ = _mm512_knot(t2);
        __mmask16 t3_ = _mm512_knot(t3);
        __mmask16 t4_ = _mm512_knot(t4);
        int x1 = _mm_countbits_32(t1);
        int x2 = _mm_countbits_32(t2);
        int x3 = _mm_countbits_32(t3);
        int x4 = _mm_countbits_32(t4);
        tt -= x1;
        _MM_MASK_USTORE(to + tt, t1, tmp1);
        _MM_MASK_USTORE(to + tf, t1_, tmp1);
        tf += VLEN - x1; 
        tt -= x2;
        _MM_MASK_USTORE(to + tt, t2, tmp2);
        _MM_MASK_USTORE(to + tf, t2_, tmp2);
        tf += VLEN - x2; 
        tt -= x3;
        _MM_MASK_USTORE(to + tt, t3, tmp3);
        _MM_MASK_USTORE(to + tf, t3_, tmp3);
        tf += VLEN - x3; 
        tt -= x4;
        _MM_MASK_USTORE(to + tt, t4, tmp4);
        _MM_MASK_USTORE(to + tf, t4_, tmp4);
        tf += VLEN - x4; 
    }
    for(; i + VLEN-1 < dt; i+=VLEN)
    {
        _VECTOR tmp = _MM_LOAD(from + i);
        __mmask16 t = _mm512_cmp_ps_mask(tmp, pi, _CMP_GE_OS);
        __mmask16 t2 = _mm512_knot(t);
        int x = _mm_countbits_32(t);
        tt -= x;
        _MM_MASK_USTORE(to + tt, t, tmp);
        _MM_MASK_USTORE(to + tf, t2, tmp);
        tf += VLEN - x; 
    }

    for(; i < dt; i++)
    {
        int tmp;
        if((tmp = from[i]) >= p)
            to[--tt] = tmp;
        else
            to[tf++] = tmp;
    }
    //printf("%d %d\n", tt, tf);

    return tt;
}


/* stack funcitons */

#define MAX_STACK_SIZE 8192
#define INIT \
    _top = 0; \
_fromto = 0;

#define PUSH(df, dt, bf, bt) \
    _dfs[_top] = df;\
_dts[_top] = dt;\
_bfs[_top] = bf;\
_bts[_top] = bt;\
_buf[_top] = !_fromto;\
_top++;

#define POP(df, dt, bf, bt, from, to) \
    _top--;\
df = _dfs[_top];\ 
dt = _dts[_top];\ 
bf = _bfs[_top];\ 
bt = _bts[_top];\
   _fromto = _buf[_top];\
   from = (_fromto)?from_:to_;\
   to = (!_fromto)?from_:to_;

#define IS_EMPTY (_top == 0)
static void partition_stack(int ** buffers, float * from_, float * to_, _VECTOR * boundary_v, float * boundary, int * bin, int df, int dt, int bf, int bt)
{

    int * _dfs=buffers[0];
    int *_dts=buffers[1];
    int *_bfs=buffers[2];
    int *_bts=buffers[3];
    int *_buf=buffers[4];


    int _top;
    int _fromto;

    INIT;
    PUSH(df, dt, bf, bt);

    while(!IS_EMPTY)
    {
        float * from, *to;
        POP(df, dt, bf, bt, from, to);

        int i;
        int tf = df, tt = dt;

        //printf("%d %d %d %d\n", df, dt, bf, bt);

        _VECTOR pi = boundary_v[(bf + bt) / 2];
        float p = boundary[(bf + bt) / 2];

        if(bt == bf) continue;

        if(bt - bf == 1)
        {
            bin[bf] += dt - df;
#ifdef PROFILE
            case1+= (dt - df);
#endif
            continue;
        }


        if(bt - bf == 2)
        {
#ifdef PROFILE
            case2+= (dt - df);
#endif
            int tot = 0;
#ifdef SMART_SCALAR
            if(dt - df >= VLEN){
                int df2 = df & 0xfffffff0;
                int count = (VLEN - (df - df2)) % VLEN;
                __mmask16 mask = back_mask[count];

                _VECTOR tmp1 = _MM_LOAD(from + df2);
                __mmask16 t1 = _mm512_cmp_ps_mask(tmp1, pi, _CMP_GE_OS);
                t1 = _mm512_kand(t1, mask);
                tot += _mm_countbits_32(t1);

                i = df + (count);
            }
            else
            {
                for(i = df; (i< dt) && (i % VLEN != 0); i++)
                {
                    int tmp;
                    if((tmp = from[i]) >= p)
                        tot++;
                }

            }


#else
            for(i = df; (i< dt) && (i % VLEN != 0); i++)
            {
#ifdef PROFILE2
                front1++;
#endif
                int tmp;
                if((tmp = from[i]) >= p)
                    tot++;
            }

#endif
            //main loop
            for(; i + VLEN * 4 -1 < dt; i += VLEN * 4)
            {
#ifdef PROFILE2
                main1+=64;
#endif
                _VECTOR tmp1 = _MM_LOAD(from + i);
                _VECTOR tmp2 = _MM_LOAD(from + i + VLEN);
                _VECTOR tmp3 = _MM_LOAD(from + i + VLEN * 2);
                _VECTOR tmp4 = _MM_LOAD(from + i + VLEN * 3);
#ifdef PREFETCH
                _mm_prefetch((char const *)(from + i + DIST), _MM_HINT_NT);
                _mm_prefetch((char const *)(from + i + DIST + VLEN), _MM_HINT_NT);
                _mm_prefetch((char const *)(from + i + DIST + VLEN * 2), _MM_HINT_NT);
                _mm_prefetch((char const *)(from + i + DIST + VLEN * 3), _MM_HINT_NT);
#endif
                __mmask16 t1 = _mm512_cmp_ps_mask(tmp1, pi, _CMP_GE_OS);
                __mmask16 t2 = _mm512_cmp_ps_mask(tmp2, pi, _CMP_GE_OS);
                __mmask16 t3 = _mm512_cmp_ps_mask(tmp3, pi, _CMP_GE_OS);
                __mmask16 t4 = _mm512_cmp_ps_mask(tmp4, pi, _CMP_GE_OS);
                tot += _mm_countbits_32(t1);
                tot += _mm_countbits_32(t2);
                tot += _mm_countbits_32(t3);
                tot += _mm_countbits_32(t4);
            }
            for(; i + VLEN-1 < dt; i+=VLEN)
            {
#ifdef PROFILE2
                back1+=16;
#endif
                _VECTOR tmp = _MM_LOAD(from + i);
                __mmask16 t = _mm512_cmp_ps_mask(tmp, pi, _CMP_GE_OS);
                int x = _mm_countbits_32(t);
                tot += x;
            }

            //printf("Vpartfinished %d %d\n", i, dt);

#ifdef SMART_SCALAR
            if( i < dt)
            {
                int count = (dt - i);
                //      printf("%d\n", count);
                __mmask16 mask = front_mask[count];

                _VECTOR tmp1 = _MM_LOAD(from + i);
                __mmask16 t1 = _mm512_cmp_ps_mask(tmp1, pi, _CMP_GE_OS);
                t1 = _mm512_kand(t1, mask);
                tot += _mm_countbits_32(t1);
            }
#else
            for(; i < dt; i++)
            {
#ifdef PROFILE2
                last1++;
#endif
                int tmp;
                if((tmp = from[i]) >= p)
                    tot++;
            }
#endif

            bin[bf] += dt - df - tot;
#ifdef DEBUG_PART
            printf("%d-%d-%d = %d\n",dt, df, tot, dt-df-tot); 
#endif
            bin[bf+1] += tot;
            continue;
        }
        if(dt == df) continue;

        if(dt - df == 1)
        {
#ifdef PROFILE
            case3++;
#endif
            float d = from[df];
            int j;
            for(j = bt; d < boundary[j]; j--);
            bin[j]++;
            continue;
        }
#ifdef USE_BIN_SEARCH 
        if((bt - bf <= VLEN))
        {
#ifdef PROFILE
            case4+= dt - df;
#endif
            _VECTOR piv = _mm512_undefined_ps();
            _MM_ULOAD(piv, (boundary + bf));

            int i;
            //printf("%d %d %d %d\n", bt, bf, dt, df);
            for(i = df; i + 15< dt; i+=16)
            {
                int t1 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i]), piv, _CMP_GE_OS));
                int t2 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+1]), piv, _CMP_GE_OS));
                int t3 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+2]), piv, _CMP_GE_OS));
                int t4 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+3]), piv, _CMP_GE_OS));
                int t5 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+4]), piv, _CMP_GE_OS));
                int t6 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+5]), piv, _CMP_GE_OS));
                int t7 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+6]), piv, _CMP_GE_OS));
                int t8 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+7]), piv, _CMP_GE_OS));
                int t9 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+8]), piv, _CMP_GE_OS));
                int t10 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+9]), piv, _CMP_GE_OS));
                int t11 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+10]), piv, _CMP_GE_OS));
                int t12 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+11]), piv, _CMP_GE_OS));
                int t13 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+12]), piv, _CMP_GE_OS));
                int t14 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+13]), piv, _CMP_GE_OS));
                int t15 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+14]), piv, _CMP_GE_OS));
                int t16 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+15]), piv, _CMP_GE_OS));
                bin[bf + t1-1]++;
                bin[bf + t2-1]++;
                bin[bf + t3-1]++;
                bin[bf + t4-1]++;
                bin[bf + t5-1]++;
                bin[bf + t6-1]++;
                bin[bf + t7-1]++;
                bin[bf + t8-1]++;
                bin[bf + t9-1]++;
                bin[bf + t10-1]++;
                bin[bf + t11-1]++;
                bin[bf + t12-1]++;
                bin[bf + t13-1]++;
                bin[bf + t14-1]++;
                bin[bf + t15-1]++;
                bin[bf + t16-1]++;
#ifdef PREFETCH
                _mm_prefetch((char const *)(from + i + DIST*2), _MM_HINT_NT);
#endif
            }
            for(; i < dt; i++)
            {
                int t = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i]), piv, _CMP_GE_OS));
                bin[bf + t-1]++;
            }
            continue;
        }
#endif
#ifdef SMALL_CHUNK_KILL

        if((bt - bf <= VLEN) && (dt - df < VLEN))
        {
#ifdef PROFILE
            case5 += (dt - df);
#endif
            _VECTOR piv = _mm512_undefined_ps();
            _MM_ULOAD(piv, (boundary + bf));

            int i;
            //printf("%d %d %d %d\n", bt, bf, dt, df);
            for(i = df; i < dt; i++)
            {
                int t = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i]), piv, _CMP_GE_OS));
                //printv(piv, "piv");
                //printv(_MM_SET1(from[i]), "this");
                //printf("at %d + %d, %f\n", bf,t, from[i]);
                bin[bf + t-1]++;
            }
            continue;
        }
#endif

#ifdef PROFILE
        case6+= dt - df;
#endif
#ifdef SMART_SCALAR
        if(dt - df >= VLEN){
            int df2 = df & 0xfffffff0;
            int count = (VLEN - (df - df2)) % VLEN;
            __mmask16 mask = back_mask[count];

            _VECTOR tmp1 = _MM_LOAD(from + df2);
            __mmask16 t1 = _mm512_cmp_ps_mask(tmp1, pi, _CMP_GE_OS);
            __mmask16 t1_ = _mm512_knot(t1);
            t1 = _mm512_kand(t1, mask);
            t1_ = _mm512_kand(t1_, mask);
            int x1 = _mm_countbits_32(t1);
            tt -= x1;
            _MM_MASK_USTORE(to + tt, t1, tmp1);
            _MM_MASK_USTORE(to + tf, t1_, tmp1);
            tf += (count) - x1; 

            i = df + (count);
            /*
               printv(tmp1, "tmp");
               printv(pi, "pi");
               printf("%d, %d, %d, %d\n", df, diff, i, x1);
               printf("%x, %x, %x\n", mask, t1, t1_);
               */
        }
        else
        {
            for(i = df; i < dt && (i % VLEN) != 0; i++)
            {
#ifdef PROFILE2
                front2++;
#endif
                int tmp;
                if((tmp = from[i]) >= p)
                    to[--tt] = tmp;
                else
                    to[tf++] = tmp;
            }
        }

#else
        for(i = df; i < dt && (i % VLEN) != 0; i++)
        {
#ifdef PROFILE2
            front2++;
#endif
            int tmp;
            if((tmp = from[i]) >= p)
                to[--tt] = tmp;
            else
                to[tf++] = tmp;
        }
#endif
        //main loop
        for(; i + VLEN * 4 -1 < dt; i += VLEN * 4)
        {
#ifdef PROFILE2
            main2+=64;
#endif
            _VECTOR tmp1 = _MM_LOAD(from + i);
            _VECTOR tmp2 = _MM_LOAD(from + i + VLEN);
            _VECTOR tmp3 = _MM_LOAD(from + i + VLEN * 2);
            _VECTOR tmp4 = _MM_LOAD(from + i + VLEN * 3);
#ifdef PREFETCH
            _mm_prefetch((char const *)(from + i + DIST), _MM_HINT_NT);
            _mm_prefetch((char const *)(from + i + DIST + VLEN), _MM_HINT_NT);
            _mm_prefetch((char const *)(from + i + DIST + VLEN * 2), _MM_HINT_NT);
            _mm_prefetch((char const *)(from + i + DIST + VLEN * 3), _MM_HINT_NT);
            _mm_prefetch((char const *)(to + tt - DIST), _MM_HINT_NT);
            _mm_prefetch((char const *)(to + tt - DIST - VLEN), _MM_HINT_NT);
            _mm_prefetch((char const *)(to + tf + DIST), _MM_HINT_NT);
            _mm_prefetch((char const *)(to + tf + DIST + VLEN), _MM_HINT_NT);
#endif
            __mmask16 t1 = _mm512_cmp_ps_mask(tmp1, pi, _CMP_GE_OS);
            __mmask16 t2 = _mm512_cmp_ps_mask(tmp2, pi, _CMP_GE_OS);
            __mmask16 t3 = _mm512_cmp_ps_mask(tmp3, pi, _CMP_GE_OS);
            __mmask16 t4 = _mm512_cmp_ps_mask(tmp4, pi, _CMP_GE_OS);
            __mmask16 t1_ = _mm512_knot(t1);
            __mmask16 t2_ = _mm512_knot(t2);
            __mmask16 t3_ = _mm512_knot(t3);
            __mmask16 t4_ = _mm512_knot(t4);
            int x1 = _mm_countbits_32(t1);
            int x2 = _mm_countbits_32(t2);
            int x3 = _mm_countbits_32(t3);
            int x4 = _mm_countbits_32(t4);
            tt -= x1;
            _MM_MASK_USTORE(to + tt, t1, tmp1);
            _MM_MASK_USTORE(to + tf, t1_, tmp1);
            tf += VLEN - x1; 
            tt -= x2;
            _MM_MASK_USTORE(to + tt, t2, tmp2);
            _MM_MASK_USTORE(to + tf, t2_, tmp2);
            tf += VLEN - x2; 
            tt -= x3;
            _MM_MASK_USTORE(to + tt, t3, tmp3);
            _MM_MASK_USTORE(to + tf, t3_, tmp3);
            tf += VLEN - x3; 
            tt -= x4;
            _MM_MASK_USTORE(to + tt, t4, tmp4);
            _MM_MASK_USTORE(to + tf, t4_, tmp4);
            tf += VLEN - x4; 
        }
        for(; i + VLEN-1 < dt; i+=VLEN)
        {
#ifdef PROFILE2
            back2+=16;
#endif
            _VECTOR tmp = _MM_LOAD(from + i);
            __mmask16 t = _mm512_cmp_ps_mask(tmp, pi, _CMP_GE_OS);
            __mmask16 t2 = _mm512_knot(t);
            int x = _mm_countbits_32(t);
            tt -= x;
            _MM_MASK_USTORE(to + tt, t, tmp);
            _MM_MASK_USTORE(to + tf, t2, tmp);
            tf += VLEN - x; 
        }


#ifdef SMART_SCALAR
        if(i < dt)
        {
            int count = (dt - i);
            __mmask16 mask = front_mask[count];

            _VECTOR tmp1 = _MM_LOAD(from + i);
            __mmask16 t1 = _mm512_cmp_ps_mask(tmp1, pi, _CMP_GE_OS);
            __mmask16 t1_ = _mm512_knot(t1);
            t1 = _mm512_kand(t1, mask);
            t1_ = _mm512_kand(t1_, mask);
            int x1 = _mm_countbits_32(t1);
            tt -= x1;
            _MM_MASK_USTORE(to + tt, t1, tmp1);
            _MM_MASK_USTORE(to + tf, t1_, tmp1);
            tf += (count) - x1; 
        }

        /*
           printv(tmp1, "tmp");
           printv(pi, "pi");
           printf("%d, %d, %d, %d\n", df, diff, i, x1);
           printf("%x, %x, %x\n", mask, t1, t1_);
           */

#else


        for(; i < dt; i++)
        {
#ifdef PROFILE2
            last2++;
#endif
            int tmp;
            if((tmp = from[i]) >= p)
                to[--tt] = tmp;
            else
                to[tf++] = tmp;
        }
#endif
        //tf--;
        //tt++;
        //
        //printf("%d == %d\n", tf, tt);

        PUSH(tt, dt, (bf+bt)/2, bt);
        PUSH(df, tf, bf, (bf+bt)/2);
    }
    return;
}


static void partition(float * from, float * to, _VECTOR * boundary_v, float * boundary, int * bin, int df, int dt, int bf, int bt)
{
    //if(bin_size / (bt-bf) > 16) return;
    int i;
    int tf = df, tt = dt;
#ifdef DEBUG_PART
    printf("%d %d %d %d\n", df, dt, bf, bt);
#endif
    _VECTOR pi = boundary_v[(bf + bt) / 2];
    float p = boundary[(bf + bt) / 2];

    if(bt == bf) return;

    if(bt - bf == 1)
    {
        bin[bf] += dt - df;
#ifdef PROFILE
        case1+= (dt - df);
#endif
        return;
    }




    if(bt - bf == 2) // Do not need to move data. Only performs counting.
    {
#ifdef PROFILE
        case2+= (dt - df);
#endif
        int tot = 0;
#ifdef SMART_SCALAR
        if(dt - df >= VLEN){
            int df2 = df & 0xfffffff0;
            int count = (VLEN - (df - df2)) % VLEN;
            __mmask16 mask = back_mask[count];

            _VECTOR tmp1 = _MM_LOAD(from + df2);
            __mmask16 t1 = _mm512_cmp_ps_mask(tmp1, pi, _CMP_GE_OS);
            t1 = _mm512_kand(t1, mask);
            tot += _mm_countbits_32(t1);

            i = df + (count);
        }
        else
        {
            for(i = df; (i< dt) && (i % VLEN != 0); i++)
            {
                int tmp;
                if((tmp = from[i]) >= p)
                    tot++;
            }

        }


#else
        for(i = df; (i< dt) && (i % VLEN != 0); i++)
        {
#ifdef PROFILE2
            front1++;
#endif
            int tmp;
            if((tmp = from[i]) >= p)
                tot++;
        }

#endif
        //main loop
        for(; i + VLEN * 4 -1 < dt; i += VLEN * 4)
        {
#ifdef PROFILE2
            main1+=64;
#endif
            _VECTOR tmp1 = _MM_LOAD(from + i);
            _VECTOR tmp2 = _MM_LOAD(from + i + VLEN);
            _VECTOR tmp3 = _MM_LOAD(from + i + VLEN * 2);
            _VECTOR tmp4 = _MM_LOAD(from + i + VLEN * 3);
#ifdef PREFETCH
            _mm_prefetch((char const *)(from + i + DIST), _MM_HINT_NT);
            _mm_prefetch((char const *)(from + i + DIST + VLEN), _MM_HINT_NT);
            _mm_prefetch((char const *)(from + i + DIST + VLEN * 2), _MM_HINT_NT);
            _mm_prefetch((char const *)(from + i + DIST + VLEN * 3), _MM_HINT_NT);
#endif
            __mmask16 t1 = _mm512_cmp_ps_mask(tmp1, pi, _CMP_GE_OS);
            __mmask16 t2 = _mm512_cmp_ps_mask(tmp2, pi, _CMP_GE_OS);
            __mmask16 t3 = _mm512_cmp_ps_mask(tmp3, pi, _CMP_GE_OS);
            __mmask16 t4 = _mm512_cmp_ps_mask(tmp4, pi, _CMP_GE_OS);
            tot += _mm_countbits_32(t1);
            tot += _mm_countbits_32(t2);
            tot += _mm_countbits_32(t3);
            tot += _mm_countbits_32(t4);
        }
        for(; i + VLEN-1 < dt; i+=VLEN)
        {
#ifdef PROFILE2
            back1+=16;
#endif
            _VECTOR tmp = _MM_LOAD(from + i);
            __mmask16 t = _mm512_cmp_ps_mask(tmp, pi, _CMP_GE_OS);
            int x = _mm_countbits_32(t);
            tot += x;
        }

        //printf("Vpartfinished %d %d\n", i, dt);

#ifdef SMART_SCALAR
        if( i < dt)
        {
            int count = (dt - i);
            //      printf("%d\n", count);
            __mmask16 mask = front_mask[count];

            _VECTOR tmp1 = _MM_LOAD(from + i);
            __mmask16 t1 = _mm512_cmp_ps_mask(tmp1, pi, _CMP_GE_OS);
            t1 = _mm512_kand(t1, mask);
            tot += _mm_countbits_32(t1);
        }
#else
        for(; i < dt; i++)
        {
#ifdef PROFILE2
            last1++;
#endif
            int tmp;
            if((tmp = from[i]) >= p)
                tot++;
        }
#endif

        bin[bf] += dt - df - tot;
#ifdef DEBUG_PART
        printf("%d-%d-%d = %d\n",dt, df, tot, dt-df-tot); 
#endif
        bin[bf+1] += tot;
        return;
    } 


    if(dt == df) return; // nothing should be in this range

    if(dt - df == 1) // Only one data element
    {
#ifdef PROFILE
        case3++;
#endif
        float d = from[df];
        int j;
        for(j = bt; d < boundary[j]; j--);
        bin[j]++;
        return;
    }

#ifdef USE_BIN_SEARCH // Do not use
    if((bt - bf <= VLEN)) // if there are few bins, scan the data once and find the bin by using binary search.
    {
#ifdef PROFILE
        case4+= dt - df;
#endif
        _VECTOR piv = _mm512_undefined_ps();
        _MM_ULOAD(piv, (boundary + bf));

        int i;
        //printf("%d %d %d %d\n", bt, bf, dt, df);
        for(i = df; i + 15< dt; i+=16)
        {
            int t1 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i]), piv, _CMP_GE_OS));
            int t2 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+1]), piv, _CMP_GE_OS));
            int t3 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+2]), piv, _CMP_GE_OS));
            int t4 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+3]), piv, _CMP_GE_OS));
            int t5 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+4]), piv, _CMP_GE_OS));
            int t6 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+5]), piv, _CMP_GE_OS));
            int t7 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+6]), piv, _CMP_GE_OS));
            int t8 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+7]), piv, _CMP_GE_OS));
            int t9 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+8]), piv, _CMP_GE_OS));
            int t10 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+9]), piv, _CMP_GE_OS));
            int t11 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+10]), piv, _CMP_GE_OS));
            int t12 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+11]), piv, _CMP_GE_OS));
            int t13 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+12]), piv, _CMP_GE_OS));
            int t14 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+13]), piv, _CMP_GE_OS));
            int t15 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+14]), piv, _CMP_GE_OS));
            int t16 = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i+15]), piv, _CMP_GE_OS));
            bin[bf + t1-1]++;
            bin[bf + t2-1]++;
            bin[bf + t3-1]++;
            bin[bf + t4-1]++;
            bin[bf + t5-1]++;
            bin[bf + t6-1]++;
            bin[bf + t7-1]++;
            bin[bf + t8-1]++;
            bin[bf + t9-1]++;
            bin[bf + t10-1]++;
            bin[bf + t11-1]++;
            bin[bf + t12-1]++;
            bin[bf + t13-1]++;
            bin[bf + t14-1]++;
            bin[bf + t15-1]++;
            bin[bf + t16-1]++;
#ifdef PREFETCH
            _mm_prefetch((char const *)(from + i + DIST*2), _MM_HINT_NT);
#endif
        }
        for(; i < dt; i++)
        {
            int t = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i]), piv, _CMP_GE_OS));
            bin[bf + t-1]++;
        }
        return;
    }
#endif
#ifdef SMALL_CHUNK_KILL // If the data partition is small, do not proceed partitoning


    if((bt - bf <= VLEN) && (dt - df < VLEN))
    {
#ifdef PROFILE
        case5 += (dt - df);
#endif
        _VECTOR piv = _mm512_undefined_ps();
        _MM_ULOAD(piv, (boundary + bf));

        int i;
        //printf("%d %d %d %d\n", bt, bf, dt, df);
        for(i = df; i < dt; i++)
        {
            int t = _mm_countbits_32(_mm512_cmp_ps_mask(_MM_SET1(from[i]), piv, _CMP_GE_OS));
            //printv(piv, "piv");
            //printv(_MM_SET1(from[i]), "this");
            //printf("at %d + %d, %f\n", bf,t, from[i]);
            bin[bf + t-1]++;
        }
        return;
    }
#endif

#ifdef PROFILE
    case6+= dt - df;
#endif
#ifdef SMART_SCALAR
    if(dt - df >= VLEN){ // General case.
        int df2 = df & 0xfffffff0;
        int count = (VLEN - (df - df2)) % VLEN;
        __mmask16 mask = back_mask[count];

        _VECTOR tmp1 = _MM_LOAD(from + df2);
        __mmask16 t1 = _mm512_cmp_ps_mask(tmp1, pi, _CMP_GE_OS);
        __mmask16 t1_ = _mm512_knot(t1);
        t1 = _mm512_kand(t1, mask);
        t1_ = _mm512_kand(t1_, mask);
        int x1 = _mm_countbits_32(t1);
        tt -= x1;
        _MM_MASK_USTORE(to + tt, t1, tmp1);
        _MM_MASK_USTORE(to + tf, t1_, tmp1);
        tf += (count) - x1; 

        i = df + (count);
        /*
           printv(tmp1, "tmp");
           printv(pi, "pi");
           printf("%d, %d, %d, %d\n", df, diff, i, x1);
           printf("%x, %x, %x\n", mask, t1, t1_);
           */
    }
    else
    {
        for(i = df; i < dt && (i % VLEN) != 0; i++)
        {
#ifdef PROFILE2
            front2++;
#endif
            int tmp;
            if((tmp = from[i]) >= p)
                to[--tt] = tmp;
            else
                to[tf++] = tmp;
        }
    }

#else
    for(i = df; i < dt && (i % VLEN) != 0; i++)
    {
#ifdef PROFILE2
        front2++;
#endif
        int tmp;
        if((tmp = from[i]) >= p)
            to[--tt] = tmp;
        else
            to[tf++] = tmp;
    }
#endif
    //main loop
    for(; i + VLEN * 4 -1 < dt; i += VLEN * 4)
    {
#ifdef PROFILE2
        main2+=64;
#endif
        _VECTOR tmp1 = _MM_LOAD(from + i);
        _VECTOR tmp2 = _MM_LOAD(from + i + VLEN);
        _VECTOR tmp3 = _MM_LOAD(from + i + VLEN * 2);
        _VECTOR tmp4 = _MM_LOAD(from + i + VLEN * 3);
#ifdef PREFETCH
        _mm_prefetch((char const *)(from + i + DIST), _MM_HINT_NT);
        _mm_prefetch((char const *)(from + i + DIST + VLEN), _MM_HINT_NT);
        _mm_prefetch((char const *)(from + i + DIST + VLEN * 2), _MM_HINT_NT);
        _mm_prefetch((char const *)(from + i + DIST + VLEN * 3), _MM_HINT_NT);
        _mm_prefetch((char const *)(to + tt - DIST), _MM_HINT_NT);
        _mm_prefetch((char const *)(to + tt - DIST - VLEN), _MM_HINT_NT);
        _mm_prefetch((char const *)(to + tf + DIST), _MM_HINT_NT);
        _mm_prefetch((char const *)(to + tf + DIST + VLEN), _MM_HINT_NT);
#endif
        __mmask16 t1 = _mm512_cmp_ps_mask(tmp1, pi, _CMP_GE_OS);
        __mmask16 t2 = _mm512_cmp_ps_mask(tmp2, pi, _CMP_GE_OS);
        __mmask16 t3 = _mm512_cmp_ps_mask(tmp3, pi, _CMP_GE_OS);
        __mmask16 t4 = _mm512_cmp_ps_mask(tmp4, pi, _CMP_GE_OS);
        __mmask16 t1_ = _mm512_knot(t1);
        __mmask16 t2_ = _mm512_knot(t2);
        __mmask16 t3_ = _mm512_knot(t3);
        __mmask16 t4_ = _mm512_knot(t4);
        int x1 = _mm_countbits_32(t1);
        int x2 = _mm_countbits_32(t2);
        int x3 = _mm_countbits_32(t3);
        int x4 = _mm_countbits_32(t4);
        tt -= x1;
        _MM_MASK_USTORE(to + tt, t1, tmp1);
        _MM_MASK_USTORE(to + tf, t1_, tmp1);
        tf += VLEN - x1; 
        tt -= x2;
        _MM_MASK_USTORE(to + tt, t2, tmp2);
        _MM_MASK_USTORE(to + tf, t2_, tmp2);
        tf += VLEN - x2; 
        tt -= x3;
        _MM_MASK_USTORE(to + tt, t3, tmp3);
        _MM_MASK_USTORE(to + tf, t3_, tmp3);
        tf += VLEN - x3; 
        tt -= x4;
        _MM_MASK_USTORE(to + tt, t4, tmp4);
        _MM_MASK_USTORE(to + tf, t4_, tmp4);
        tf += VLEN - x4; 
    }
    for(; i + VLEN-1 < dt; i+=VLEN)
    {
#ifdef PROFILE2
        back2+=16;
#endif
        _VECTOR tmp = _MM_LOAD(from + i);
        __mmask16 t = _mm512_cmp_ps_mask(tmp, pi, _CMP_GE_OS);
        __mmask16 t2 = _mm512_knot(t);
        int x = _mm_countbits_32(t);
        tt -= x;
        _MM_MASK_USTORE(to + tt, t, tmp);
        _MM_MASK_USTORE(to + tf, t2, tmp);
        tf += VLEN - x; 
    }


#ifdef SMART_SCALAR
    if(i < dt)
    {
        int count = (dt - i);
        __mmask16 mask = front_mask[count];

        _VECTOR tmp1 = _MM_LOAD(from + i);
        __mmask16 t1 = _mm512_cmp_ps_mask(tmp1, pi, _CMP_GE_OS);
        __mmask16 t1_ = _mm512_knot(t1);
        t1 = _mm512_kand(t1, mask);
        t1_ = _mm512_kand(t1_, mask);
        int x1 = _mm_countbits_32(t1);
        tt -= x1;
        _MM_MASK_USTORE(to + tt, t1, tmp1);
        _MM_MASK_USTORE(to + tf, t1_, tmp1);
        tf += (count) - x1; 
    }

    /*
       printv(tmp1, "tmp");
       printv(pi, "pi");
       printf("%d, %d, %d, %d\n", df, diff, i, x1);
       printf("%x, %x, %x\n", mask, t1, t1_);
       */

#else


    for(; i < dt; i++)
    {
#ifdef PROFILE2
        last2++;
#endif
        int tmp;
        if((tmp = from[i]) >= p)
            to[--tt] = tmp;
        else
            to[tf++] = tmp;
    }
#endif
    //tf--;
    //tt++;
    //
    //printf("%d == %d\n", tf, tt);
    partition(to, from, boundary_v, boundary, bin, df, tf, bf, (bf+bt)/2);
    partition(to, from, boundary_v, boundary, bin, tt, dt, (bf+bt)/2, bt);
    return;
}
#endif
#endif
//int hist_quicksort_float_simd
int hist_partition_float_simd
(
 float * data, //data should be aligned
 float * boundary,
 unsigned int count,
 int * bin,
 unsigned int bin_count
 )
{
    bin_size = bin_count;
#ifdef SIMD
#ifdef __MIC__

    size_t remainder;
    size_t start = 0;
    unsigned int i;
    int j;
    //  float * boundary = _mm_malloc(sizeof(float) * (bin_count + VLEN + 1), 64);
    //    memcpy(boundary, _boundary, sizeof(float) * (bin_count + 1));
    //    float big = boundary[bin_count] + 0.1;
    //    for(i=bin_count + 1; i < bin_count + VLEN + 1; i++)boundary[i] = big;
    if(remainder = ((unsigned long long int)data) % (VLEN * sizeof(float)))
    {
        unsigned int i;
        start = (VLEN - (remainder / sizeof(float)));
        for(i = 0; i < start; i++)
        {
            float d = data[i];
            for(j = bin_count-1; d < boundary[j]; j--);
            bin[j]++;
        }
    }


    unsigned int n = bin_count;

    _VECTOR * v_boundary = (_VECTOR *)_mm_malloc(sizeof(_VECTOR) * (n + 1),4096);
    for( i = 0; i < n+1; i++)
    {
        v_boundary[i] = _MM_SET1(boundary[i]);
    }


#ifndef BLOCK_SIZE
      int BLOCK_SIZE = count - start;
#endif
#ifdef SMART_SCALAR

    front_mask[0] = _mm512_int2mask(0);
    front_mask[1] = _mm512_int2mask(0x1);
    front_mask[2] = _mm512_int2mask(0x3);
    front_mask[3] = _mm512_int2mask(0x7);
    front_mask[4] = _mm512_int2mask(0xf);
    front_mask[5] = _mm512_int2mask(0x1f);
    front_mask[6] = _mm512_int2mask(0x3f);
    front_mask[7] = _mm512_int2mask(0x7f);
    front_mask[8] = _mm512_int2mask(0xff);
    front_mask[9] = _mm512_int2mask(0x1ff);
    front_mask[10] = _mm512_int2mask(0x3ff);
    front_mask[11] = _mm512_int2mask(0x7ff);
    front_mask[12] = _mm512_int2mask(0xfff);
    front_mask[13] = _mm512_int2mask(0x1fff);
    front_mask[14] = _mm512_int2mask(0x3fff);
    front_mask[15] = _mm512_int2mask(0x7fff);

    back_mask[0] = _mm512_int2mask(0x0);
    back_mask[1] = _mm512_int2mask(0x8000);
    back_mask[2] = _mm512_int2mask(0xc000);
    back_mask[3] = _mm512_int2mask(0xe000);
    back_mask[4] = _mm512_int2mask(0xf000);
    back_mask[5] = _mm512_int2mask(0xf800);
    back_mask[6] = _mm512_int2mask(0xfc00);
    back_mask[7] = _mm512_int2mask(0xfe00);
    back_mask[8] = _mm512_int2mask(0xff00);
    back_mask[9] = _mm512_int2mask(0xff80);
    back_mask[10] = _mm512_int2mask(0xffc0);
    back_mask[11] = _mm512_int2mask(0xffe0);
    back_mask[12] = _mm512_int2mask(0xfff0);
    back_mask[13] = _mm512_int2mask(0xfff8);
    back_mask[14] = _mm512_int2mask(0xfffc);
    back_mask[15] = _mm512_int2mask(0xfffe);


#endif
    unsigned int x = 0;
    int iter;
    float * buf1 = (float *)_mm_malloc(sizeof(float) * (BLOCK_SIZE + VLEN), 4096);
    float * buf2 = (float *)_mm_malloc(sizeof(float) * (BLOCK_SIZE + VLEN), 4096);
    int * local_bin = (int *)_mm_malloc(sizeof(int) * bin_count, 64);
    //float * buf1 = _mm_malloc(sizeof(float) * (count - start), 64);
    //memcpy(buf1, data, sizeof(float) * (count - start));
    //float buf1[BLOCK_SIZE];
    //float buf2[BLOCK_SIZE];

#ifdef USE_STACK
    int _dfs[MAX_STACK_SIZE];
    int _dts[MAX_STACK_SIZE];
    int _bfs[MAX_STACK_SIZE];
    int _bts[MAX_STACK_SIZE];
    int _buf[MAX_STACK_SIZE];
    int * buffers[5];
    buffers[0] = _dfs;
    buffers[1] = _dts;
    buffers[2] = _bfs;
    buffers[3] = _bts;
    buffers[4] = _buf;
#endif
#ifdef SINGLESTEP_TEST
    float * bf = _mm_malloc(sizeof(float) * count, 64);
    float * bufs[2];
    bufs[0] = data + start;
    bufs[1] = bf;

    
    pre_partition(bufs, v_boundary, boundary, bin, 0, count /2 , bin_count/2, 0, bin_count);

    _mm_free(bf);
    _mm_free(v_boundary);
    _mm_free(buf1);
    _mm_free(buf2);
    _mm_free(local_bin);
    return 0 ;
#endif
    for( iter = start; iter + BLOCK_SIZE - 1 < count; iter+=BLOCK_SIZE)
    {
#ifndef WITHOUT_MEMCPY
        memcpy(buf1, data + iter, sizeof(float) * BLOCK_SIZE);
#endif
        //continue;

#ifdef PREPARTITION
        int idx1, idx2;
        int sample_i;
        int PFROM, PTO;
        int max_cnt;
        PFROM = 0;
        float * bufs[2];
        bufs[0] = buf1;
        bufs[1] = buf2;
        memset(local_bin, 0, sizeof(int) * bin_count); 

        // find PFROM, PTO
        max_cnt = 0;
        unsigned int l;
        int j;
        for(j = 0; j < 500 / (100 - X) ; j++)
        {
          //printf("%d\n", sample_i);
          float d = buf1[j];
          int i;
          for(l = bin_count-1; d < boundary[l]; l--);
          local_bin[l]++;
          if(local_bin[l] > max_cnt)
          {
            PFROM = l;
            max_cnt = local_bin[l];
            break;
          }
        }
        //printf("%d\n", PFROM);
        PTO = PFROM + 1;
        //printf("%d\n", PFROM);
        //exit(0);
        
        idx1 = pre_partition(bufs, v_boundary, boundary, bin, 0, BLOCK_SIZE, PFROM, 0, bin_count);
        memcpy(buf1, buf2, sizeof(float) * idx1);
        idx2 = pre_partition(bufs, v_boundary, boundary, bin, idx1, BLOCK_SIZE, PTO, 0, bin_count);

        //    printf("%d %d %d %d\n", idx1, idx2, PFROM, PTO);
        if(bufs[0] == buf1){
            partition(buf1,  buf2, v_boundary, boundary, bin, 0, idx1, 0, PFROM);
            //bin[PFROM] += idx2 - idx1;
            partition(buf1,  buf2, v_boundary, boundary, bin, idx1, idx2, PFROM, PTO);
            partition(buf1,  buf2, v_boundary, boundary, bin, idx2, BLOCK_SIZE, PTO, bin_count);
        }
        else
        {
            partition(buf2,  buf1, v_boundary, boundary, bin, 0, idx1, 0, PFROM);
            //bin[PFROM] += idx2 - idx1;
            partition(buf2,  buf1, v_boundary, boundary, bin, idx1, idx2, PFROM, PTO);
            partition(buf2,  buf1, v_boundary, boundary, bin, idx2, BLOCK_SIZE, PTO, bin_count);
        }
#else

#ifdef WITHOUT_MEMCPY
        int idx1, idx2;
        float * bufs[2];
        bufs[0] = data + iter;
        bufs[1] = buf2;
        idx1 = pre_partition(bufs, v_boundary, boundary, bin, 0, BLOCK_SIZE, bin_count / 2, 0, bin_count);
        partition(buf2,  buf1, v_boundary, boundary, bin, 0, idx1, 0, bin_count/2);
        partition(buf2,  buf1, v_boundary, boundary, bin, idx1, BLOCK_SIZE, bin_count/2, bin_count);

#else
#ifdef USE_STACK
        partition_stack(buffers, buf1,  buf2, v_boundary, boundary, bin, 0, BLOCK_SIZE, 0, bin_count);
#else
        partition(buf1,  buf2, v_boundary, boundary, bin, 0, BLOCK_SIZE, 0, bin_count);
#endif

#endif
#endif

        //    partition(data + iter,  buf2, v_boundary, boundary, bin, 0, BLOCK_SIZE, 0, bin_count);
#ifdef DEBUG_PART
        return 0;
#endif
    }

    unsigned int remain = count - iter;
    memcpy(buf1, data + iter, sizeof(float) * remain);
    partition(buf1, buf2, v_boundary, boundary, bin, 0, remain, 0, bin_count); 
    //printf("%d remain\n", remain);
    /*
       if(remain > 256)
       {
       unsigned int N2remain = 1;
       for(; N2remain < remain; N2remain*=2);
       N2remain /=2;

    //    printf("%d N2remain\n", N2remain);
    memcpy(buf1, data + iter, sizeof(float) * N2remain);
    partition(buf1, buf2, v_boundary, boundary, bin, 0, N2remain, 0, bin_count); 
    iter+=N2remain;
    }
    for(i = iter; i < count; i++)
    {
    float d = data[i];
    for(j = bin_count-1; d < boundary[j]; j--);
    bin[j]++;
    }
    */
#ifdef PROFILE
    printf("profile result %d %d %d %d %d %d \n", case1, case2, case3, case4, case5, case6);
#endif
#ifdef PROFILE2
    printf("profile2 result %d %d %d %d => %d, %d %d %d %d => %d\n", front1, main1, back1, last1, front1+main1+back1+last1, front2, main2, back2, last2, front2+main2+back2+last2);
#endif
    _mm_free(v_boundary);
    _mm_free(buf1);
    _mm_free(buf2);
    _mm_free(local_bin);
    //_mm_free(boundary);
#endif
#endif
    return 0;
}

static void partition_scalar(float * from, float * to, float * boundary, int * bin, int df, int dt, int bf, int bt)
{
    int i;
    int tf = df, tt = dt;
    float p = boundary[(bf + bt) / 2];

    if(bt == bf) return;

    if(bt - bf == 1)
    {
        bin[bf] += dt - df;
        return;
    }

    if(bt - bf == 2)
    {
        int tot = 0;
                //main loop
        for(i = df; i + 3< dt; i += 4)
        {
            float d1 = from[i];
            float d2 = from[i+1];
            float d3 = from[i+2];
            float d4 = from[i+3];
            int r1 = d1>=p;
            int r2 = d2>=p;
            int r3 = d3>=p;
            int r4 = d4>=p;
            tot+=r1;
            tot+=r2;
            tot+=r3;
            tot+=r4;
#ifdef __MIC__
#endif
        }
        for(; i<dt; i++) 
          tot += (from[i] >= p);

        //printf("Vpartfinished %d %d\n", i, dt);
        bin[bf] += dt - df - tot;
        bin[bf+1] += tot;
        return;
    }
    if(dt == df) return;

    if(dt - df == 1)
    {
        return;
        float d = from[df];
        int j;
        for(j = bt; d < boundary[j]; j--);
        bin[j]++;
        return;
    }
    //main loop
    for(;tf<tt;)
    {
        while(from[tf]<p)tf++;
        do{tt--;}while(from[tt]>=p);
        if(tf<tt)
        {
          //  printf("swap %f, %f at %d, %d\n", from[tf], from[tt],tf, tt);
            float t = from[tt];
            from[tt]=from[tf];
            from[tf]=t;
        }
    }
    tt++;
    partition_scalar(from, to, boundary, bin, df, tt, bf, (bf+bt)/2);
    partition_scalar(from, to, boundary, bin, tt, dt, (bf+bt)/2, bt);
    return;
}

//int hist_quicksort_float 
int hist_partition_float 
(
 float * data, 
 float * boundary,
 unsigned int count,
 int * bin,
 unsigned int bin_count
 )
{

      int i;

#ifndef BLOCK_SIZE
      int BLOCK_SIZE = count;
#endif
#if 0
      partition_scalar(data, data, boundary, bin, 0, count, 0, bin_count);
#else
      float * buf1 = (float *)_mm_malloc(sizeof(float) * (BLOCK_SIZE), 64);
      for(i = 0; i+BLOCK_SIZE-1 < count; i+=BLOCK_SIZE){
          memcpy(buf1, data, BLOCK_SIZE*sizeof(float));
          partition_scalar(buf1, buf1, boundary,bin,  0, BLOCK_SIZE, 0, bin_count);
      }
      
      memcpy(buf1, data+i, (count-i)*sizeof(float));
      partition_scalar(buf1, buf1, boundary,bin,  0, count-i, 0, bin_count);
      
      _mm_free(buf1);
#endif
    return 0;
}
