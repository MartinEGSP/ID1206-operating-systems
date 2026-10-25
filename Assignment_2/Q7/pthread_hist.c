#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

int num_threads;
int N;
double *a;

#define BINS 30

int **local_hist;
int final_hist[BINS];

void *thread_func(void *arg)
{
    int id = *(int*)arg;
    free(arg);

    int chunk = N / num_threads;
    int start = id * chunk;
    int end = (id == num_threads - 1) ? N : start + chunk;

    for (int i = 0; i < BINS; i++)
        local_hist[id][i] = 0;

    for (int i = start; i < end; i++) {
        int bin = (int)(a[i] * BINS);
        if (bin >= BINS) bin = BINS - 1;
        local_hist[id][bin]++;
    }

    pthread_exit(0);
}

double elapsed_ms(struct timeval t1, struct timeval t2)
{
    return (t2.tv_sec - t1.tv_sec) * 1000.0 +
           (t2.tv_usec - t1.tv_usec) / 1000.0;
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage: %s num_threads N\n", argv[0]);
        return 1;
    }

    num_threads = atoi(argv[1]);
    N = atoi(argv[2]);

    a = malloc(N * sizeof(double));
    local_hist = malloc(num_threads * sizeof(int*));

    for (int i = 0; i < num_threads; i++)
        local_hist[i] = malloc(BINS * sizeof(int));

    for (int i = 0; i < N; i++)
        a[i] = (double)rand() / RAND_MAX;

    struct timeval t1, t2;

    /* serial histogram */
    gettimeofday(&t1, NULL);
    for (int i = 0; i < BINS; i++)
        final_hist[i] = 0;

    for (int i = 0; i < N; i++) {
        int bin = (int)(a[i] * BINS);
        if (bin >= BINS) bin = BINS - 1;
        final_hist[bin]++;
    }
    gettimeofday(&t2, NULL);
    double serial_time = elapsed_ms(t1, t2);

    /* parallel histogram */
    pthread_t *workers = malloc(num_threads * sizeof(pthread_t));

    gettimeofday(&t1, NULL);

    for (int i = 0; i < num_threads; i++) {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_create(&workers[i], &attr, thread_func, id);
    }

    for (int i = 0; i < num_threads; i++)
        pthread_join(workers[i], NULL);

    for (int b = 0; b < BINS; b++)
        final_hist[b] = 0;

    for (int t = 0; t < num_threads; t++)
        for (int b = 0; b < BINS; b++)
            final_hist[b] += local_hist[t][b];

    gettimeofday(&t2, NULL);
    double parallel_time = elapsed_ms(t1, t2);

    printf("N = %d, threads = %d\n", N, num_threads);
    printf("Serial time   = %.3f ms\n", serial_time);
    printf("Parallel time = %.3f ms\n", parallel_time);
    printf("Histogram:\n");

    for (int i = 0; i < BINS; i++)
        printf("bin %2d: %d\n", i, final_hist[i]);

    free(workers);
    for (int i = 0; i < num_threads; i++)
        free(local_hist[i]);
    free(local_hist);
    free(a);

    return 0;
}
