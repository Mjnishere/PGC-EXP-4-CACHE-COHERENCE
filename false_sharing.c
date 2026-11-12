#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>

#define N 100000000L

int main(int argc, char **argv) {
    int padded = (argc > 1 && argv[1][0] == 'p');
    int stride = padded ? 8 : 1; /* 8 longs = 64 bytes */
    int nt = omp_get_max_threads();
    size_t bytes = ((size_t)nt * stride * sizeof(long) + 63) / 64 * 64;
    long *cnt = aligned_alloc(64, bytes);

    if (cnt == NULL) {
        perror("aligned_alloc");
        return 1;
    }
    memset(cnt, 0, bytes);

    double start = omp_get_wtime();
    #pragma omp parallel
    {
        volatile long *counter =
            &cnt[omp_get_thread_num() * stride];
        for (long i = 0; i < N / nt; i++) {
            (*counter)++;
        }
    }
    double elapsed = omp_get_wtime() - start;

    long total = 0;
    for (int i = 0; i < nt; i++) {
        total += cnt[i * stride];
    }

    printf("%s | threads=%d | total=%ld | time=%.4f s\n",
           padded ? "padded " : "adjacent", nt, total, elapsed);
    free(cnt);
    return total == N ? 0 : 2;
}
