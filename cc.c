// Unified Collatz Explorer
// Build modes:
//   RETRO  (-DRETRO): 64-bit only, cache + auto-train, exponents <= 20
//   MODERN (default): GMP + OpenMP, arbitrary precision
//
// Modern changes in this revision:
// - -nostep (alias -ustep) now parallel: threads share the full sweep by striding
//   n = 2^p + tid, n += T each loop (T = thread count).
// - Keeps hybrid Collatz (GMP + batched shifts + 64-bit fast path) — exact & faster.
// - Output remains compact; logging guarded with critical sections.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#ifdef RETRO
// ====================================================
// RETRO MODE (single-thread by design)
// ====================================================

static uint32_t *collatz_cache = NULL;
static unsigned  LIMIT         = 0;
static int       log_enable    = 0;
static int       nostep_enable = 0; // -nostep or -ustep

static void usage_retro(void){
    printf("Usage: ccretro.exe -start EXP -end EXP [-stepsize N|-nostep|-ustep] [-log]\n");
    printf("  EXP must be <= 20. Example: ccretro.exe -start 1 -end 20 -nostep -log\n");
}

static int is_p2(unsigned x){ return x && !(x&(x-1)); }

static int load_limit_from_cfg(void){
    FILE *f=fopen("train.cfg","r");
    if(!f) return 0;
    unsigned v=0; int ok=(fscanf(f,"%u",&v)==1); fclose(f);
    if(!ok || !is_p2(v) || v<(1u<<12) || v>(1u<<20)) return 0;
    LIMIT=v; printf("[retro] Loaded LIMIT=%u from train.cfg\n",LIMIT);
    return 1;
}
static void save_limit_to_cfg(void){
    FILE *f=fopen("train.cfg","w");
    if(f){ fprintf(f,"%u\n",LIMIT); fclose(f); printf("[retro] Wrote train.cfg (LIMIT=%u)\n",LIMIT); }
}

static void train_cache(void){
    printf("[retro] Training cache size...\n");
    double best=1e300; unsigned bestL=(1u<<12);
    for(unsigned e=12;e<=20;e++){
        unsigned L=(1u<<e);
        size_t bytes=(size_t)L*sizeof(uint32_t);
        uint32_t*buf=(uint32_t*)malloc(bytes);
        if(!buf) break;
        for(unsigned i=0;i<L;i++) buf[i]=0;
        const int passes=3;
        volatile uint32_t sink=0;
        clock_t t0=clock();
        for(int p=0;p<passes;p++){
            for(unsigned i=0;i<L;i++) buf[i]=(uint32_t)i;
            for(unsigned i=0;i<L;i++) sink+=buf[i];
        }
        clock_t t1=clock();
        double secs=(double)(t1-t0)/CLOCKS_PER_SEC;
        double score=secs/(passes*2.0*L);
        free(buf);
        if(score+score*0.01<best){ best=score; bestL=L; }
    }
    LIMIT=bestL;
    printf("[retro-train] Selected LIMIT=%u (%u MB)\n",
           LIMIT,(unsigned)((LIMIT*sizeof(uint32_t))/(1024u*1024u)));
    save_limit_to_cfg();
}

typedef struct { uint32_t v, steps_at_entry; } Node;
static uint32_t collatz_len_fill(uint64_t n){
    if(n<LIMIT && (collatz_cache[n] || n==1)) return collatz_cache[n];
    size_t cap=64,len=0;
    Node*stk=(Node*)malloc(cap*sizeof(Node));
    if(!stk){ fprintf(stderr,"[retro] OOM\n"); exit(1); }
    uint64_t cur=n; uint32_t steps=0;
    for(;;){
        if(cur<LIMIT && (collatz_cache[cur] || cur==1)){ steps+=collatz_cache[cur]; break; }
        if(cur<LIMIT){
            if(len==cap){ cap<<=1; Node*ns=(Node*)realloc(stk,cap*sizeof(Node));
                if(!ns){ free(stk); fprintf(stderr,"[retro] OOM\n"); exit(1);} stk=ns; }
            stk[len].v=(uint32_t)cur; stk[len].steps_at_entry=steps; len++;
        }
        if(cur==1) break;
        if(cur&1ULL) cur=3*cur+1; else cur>>=1;
        steps++;
    }
    const uint32_t total=steps;
    for(size_t i=0;i<len;i++){
        uint32_t v=stk[i].v;
        if(v<LIMIT && collatz_cache[v]==0) collatz_cache[v]=total-stk[i].steps_at_entry;
    }
    uint32_t r=(n<LIMIT)?collatz_cache[n]:total;
    free(stk);
    return r;
}

