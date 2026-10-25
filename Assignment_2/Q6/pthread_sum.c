#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>

#define N 1000000

int num_threads = 0;
float *a;
float serial_sum = 0.0f;
float parallel_sum = 0.0f;
float *partial_sums;

void *thread_func(void *arg); /* the thread function */

double elapsed_ms(struct timeval start, struct timeval end)
{
    return (end.tv_sec - start.tv_sec) * 1000.0 +
           (end.tv_usec - start.tv_usec) / 1000.0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s num_threads\n", argv[0]);
        return 1;
    }

    num_threads = atoi(argv[1]);

    a = malloc(N * sizeof(float));
    partial_sums = malloc(num_threads * sizeof(float));
    if (!a || !partial_sums) {
        perror("malloc");
        return 1;
    }

    /* Initialize an array of random values */
    for (int i = 0; i < N; i++)
        a[i] = (float)rand() / RAND_MAX;

    struct timeval t1, t2;

    /* Serial sum */
    gettimeofday(&t1, NULL);
    serial_sum = 0.0f;
    for (int i = 0; i < N; i++)
        serial_sum += a[i];
    gettimeofday(&t2, NULL);
    double serial_time = elapsed_ms(t1, t2);

    /* Parallel sum */
    pthread_t *workers = malloc(num_threads * sizeof(pthread_t));
    if (!workers) {
        perror("malloc");
        return 1;
    }

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

    parallel_sum = 0.0f;
    for (int i = 0; i < num_threads; i++)
        parallel_sum += partial_sums[i];

    gettimeofday(&t2, NULL);
    double parallel_time = elapsed_ms(t1, t2);

    printf("N = %d, threads = %d\n", N, num_threads);
    printf("Serial sum   = %f, time = %.3f ms\n", serial_sum, serial_time);
    printf("Parallel sum = %f, time = %.3f ms\n", parallel_sum, parallel_time);

    free(workers);
    free(a);
    free(partial_sums);
    return 0;
}

void *thread_func(void *arg)
{
    /* Assign each thread an id so that they are unique in range [0, num_thread -1 ] */
    int my_id = *(int *)arg;
    free(arg);

    int chunk = N / num_threads;
    int start = my_id * chunk;
    int end = (my_id == num_threads - 1) ? N : start + chunk;

    /* Perform Partial Parallel Sum Here */
    float my_sum = 0.0f;
    for (int i = start; i < end; i++)
        my_sum += a[i];

    partial_sums[my_id] = my_sum;

    printf("Thread %d sum = %f\n", my_id, my_sum);
    pthread_exit(0);
}
