#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> // Benchmarking timer

void print_mpz_in_scientific_notation(const mpz_t value, int total_digits) {
    mpz_t temp;
    char *str = NULL;

    mpz_init_set(temp, value);
    mpz_abs(temp, temp);

    str = mpz_get_str(NULL, 10, temp);

    size_t len = strlen(str);

    if (len <= 1) {
        printf("%se+0", str);
    } else {
        printf("%c.", str[0]);
        for (int i = 1; i < total_digits && i < len; i++) {
            printf("%c", str[i]);
        }
        printf("e+%zu", len - 1);
    }

    mpz_clear(temp);
    free(str);
}

unsigned long long CalcCCopt(const mpz_t n) {
    unsigned long long iter = 0;
    mpz_t temp;
    mpz_init_set(temp, n);

    while (mpz_cmp_ui(temp, 1) > 0) {
        iter++;
        if (mpz_even_p(temp)) {
            mpz_tdiv_q_2exp(temp, temp, 1);
        } else {
            mpz_mul_ui(temp, temp, 3);
            mpz_add_ui(temp, temp, 1);
        }
    }

    mpz_clear(temp);
    return iter;
}

int main(int argc, char *argv[]) {
    mpz_t start, end, current, numberWithMaxIter, rangeSize, step;
    unsigned long long maxIter = 0, iter;
    unsigned int startPower = 64, endPower = 256;
    unsigned long long stepCount = 1000; // Now using an integer type

    mpz_inits(start, end, current, numberWithMaxIter, rangeSize, step, NULL);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-start") == 0 && i + 1 < argc) {
            startPower = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-end") == 0 && i + 1 < argc) {
            endPower = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-stepsize") == 0 && i + 1 < argc) {
            stepCount = atoll(argv[++i]); // Accepting step count as an integer
        }
    }

    clock_t start_clock = clock(); // Start timing

    for (unsigned int i = startPower; i <= endPower; ++i) {
        mpz_ui_pow_ui(start, 2, i);
        mpz_ui_pow_ui(end, 2, i + 1);
        mpz_sub(rangeSize, end, start);
        mpz_cdiv_q_ui(step, rangeSize, stepCount);

        gmp_printf("Range: 2^%u to 2^%u, step size: ", i, i + 1); print_mpz_in_scientific_notation(step, 8);
        printf("\n");

        for (unsigned long long j = 0; j < stepCount; ++j) {
            mpz_t local_current;
            mpz_init_set(local_current, start);
            mpz_addmul_ui(local_current, step, j);
            if (mpz_cmp(local_current, end) > 0) {
                mpz_clear(local_current);
                continue;
            }

            iter = CalcCCopt(local_current);

            {
                if (iter > maxIter) {
                    maxIter = iter;
                    mpz_set(numberWithMaxIter, local_current);
                    gmp_printf("New longer sequence found: %Zd, sequence length = %llu\n", local_current, iter);
                }
            }
            mpz_clear(local_current);
        }
    }

    clock_t end_clock = clock(); // End timing
    double total_time = (double)(end_clock - start_clock) / CLOCKS_PER_SEC; // Calculate total runtime

    gmp_printf("Largest sequence length: %llu, found for number: %Zd\n", maxIter, numberWithMaxIter);
    printf("Total runtime: %.2f seconds\n", total_time);

    mpz_clears(start, end, current, numberWithMaxIter, rangeSize, step, NULL);
    return 0;
}
