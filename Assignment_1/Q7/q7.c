#include <stdio.h>
#include <unistd.h>

int main() {
    int pid = fork();  // create child process

    if (pid == 0) {
        // Code executed by the child
        printf("I'm the child %d\n", getpid());
    } else {
        // Code executed by the parent
        printf("My child is called %d\n", pid);
        sleep(10); // parent waits 10s so we can observe processes
    }

    // Both processes run this line
    printf("PID%d says goodbye\n", getpid());
    return 0;
}
