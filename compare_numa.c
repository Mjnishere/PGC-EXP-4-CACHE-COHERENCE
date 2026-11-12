#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#define N 100000000L
#define MB 512UL

static double run_counters(int nt, int stride, long *total) {
    size_t bytes = ((size_t)nt * stride * sizeof(long) + 63) / 64 * 64;
    long *cnt = aligned_alloc(64, bytes);
    if (cnt == NULL) {
        perror("aligned_alloc");
        exit(EXIT_FAILURE);
    }
    memset(cnt, 0, bytes);
    double start = omp_get_wtime();

    #pragma omp parallel num_threads(nt)
    {
        volatile long *counter =
            &cnt[omp_get_thread_num() * stride];
        for (long i = 0; i < N / nt; i++) {
            (*counter)++;
        }
    }

    double elapsed = omp_get_wtime() - start;
    *total = 0;
    for (int i = 0; i < nt; i++) {
        *total += cnt[i * stride];
    }
    free(cnt);
    return elapsed;
}

static double run_sweep(char *buffer, size_t n, int nt) {
    long sum = 0;
    double start = omp_get_wtime();

    for (int pass = 0; pass < 4; pass++) {
        #pragma omp parallel for num_threads(nt) reduction(+:sum)
        for (size_t i = 0; i < n; i += 64) {
            sum += buffer[i];
        }
    }

    if (sum == 0) {
        fprintf(stderr, "Unexpected zero checksum.\n");
    }
    return omp_get_wtime() - start;
}

int main(void) {
    const int thread_counts[] = {1, 2, 4};

    printf("Table 1: false sharing (%ld increments in total)\n", N);
    printf("Layout   | Threads | Final Count | Correctness | Time (s)\n");
    printf("---------------------------------------------------------\n");

    for (int i = 0; i < 3; i++) {
        for (int stride = 1; stride <= 8; stride *= 8) {
            long total;
            int nt = thread_counts[i];
            double elapsed = run_counters(nt, stride, &total);
            printf("%-8s | %7d | %11ld | %-11s | %8.4f\n",
                   stride == 1 ? "adjacent" : "padded",
                   nt, total, total == N ? "Correct" : "INCORRECT",
                   elapsed);
        }
        puts("---------------------------------------------------------");
    }

    size_t n = MB << 20;
    char *buffer = malloc(n);
    if (buffer == NULL) {
        perror("malloc");
        return 1;
    }
    memset(buffer, 1, n); /* First-touch placement. */

    printf("\nTable 2: memory sweep of %lu MB (4 passes)\n", MB);
    printf("Threads | Time (s)\n");
    printf("-------------------\n");
    for (int i = 0; i < 2; i++) {
        int nt = thread_counts[i];
        printf("%7d | %8.4f\n", nt, run_sweep(buffer, n, nt));
    }

    free(buffer);
    return 0;
}
