#include <pthread.h> 
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

typedef struct {
    off_t offset;
    size_t bytes;
} req_t;

typedef struct {
    int fd;
    req_t *reqs;
    int start_idx;
    int count;
    char *buf;
} thread_arg_t;

static double timespec_diff_sec(struct timespec a, struct timespec b) {
    return (b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) / 1e9;
}

void *reader_thread_func(void *input_parameters) { 
     
     thread_arg_t *arg = (thread_arg_t *)input_parameters;

     for (int i = 0; i < arg->count; i++) {
          req_t r = arg->reqs[arg->start_idx + i];
          ssize_t got = pread(arg->fd, arg->buf, r.bytes, r.offset);
          if (got < 0) {
               perror("pread");
               break;
          }
     }

     pthread_exit(0);
}


void *writer_thread_func(void *input_parameters) { 
     
     thread_arg_t *arg = (thread_arg_t *)input_parameters;

     for (int i = 0; i < arg->count; i++) {
          req_t r = arg->reqs[arg->start_idx + i];
          ssize_t wrote = pwrite(arg->fd, arg->buf, r.bytes, r.offset);
          if (wrote < 0) {
               perror("pwrite");
               break;
          }
     }

     pthread_exit(0);
}


int main(int argc, char *argv[])
{
     int n = atoi(argv[1]); //number of bytes
     int p = atoi(argv[2]); //number of threads

     if (argc != 3 || n <= 0 || p <= 0) {
          printf("Usage: %s <N bytes> <P threads>\n", argv[0]);
          return 1;
     }

     // create a file for saving the data
     int fd = open("testfile.dat", O_CREAT | O_RDWR | O_TRUNC, 0666);
     if (fd < 0) {
          perror("open");
          return 1;
     }

     // allocate a buffer and initialize it
     size_t max_req_bytes = 16384;
     char *buf = (char *)malloc(max_req_bytes);
     if (!buf) {
          perror("malloc");
          close(fd);
          return 1;
     }
     memset(buf, 'A', max_req_bytes);

     // create two lists of 100 requests in the format of [offset, bytes]
     int req_count = 100;

     // List 1: sequential requests of 16384 bytes, where offset_n = offset_(n-1) + 16384
     // ensure no overlapping among these requests.
     size_t list1_bytes = 16384;
     int max_list1 = n / (int)list1_bytes;
     if (max_list1 < req_count) req_count = max_list1;
     if (req_count <= 0) {
          printf("N is too small for List1 (need at least 16384 bytes).\n");
          free(buf);
          close(fd);
          return 1;
     }

     req_t *list1 = (req_t *)malloc(sizeof(req_t) * req_count);
     req_t *list2 = (req_t *)malloc(sizeof(req_t) * req_count);
     if (!list1 || !list2) {
          perror("malloc");
          free(buf);
          close(fd);
          free(list1);
          free(list2);
          return 1;
     }

     for (int i = 0; i < req_count; i++) {
          list1[i].offset = (off_t)i * (off_t)list1_bytes;
          list1[i].bytes  = list1_bytes;
     }

     // List 2: random requests of 128 bytes, where offset_n = random[0,N/4096] * 4096
     // ensure no overlapping among these requests.
     size_t list2_bytes = 128;
     int blocks = n / 4096;
     if (blocks < req_count) req_count = blocks;   // adjust if too few distinct 4KB blocks
     if (req_count <= 0) {
          printf("N is too small for List2 (need at least 4096 bytes).\n");
          free(buf);
          close(fd);
          free(list1);
          free(list2);
          return 1;
     }

     // create a simple unique random selection of block indices [0..blocks-1]
     int *idx = (int *)malloc(sizeof(int) * blocks);
     if (!idx) {
          perror("malloc");
          free(buf);
          close(fd);
          free(list1);
          free(list2);
          return 1;
     }
     for (int i = 0; i < blocks; i++) idx[i] = i;

     srand((unsigned int)time(NULL));
     for (int i = blocks - 1; i > 0; i--) {
          int j = rand() % (i + 1);
          int tmp = idx[i];
          idx[i] = idx[j];
          idx[j] = tmp;
     }

     for (int i = 0; i < req_count; i++) {
          list2[i].offset = (off_t)idx[i] * (off_t)4096;
          list2[i].bytes  = list2_bytes;
     }
     free(idx);

     pthread_t *threads = (pthread_t *)malloc(sizeof(pthread_t) * p);
     thread_arg_t *targs = (thread_arg_t *)malloc(sizeof(thread_arg_t) * p);
     if (!threads || !targs) {
          perror("malloc");
          free(buf);
          close(fd);
          free(list1);
          free(list2);
          free(threads);
          free(targs);
          return 1;
     }

     // ---------------------- LIST 1 WRITE/READ ----------------------
     // start timing 
     struct timespec t0, t1;
     clock_gettime(CLOCK_MONOTONIC, &t0);

     /* Create writer workers and pass in their portion of list1 */   
     int base = req_count / p;
     int rem  = req_count % p;
     int cur  = 0;

     for (int i = 0; i < p; i++) {
          int cnt = base + (i < rem ? 1 : 0);
          targs[i].fd = fd;
          targs[i].reqs = list1;
          targs[i].start_idx = cur;
          targs[i].count = cnt;
          targs[i].buf = buf;
          cur += cnt;

          if (pthread_create(&threads[i], NULL, writer_thread_func, &targs[i]) != 0) {
               perror("pthread_create");
               // best-effort: join what was created so far
               for (int k = 0; k < i; k++) pthread_join(threads[k], NULL);
               free(buf); close(fd); free(list1); free(list2); free(threads); free(targs);
               return 1;
          }
     }

     /* Wait for all writers to finish */ 
     for (int i = 0; i < p; i++) pthread_join(threads[i], NULL);

     // close the file 
     close(fd);

     // end timing 
     clock_gettime(CLOCK_MONOTONIC, &t1);

     double elapsed = timespec_diff_sec(t0, t1);
     double total_bytes = (double)req_count * (double)list1_bytes;
     double total_mb = total_bytes / (1024.0 * 1024.0);
     double bw = total_mb / elapsed;

     // Print out the write bandwidth
     printf("Write %f MB, use %d threads, elapsed time %f s, write bandwidth: %f MB/s \n",
            total_mb, p, elapsed, bw);
     
     
     // reopen the file 
     fd = open("testfile.dat", O_RDONLY);
     if (fd < 0) {
          perror("open");
          free(buf);
          free(list1);
          free(list2);
          free(threads);
          free(targs);
          return 1;
     }

     // start timing 
     clock_gettime(CLOCK_MONOTONIC, &t0);

     /* Create reader workers and pass in their portion of list1 */   
     base = req_count / p;
     rem  = req_count % p;
     cur  = 0;

     for (int i = 0; i < p; i++) {
          int cnt = base + (i < rem ? 1 : 0);
          targs[i].fd = fd;
          targs[i].reqs = list1;
          targs[i].start_idx = cur;
          targs[i].count = cnt;
          targs[i].buf = buf;
          cur += cnt;

          if (pthread_create(&threads[i], NULL, reader_thread_func, &targs[i]) != 0) {
               perror("pthread_create");
               for (int k = 0; k < i; k++) pthread_join(threads[k], NULL);
               free(buf); close(fd); free(list1); free(list2); free(threads); free(targs);
               return 1;
          }
     }

     /* Wait for all reader to finish */ 
     for (int i = 0; i < p; i++) pthread_join(threads[i], NULL);

     // close the file 
     close(fd);

     // end timing 
     clock_gettime(CLOCK_MONOTONIC, &t1);

     elapsed = timespec_diff_sec(t0, t1);
     bw = total_mb / elapsed;

     // Print out the read bandwidth
     printf("Read %f MB, use %d threads, elapsed time %f s, write bandwidth: %f MB/s \n",
            total_mb, p, elapsed, bw);


     // ---------------------- LIST 2 WRITE/READ ----------------------
     // Repeat the write and read test now using List2 

     fd = open("testfile.dat", O_CREAT | O_RDWR | O_TRUNC, 0666);
     if (fd < 0) {
          perror("open");
          free(buf);
          free(list1);
          free(list2);
          free(threads);
          free(targs);
          return 1;
     }

     clock_gettime(CLOCK_MONOTONIC, &t0);

     base = req_count / p;
     rem  = req_count % p;
     cur  = 0;

     for (int i = 0; i < p; i++) {
          int cnt = base + (i < rem ? 1 : 0);
          targs[i].fd = fd;
          targs[i].reqs = list2;
          targs[i].start_idx = cur;
          targs[i].count = cnt;
          targs[i].buf = buf;
          cur += cnt;

          if (pthread_create(&threads[i], NULL, writer_thread_func, &targs[i]) != 0) {
               perror("pthread_create");
               for (int k = 0; k < i; k++) pthread_join(threads[k], NULL);
               free(buf); close(fd); free(list1); free(list2); free(threads); free(targs);
               return 1;
          }
     }

     for (int i = 0; i < p; i++) pthread_join(threads[i], NULL);

     close(fd);
     clock_gettime(CLOCK_MONOTONIC, &t1);

     elapsed = timespec_diff_sec(t0, t1);
     total_bytes = (double)req_count * (double)list2_bytes;
     total_mb = total_bytes / (1024.0 * 1024.0);
     bw = total_mb / elapsed;

     printf("Write %f MB, use %d threads, elapsed time %f s, write bandwidth: %f MB/s \n",
            total_mb, p, elapsed, bw);

     fd = open("testfile.dat", O_RDONLY);
     if (fd < 0) {
          perror("open");
          free(buf);
          free(list1);
          free(list2);
          free(threads);
          free(targs);
          return 1;
     }

     clock_gettime(CLOCK_MONOTONIC, &t0);

     base = req_count / p;
     rem  = req_count % p;
     cur  = 0;

     for (int i = 0; i < p; i++) {
          int cnt = base + (i < rem ? 1 : 0);
          targs[i].fd = fd;
          targs[i].reqs = list2;
          targs[i].start_idx = cur;
          targs[i].count = cnt;
          targs[i].buf = buf;
          cur += cnt;

          if (pthread_create(&threads[i], NULL, reader_thread_func, &targs[i]) != 0) {
               perror("pthread_create");
               for (int k = 0; k < i; k++) pthread_join(threads[k], NULL);
               free(buf); close(fd); free(list1); free(list2); free(threads); free(targs);
               return 1;
          }
     }

     for (int i = 0; i < p; i++) pthread_join(threads[i], NULL);

     close(fd);
     clock_gettime(CLOCK_MONOTONIC, &t1);

     elapsed = timespec_diff_sec(t0, t1);
     bw = total_mb / elapsed;

     printf("Read %f MB, use %d threads, elapsed time %f s, write bandwidth: %f MB/s \n",
            total_mb, p, elapsed, bw);


     /*free up resources properly */
     free(buf);
     free(list1);
     free(list2);
     free(threads);
     free(targs);

     return 0;
}
