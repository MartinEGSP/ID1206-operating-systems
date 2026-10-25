#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s N_pages init(0|1)\n", argv[0]);
        return 1;
    }

    long N = strtol(argv[1], NULL, 10);
    int init = atoi(argv[2]);

    if (N <= 0 || (init != 0 && init != 1)) {
        fprintf(stderr, "Error: N_pages must be > 0 and init must be 0 or 1\n");
        return 1;
    } 
 
    long page_size = getpagesize();
    size_t bytes = (size_t)N * (size_t)page_size;

    printf("Page size: %ld bytes\n", page_size);
    printf("Allocating: N=%ld pages => %zu bytes\n", N, bytes);
    printf("Init: %s\n", init ? "yes (touch pages)" : "no (no touching)");

    char *buf = (char*)malloc(bytes);
    if (!buf) {
        perror("malloc");
        return 1;
    }

    if (init) {
        // Touch all pages (forces physical allocation on demand-paged systems)
        memset(buf, 1, bytes);
    }

    // Keep program alive briefly and also prevent compiler optimizing away buf
    volatile unsigned long sum = 0;
    for (size_t i = 0; i < bytes; i += (size_t)page_size) sum += (unsigned char)buf[i];
    printf("Touch-sum: %lu\n", sum);

    free(buf);
    return 0;
}