static void build_cache(void){
    if(LIMIT==0){ fprintf(stderr,"[retro] LIMIT==0\n"); exit(1); }
    free(collatz_cache);
    collatz_cache=(uint32_t*)calloc(LIMIT,sizeof(uint32_t));
    if(!collatz_cache){ fprintf(stderr,"[retro] cache alloc fail\n"); exit(1); }
    collatz_cache[1]=0;
    for(uint32_t i=2;i<LIMIT;i++) if(collatz_cache[i]==0) (void)collatz_len_fill(i);
}

static uint64_t collatz64(uint64_t n){
    if(n==1) return 0;
    uint64_t it=0;
    for(;;){
        if(n<LIMIT){
            if(collatz_cache[n]==0 && n!=1) it+=collatz_len_fill(n);
            else it+=collatz_cache[n];
            break;
        }
#if defined(__SIZEOF_INT128__)
        if(n&1ULL){
            __uint128_t w=(__uint128_t)3*n+1;
            if(w>UINT64_MAX){ fprintf(stderr,"[retro] Overflow at %llu\n",(unsigned long long)n); return it; }
            n=(uint64_t)w;
        } else n>>=1;
#else
        if(n&1ULL){
            if(n>(UINT64_MAX-1ULL)/3ULL){ fprintf(stderr,"[retro] Overflow at %llu\n",(unsigned long long)n); return it; }
            n=3*n+1;
        } else n>>=1;
#endif
        it++; if(n==1) break;
    }
    return it;
}

int main(int argc,char**argv){
    unsigned startP=1,endP=20; unsigned long long stepCount=1000ULL;
    if(argc==1){ usage_retro(); return 0; }
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"-start")==0&&i+1<argc) startP=(unsigned)atoi(argv[++i]);
        else if(strcmp(argv[i],"-end")==0&&i+1<argc) endP=(unsigned)atoi(argv[++i]);
        else if(strcmp(argv[i],"-stepsize")==0&&i+1<argc){ stepCount=(unsigned long long)atoll(argv[++i]); if(stepCount==0) stepCount=1; }
        else if(strcmp(argv[i],"-log")==0) log_enable=1;
        else if(strcmp(argv[i],"-nostep")==0 || strcmp(argv[i],"-ustep")==0) nostep_enable=1;
    }
    if(endP<startP){ unsigned t=startP; startP=endP; endP=t; }
    if(startP>20||endP>20){ fprintf(stderr,"[retro] ERROR: exponent too large. Max=20\n"); return 1; }

    if(!load_limit_from_cfg()){ printf("[retro] train.cfg missing or invalid -> retraining\n"); train_cache(); }
    build_cache();

    if(nostep_enable) printf("[retro] step=none (full sweep)\n");
    else              printf("[retro] steps=%llu (samples per range)\n",(unsigned long long)stepCount);

    FILE*lf=NULL; if(log_enable) lf=fopen("cc.log","a");

    uint64_t maxIter=0,maxN=0;
    for(unsigned p=startP;p<=endP;p++){
        uint64_t base=1ULL<<p, end=(1ULL<<(p+1)), range=end-base;
        uint64_t step=nostep_enable?1: range/stepCount; if(step==0) step=1;
        printf("[retro] Range 2^%u..2^%u\n",p,p+1);

        if(nostep_enable){
            for(uint64_t n=base;n<end;n++){
                uint64_t it=collatz64(n);
                if(it>maxIter){ maxIter=it; maxN=n;
                    printf("[retro] New longest: n=%llu length=%llu\n",(unsigned long long)maxN,(unsigned long long)maxIter);
                    if(lf) fprintf(lf,"%llu,%llu\n",(unsigned long long)maxN,(unsigned long long)maxIter);
                }
            }
        } else {
            uint64_t seeds=range/step; if(seeds==0) seeds=1;
            for(uint64_t j=0;j<seeds;j++){
                uint64_t n=base + j*step; if(n>=end) break;
                uint64_t it=collatz64(n);
                if(it>maxIter){ maxIter=it; maxN=n;
                    printf("[retro] New longest: n=%llu length=%llu\n",(unsigned long long)maxN,(unsigned long long)maxIter);
                    if(lf) fprintf(lf,"%llu,%llu\n",(unsigned long long)maxN,(unsigned long long)maxIter);
                }
            }
        }
    }
    if(lf) fclose(lf);
    printf("[retro] Longest length=%llu at n=%llu\n",(unsigned long long)maxIter,(unsigned long long)maxN);
    free(collatz_cache);
    return 0;
}

