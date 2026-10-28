/* Include the production static helpers without changing their visibility. */
#define main ccretro_main
#include "../cc.c"
#undef main

/* Deliberately count one transition per iteration, without batching. */
static unsigned long long reference(mpz_t n, int stop_at_overflow){
    unsigned long long steps=0;
    mpz_t next;
    mpz_init(next);
    while(mpz_cmp_ui(n,1)>0){
        if(mpz_odd_p(n)){
            mpz_mul_ui(next,n,3);
            mpz_add_ui(next,next,1);
            if(stop_at_overflow && mpz_sizeinbase(next,2)>64) break;
            mpz_set(n,next);
        } else mpz_tdiv_q_2exp(n,n,1);
        steps++;
    }
    mpz_clear(next);
    return steps;
}

static int check(const mpz_t input, unsigned long long known, int has_known){
    mpz_t expected_state,scratch;
    mpz_inits(expected_state,scratch,NULL);
    mpz_set(expected_state,input);
    unsigned long long expected=reference(expected_state,0);
    unsigned long long actual=CollatzLenHybrid(scratch,input);
    int failed=(actual!=expected || (has_known && expected!=known));
    if(failed) gmp_fprintf(stderr,"hybrid n=%Zd: got %llu, expected %llu (known %llu)\n",
                          input,actual,expected,known);
    if(mpz_sizeinbase(input,2)<=64){
        uint64_t n64=mpz_get_ui(input);
        actual=collatz64_batched(&n64);
        mpz_set(expected_state,input);
        expected=reference(expected_state,1);
        mpz_set_ui(scratch,n64);
        if(actual!=expected || mpz_cmp(scratch,expected_state)!=0){
            gmp_fprintf(stderr,"64-bit n=%Zd: got %llu ending at %Zd; expected %llu at %Zd\n",
                        input,actual,scratch,expected,expected_state);
            failed=1;
        }
    }
    mpz_clears(expected_state,scratch,NULL);
    return failed;
}

int main(void){
    int failures=0,threads=0;
    const unsigned long seeds[]={1,2,3,7,27};
    const unsigned long long lengths[]={0,1,7,16,111};
    mpz_t n;
    mpz_init(n);
    for(size_t i=0;i<sizeof(seeds)/sizeof(seeds[0]);i++){
        mpz_set_ui(n,seeds[i]);
        failures+=check(n,lengths[i],1);
    }
    /* UINT64_MAX, the 3*n+1 overflow boundary, and a delayed overflow. */
    const char *boundaries[]={
        "6148914691236517203", "6148914691236517205",
        "9223372036854775808", "12297829382473034410",
        "18446744073709551615", "18446744073709551616",
        "18446744073709551617"
    };
    for(size_t i=0;i<sizeof(boundaries)/sizeof(boundaries[0]);i++){
        mpz_set_str(n,boundaries[i],10);
        failures+=check(n,0,0);
    }
    for(unsigned p=128;p<=1024;p*=2){
        mpz_ui_pow_ui(n,2,p);
        failures+=check(n,p,1);              /* GMP even path, then 64-bit. */
        mpz_mul_ui(n,n,27);
        failures+=check(n,p+111,1);
        mpz_ui_pow_ui(n,2,p);
        mpz_sub_ui(n,n,1);
        failures+=check(n,0,0);              /* Repeated GMP odd steps. */
        mpz_add_ui(n,n,2);
        failures+=check(n,0,0);
    }
    mpz_clear(n);

    /* Each worker owns its GMP state, as in the benchmark. */
    #pragma omp parallel reduction(+:failures)
    {
        mpz_t input;
        mpz_init(input);
        #pragma omp single
        threads=omp_get_num_threads();
        #pragma omp for schedule(guided,8)
        for(unsigned i=1;i<=10000;i++){
            mpz_set_ui(input,i);
            failures+=check(input,0,0);
            mpz_mul_2exp(input,input,128);
            mpz_add_ui(input,input,1);
            failures+=check(input,0,0);
        }
        mpz_clear(input);
    }
    if(failures){ fprintf(stderr,"FAIL: %d cases\n",failures); return 1; }
    printf("PASS: known lengths, boundaries, GMP cases and 20000 reference comparisons (%d threads)\n",threads);
    return 0;
}
