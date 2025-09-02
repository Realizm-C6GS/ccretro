#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

// Print mpz_t in scientific notation without generating full string
void print_mpz_in_scientific_notation(const mpz_t value, int total_digits) {
    size_t len = mpz_sizeinbase(value, 10); // number of digits in base 10
    char *buf = malloc(total_digits + 2);
    gmp_snprintf(buf, total_digits + 2, "%Zd", value);
    if (len <= 1) {
        printf("%se+0", buf);
    } else {
        printf("%c.", buf[0]);
        for (int i = 1; i < total_digits && buf[i] != '\0'; i++) {
            putchar(buf[i]);
        }
        printf("e+%zu", len - 1);
    }
    free(buf);
}

// Collatz function, optimized (no new temporaries every odd step)
unsigned long long CalcCCopt(const mpz_t n) {
    unsigned long long iter = 0;
    mpz_t temp;
    mpz_init_set(temp, n);

    while (mpz_cmp_ui(temp, 1) > 0) {
        iter++;
        if (mpz_even_p(temp)) {
            mpz_tdiv_q_2exp(temp, temp, 1);  // divide by 2
        } else {
            mpz_addmul_ui(temp, temp, 2);    // temp = temp + 2*temp = 3n
            mpz_add_ui(temp, temp, 1);       // temp = 3n + 1
        }
    }

    mpz_clear(temp);
    return iter;
}

int main(int argc, char *argv[]) {
    mpz_t start, end, rangeSize, step;
    unsigned long long maxIter = 0;
    mpz_t numberWithMaxIter;
    mpz_init(numberWithMaxIter);

    unsigned int startPower = 64, endPower = 256;
    unsigned long long stepCount = 1000;

    // parse args
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-start") == 0 && i + 1 < argc) {
            startPower = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-end") == 0 && i + 1 < argc) {
            endPower = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-stepsize") == 0 && i + 1 < argc) {
            stepCount = atoll(argv[++i]);
        }
    }

    mpz_inits(start, end, rangeSize, step, NULL);

    double start_time = omp_get_wtime();

    for (unsigned int i = startPower; i <= endPower; ++i) {
        mpz_ui_pow_ui(start, 2, i);
        mpz_ui_pow_ui(end, 2, i + 1);
        mpz_sub(rangeSize, end, start);
        mpz_cdiv_q_ui(step, rangeSize, stepCount);

        gmp_printf("Range: 2^%u to 2^%u, step size: ", i, i + 1);
        print_mpz_in_scientific_notation(step, 8);
        printf("\n");

        // parallel region with thread-local buffers
        #pragma omp parallel
        {
            unsigned long long threadMaxIter = 0;
            mpz_t threadNumberWithMaxIter, local_current;
            mpz_inits(threadNumberWithMaxIter, local_current, NULL);

            #pragma omp for schedule(dynamic,8)
            for (unsigned long long j = 0; j < stepCount; ++j) {
                mpz_set(local_current, start);
                mpz_addmul_ui(local_current, step, j);

                if (mpz_cmp(local_current, end) > 0) continue;

                unsigned long long iter = CalcCCopt(local_current);

                if (iter > threadMaxIter) {
                    threadMaxIter = iter;
                    mpz_set(threadNumberWithMaxIter, local_current);
                }
            }

            // merge results
            #pragma omp critical
            {
                if (threadMaxIter > maxIter) {
                    maxIter = threadMaxIter;
                    mpz_set(numberWithMaxIter, threadNumberWithMaxIter);
                    gmp_printf("New longer sequence found: %Zd, sequence length = %llu\n",
                               threadNumberWithMaxIter, threadMaxIter);
                }
            }

            mpz_clears(threadNumberWithMaxIter, local_current, NULL);
        } // end parallel region
    }

    double end_time = omp_get_wtime();

    gmp_printf("Largest sequence length: %llu, found for number: %Zd\n",
               maxIter, numberWithMaxIter);
    printf("Total runtime: %.2f seconds\n", end_time - start_time);

    mpz_clears(start, end, rangeSize, step, numberWithMaxIter, NULL);
    return 0;
}
