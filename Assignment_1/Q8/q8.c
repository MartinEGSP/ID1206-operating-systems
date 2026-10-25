#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#define FIFO1 "/tmp/q8_sum1"
#define FIFO2 "/tmp/q8_sum2"

static double getSum(double *x, long start, long end) {
    double s = 0.0;
    for (long i = start; i < end; i++) s += x[i];
    return s;
}

static double tdiff(struct timespec a, struct timespec b) {
    return (b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec)/1e9;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s N\n", argv[0]);
        return 1;
    }

    long N = strtol(argv[1], NULL, 10);
    if (N <= 0) {
        fprintf(stderr, "N must be > 0\n");
        return 1;
    }

    double *a = malloc(sizeof(double) * (size_t)N);
    if (!a) {
        perror("malloc");
        return 1;
    }

    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)getpid();
    for (long i = 0; i < N; i++)
        a[i] = (double)rand_r(&seed)/RAND_MAX;

    mkfifo(FIFO1, 0666);
    mkfifo(FIFO2, 0666);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    pid_t c1 = fork();
    if (c1 == 0) {
        double s = getSum(a, 0, N/2);
        int fd = open(FIFO1, O_WRONLY);
        if (fd >= 0) {
            write(fd, &s, sizeof s);
            close(fd);
        }
        _exit(0);
    }

    pid_t c2 = fork();
    if (c2 == 0) {
        double s = getSum(a, N/2, N);
        int fd = open(FIFO2, O_WRONLY);
        if (fd >= 0) {
            write(fd, &s, sizeof s);
            close(fd);
        }
        _exit(0);
    }

    double s1 = 0.0, s2 = 0.0;
    int fdr1 = open(FIFO1, O_RDONLY);
    int fdr2 = open(FIFO2, O_RDONLY);
    if (fdr1 >= 0) { read(fdr1, &s1, sizeof s1); close(fdr1); }
    if (fdr2 >= 0) { read(fdr2, &s2, sizeof s2); close(fdr2); }

    waitpid(c1, NULL, 0);
    waitpid(c2, NULL, 0);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double total = s1 + s2;
    double dt = tdiff(t0, t1);

    printf("N = %ld\nSum = %.9f\nTime = %.6f s\n", N, total, dt);

    unlink(FIFO1);
    unlink(FIFO2);
    free(a);
    return 0;
}