#else
// ====================================================
// MODERN MODE (GMP + OpenMP, -nostep now parallel)
// ====================================================
#include <gmp.h>
#include <omp.h>

static int log_enable=0, nostep_enable=0;

static void usage_modern(void){
    printf("Usage: cc.exe -start EXP -end EXP [-stepsize N|-nostep|-ustep] [-log]\n");
    printf("  EXP can be large. Example: cc.exe -start 8192 -end 8192 -stepsize 50000\n");
}

// Compact printer: small ints fully, big in scientific.
static void print_scientific(const mpz_t val, int digits){
    size_t len=mpz_sizeinbase(val,10);
    if(len<=6){ gmp_printf("%Zd",val); return; }
    mpz_t tmp,p10; mpz_inits(tmp,p10,NULL); mpz_set(tmp,val);
    size_t drop=(len>(size_t)digits? len-(size_t)digits:0);
    if(drop){ mpz_ui_pow_ui(p10,10,(unsigned)drop); mpz_tdiv_q(tmp,tmp,p10); }
    char*buf=mpz_get_str(NULL,10,tmp);
    if(buf&&buf[0]){ putchar(buf[0]); putchar('.'); for(int i=1;i<digits&&buf[i];i++) putchar(buf[i]); printf("e+%zu",len-1); }
    else printf("0e+0");
    free(buf); mpz_clears(tmp,p10,NULL);
}

// ---------- 64-bit helper (batched) ----------
#if defined(__GNUC__) || defined(__clang__)
# define CTTZ64(x) __builtin_ctzll((unsigned long long)(x))
#else
static inline int CTTZ64_f(uint64_t x){ int c=0; while((x&1ULL)==0){ x>>=1; c++; } return c; }
# define CTTZ64(x) CTTZ64_f(x)
#endif

static inline unsigned long long collatz64_batched(uint64_t *n_inout){
    uint64_t n = *n_inout;
    if(n<=1) return 0ULL;
    unsigned long long it=0ULL;
    while(n>1){
        if((n&1ULL)==0){
            int tz = CTTZ64(n);
            n >>= tz; it += (unsigned long long)tz;
        }else{
#if defined(__SIZEOF_INT128__)
            __uint128_t w = ( (__uint128_t)3*n + 1 );
            if(w > UINT64_MAX){ *n_inout = n; return it; } // bounce to GMP
            uint64_t v = (uint64_t)w;
            int tz = CTTZ64(v);
            v >>= tz;
            n = v;
            it += 1ULL + (unsigned long long)tz; // 3*n+1, then tz halvings
#else
            if(n > (UINT64_MAX-1ULL)/3ULL){ *n_inout = n; return it; }
            uint64_t v = 3*n + 1;
            int tz = CTTZ64(v);
            v >>= tz;
            n = v;
            it += 1ULL + (unsigned long long)tz; // 3*n+1, then tz halvings
#endif
        }
    }
    *n_inout = n;
    return it;
}

// ---------- Hybrid Collatz length ----------
static unsigned long long CollatzLenHybrid(mpz_t scratch, const mpz_t n_in){
    mpz_set(scratch, n_in);
    unsigned long long it = 0ULL;

    for(;;){
        if(mpz_cmp_ui(scratch, 0) > 0 && mpz_sizeinbase(scratch, 2) <= 64){
            uint64_t n64 = mpz_get_ui(scratch);
            unsigned long long add = collatz64_batched(&n64);
            it += add;
            if(n64==1) return it;
            mpz_set_ui(scratch, n64);
        }

        if(mpz_cmp_ui(scratch,1) <= 0) return it;

        if(mpz_even_p(scratch)){
            size_t tz = mpz_scan1(scratch, 0);
            mpz_tdiv_q_2exp(scratch, scratch, tz);
            it += (unsigned long long)tz;
        } else {
            mpz_mul_ui(scratch, scratch, 3);
            mpz_add_ui(scratch, scratch, 1);
            size_t tz = mpz_scan1(scratch, 0);
            mpz_tdiv_q_2exp(scratch, scratch, tz);
            it += 1ULL + (unsigned long long)tz; // 3*n+1, then tz halvings
        }
    }
}

