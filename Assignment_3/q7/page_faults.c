#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <stdio.h>

int main(int argc, char** argv){

  time_t start, end;

  int num_pages = atoi(argv[1]);
  int page_size = getpagesize();

  printf("Allocating %d pages of %d bytes\n", num_pages, page_size);

  char *addr;

  /* Start timer */
  start = clock();

   //Option 1: normal pages -> remove MAP_HUGETLB
   //Option 2: huge pages   -> keep MAP_HUGETLB
  addr = (char*) mmap(NULL,
                      (size_t)num_pages * (size_t)page_size,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                      -1,
                      0);

  if (addr == MAP_FAILED) {
    perror("mmap");
    exit(1);
  }

  /* Update one byte per page */
  char c = 'a';
  for(int i = 0; i < num_pages; i++){
    addr[i * page_size] = c;
    c++;
  }

  /* End timer */
  end = clock();

  /* Print elapsed time in cycles */
  printf("Elapsed cycles: %ld\n", (long)(end - start));

  for(int i = 0; (i < num_pages && i < 16); i++){
    printf("%c ", addr[i * page_size]);
  }
  printf("\n");

  munmap(addr, (size_t)page_size * (size_t)num_pages);
}
