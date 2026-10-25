#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int num_threads = 0;

typedef struct node {
    int node_id;      // a unique ID assigned to each node
    struct node *next;
} Node;

Node *top; // top of stack
int global_id = 0;

pthread_mutex_t stack_mutex = PTHREAD_MUTEX_INITIALIZER;

/*Option 1: Mutex Lock*/
void push_mutex()
{
    Node *new_node = malloc(sizeof(Node));
    if (!new_node) pthread_exit(NULL);

    pthread_mutex_lock(&stack_mutex);

    new_node->node_id = global_id++;
    new_node->next = top;
    top = new_node;

    pthread_mutex_unlock(&stack_mutex);
}

int pop_mutex()
{
    pthread_mutex_lock(&stack_mutex);

    if (top == NULL) {
        pthread_mutex_unlock(&stack_mutex);
        return -1;
    }

    Node *old_node = top;
    top = old_node->next;
    int id = old_node->node_id;
    free(old_node);

    pthread_mutex_unlock(&stack_mutex);
    return id;
}

/*Option 2: Compare-and-Swap (CAS)*/
void push_cas()
{
    Node *new_node = malloc(sizeof(Node));
    if (!new_node) pthread_exit(NULL);

    new_node->node_id = __sync_fetch_and_add(&global_id, 1);

    Node *old_node;
    do {
        old_node = top;
        new_node->next = old_node;
    } while (!__sync_bool_compare_and_swap(&top, old_node, new_node));
}

int pop_cas()
{
    Node *old_node;
    Node *next;

    do {
        old_node = top;
        if (old_node == NULL)
            return -1;

        next = old_node->next;
    } while (!__sync_bool_compare_and_swap(&top, old_node, next));

    int id = old_node->node_id;
    free(old_node);
    return id;
}

/* the thread function */
typedef struct {
    int opt;
    int my_id;
} thread_arg_t;

void *thread_func(void *arg)
{
    thread_arg_t *t = arg;
    int opt = t->opt;
    int my_id = t->my_id;

    free(t);

    if (opt == 0) {
        push_mutex(); push_mutex();
        pop_mutex();  pop_mutex();
        push_mutex();
    } else {
        push_cas(); push_cas();
        pop_cas();  pop_cas();
        push_cas();
    }

    printf("Thread %d: exit\n", my_id);
    pthread_exit(0);
}

/* Count and clear remaining nodes in Stack */
int count_and_clear_stack()
{
    int count = 0;
    Node *curr = top;

    while (curr != NULL) {
        count++;
        Node *tmp = curr;
        curr = curr->next;
        free(tmp);
    }

    top = NULL;
    return count;
}

int main(int argc, char *argv[])
{
    num_threads = atoi(argv[1]);
    pthread_t *workers = malloc(num_threads * sizeof(pthread_t));

    /* Option 1: Mutex */
    global_id = 0;
    top = NULL;

    for (int i = 0; i < num_threads; i++) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);

        thread_arg_t *arg = malloc(sizeof(thread_arg_t));
        arg->opt = 0;
        arg->my_id = i;

        pthread_create(&workers[i], &attr, thread_func, arg);
    }
    for (int i = 0; i < num_threads; i++)
        pthread_join(workers[i], NULL);

    printf("Mutex: remaining nodes = %d\n", count_and_clear_stack());

    /* Option 2: CAS */
    global_id = 0;
    top = NULL;

    for (int i = 0; i < num_threads; i++) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);

        thread_arg_t *arg = malloc(sizeof(thread_arg_t));
        arg->opt = 1;
        arg->my_id = i;

        pthread_create(&workers[i], &attr, thread_func, arg);
    }
    for (int i = 0; i < num_threads; i++)
        pthread_join(workers[i], NULL);

    printf("CAS: remaining nodes = %d\n", count_and_clear_stack());

    free(workers);
    pthread_mutex_destroy(&stack_mutex);
    return 0;
}