int main(int argc,char**argv){
    unsigned startP=1,endP=20; unsigned long long stepCount=1000ULL;

    if(argc==1){ usage_modern(); return 0; }
    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"-start")==0 && i+1<argc) startP=(unsigned)atoi(argv[++i]);
        else if(strcmp(argv[i],"-end")==0 && i+1<argc) endP=(unsigned)atoi(argv[++i]);
        else if(strcmp(argv[i],"-stepsize")==0 && i+1<argc){ stepCount=(unsigned long long)atoll(argv[++i]); if(stepCount==0) stepCount=1; }
        else if(strcmp(argv[i],"-log")==0) log_enable=1;
        else if(strcmp(argv[i],"-nostep")==0 || strcmp(argv[i],"-ustep")==0) nostep_enable=1;
    }
    if(endP<startP){ unsigned t=startP; startP=endP; endP=t; }

    if(nostep_enable) printf("[modern] step=none (full sweep)\n");
    else              printf("[modern] steps=%llu (samples per range)\n",(unsigned long long)stepCount);

    FILE*lf=NULL; if(log_enable) lf=fopen("cc.log","a");

    mpz_t start,end,range,step,maxNum;
    unsigned long long maxIter=0ULL;
    mpz_inits(start,end,range,step,maxNum,NULL);

    double t0=omp_get_wtime();
    for(unsigned i=startP;i<=endP;i++){
        mpz_ui_pow_ui(start,2,i);
        mpz_ui_pow_ui(end,2,i+1);
        mpz_sub(range,end,start);

        printf("[modern] Range 2^%u..2^%u\n", i, i+1);

        if(nostep_enable){
            // Parallel full sweep using round-robin striding across threads:
            // Thread tid handles n = start + tid, then n += T each time.
            #pragma omp parallel
            {
                int tid = omp_get_thread_num();
                int T   = omp_get_num_threads();
                mpz_t cur, scratch; mpz_inits(cur, scratch, NULL);

                // cur = start + tid
                if(tid==0){
                    // ensure T is visible; nothing else to do
                }
                mpz_set(cur, start);
                if(tid>0) mpz_add_ui(cur, cur, (unsigned long)tid);

                unsigned long long tMaxIter = 0ULL;
                mpz_t tMaxNum; mpz_init(tMaxNum);

                // stride = T
                while(mpz_cmp(cur, end) < 0){
                    unsigned long long it = CollatzLenHybrid(scratch, cur);
                    if(it > tMaxIter){
                        tMaxIter = it; mpz_set(tMaxNum, cur);
                    }
                    mpz_add_ui(cur, cur, (unsigned long)T);
                }

                #pragma omp critical
                {
                    if(tMaxIter > maxIter){
                        maxIter = tMaxIter; mpz_set(maxNum, tMaxNum);
                        printf("[modern] New longest: "); print_scientific(maxNum,10);
                        printf(", length=%llu\n", maxIter);
                        if(lf) gmp_fprintf(lf, "%Zd,%llu\n", maxNum, maxIter);
                    }
                }

                mpz_clears(cur, scratch, tMaxNum, NULL);
            }
        } else {
            // Sample exactly stepCount seeds per range
            if(stepCount==0) stepCount=1;
            mpz_cdiv_q_ui(step, range, (unsigned long)stepCount);
            if(mpz_cmp_ui(step,0)==0) mpz_set_ui(step,1);

            #pragma omp parallel
            {
                unsigned long long tMaxIter=0ULL;
                mpz_t tMaxNum, cur, scratch; mpz_inits(tMaxNum, cur, scratch, NULL);

                #pragma omp for schedule(guided,8)
                for(unsigned long long j=0;j<stepCount;j++){
                    mpz_set(cur,start);
                    mpz_addmul_ui(cur, step, (unsigned long)j); // cur = start + j*step
                    if(mpz_cmp(cur,end)>=0) continue;

                    unsigned long long it=CollatzLenHybrid(scratch, cur);
                    if(it>tMaxIter){ tMaxIter=it; mpz_set(tMaxNum,cur); }
                }

                #pragma omp critical
                {
                    if(tMaxIter>maxIter){
                        maxIter=tMaxIter; mpz_set(maxNum,tMaxNum);
                        printf("[modern] New longest: "); print_scientific(maxNum,10);
                        printf(", length=%llu\n",maxIter);
                        if(lf) gmp_fprintf(lf,"%Zd,%llu\n",maxNum,maxIter);
                    }
                }
                mpz_clears(tMaxNum, cur, scratch, NULL);
            }
        }
    }
    double t1=omp_get_wtime();

    printf("[modern] Longest length=%llu at n=",maxIter); print_scientific(maxNum,12);
    printf("\n[modern] Runtime %.2f seconds\n", t1-t0);

    mpz_clears(start,end,range,step,maxNum,NULL);
    if(lf) fclose(lf);
    return 0;
}
#endif
