#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <string.h>

#define FILESIZE (1024 * 1024)   // 1 MB

int main(void)
{
    int fd;
    char *mmapped_ptr;

    fd = open("file_to_map.txt", O_RDWR);
    if (fd < 0) {
        perror("open");
        exit(1);
    }

    mmapped_ptr = mmap(NULL, FILESIZE,
                       PROT_READ | PROT_WRITE,
                       MAP_SHARED,
                       fd, 0);
    if (mmapped_ptr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        exit(1);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        munmap(mmapped_ptr, FILESIZE);
        close(fd);
        exit(1);
    }

    if (pid == 0) {
        // Child: write first, then exit
        printf("Child process (pid=%d): mmap address: %p\n",
               getpid(), mmapped_ptr);

        memcpy(mmapped_ptr + 0, "01234", 5);

        munmap(mmapped_ptr, FILESIZE);
        close(fd);
        exit(0);
    } else {
        // Parent: wait until child finished writing, then write+read
        printf("Parent process (pid=%d): mmap address: %p\n",
               getpid(), mmapped_ptr);

        wait(NULL); // ensure child finished writing "01234"

        // Parent writes its part now
        memcpy(mmapped_ptr + 4096, "56789", 5);

        // Parent reads 5 chars from mmapped_ptr[0]
        char buf0[6];
        memcpy(buf0, mmapped_ptr + 0, 5);
        buf0[5] = '\0';
        printf("Parent process (pid=%d): read from mmapped_ptr[0]: %s\n",
               getpid(), buf0);

        // Now child is gone, so we demonstrate the other read by printing what is at 4096 too
        char buf4096[6];
        memcpy(buf4096, mmapped_ptr + 4096, 5);
        buf4096[5] = '\0';
        printf("Parent process (pid=%d): read from mmapped_ptr[4096]: %s\n",
               getpid(), buf4096);

        munmap(mmapped_ptr, FILESIZE);
        close(fd);
    }

    return 0;
}
