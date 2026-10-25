#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

typedef struct page {
    int page_id;
    int reference_bit;
    struct page *next;

    // @other auxiliary
    unsigned long total_referenced;
    int where; // 1 = active, 2 = inactive
} Node;

/* ---------------- Globals ---------------- */

static int n = 0;          // number of pages
static int m = 0;          // checker interval (microseconds)
static int *ref_str = NULL;
static int ref_len = 1000;

static Node *pages = NULL;           // array of pages [0..n-1]
static Node *active_head = NULL, *active_tail = NULL;
static Node *inactive_head = NULL, *inactive_tail = NULL;
static int active_size = 0, inactive_size = 0;

static pthread_mutex_t list_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile int player_done = 0;

/* ---------------- List helpers (singly linked with head+tail) ---------------- */

static void list_append(Node **head, Node **tail, int *sz, Node *p) {
    p->next = NULL;
    if (*tail) (*tail)->next = p;
    else *head = p;
    *tail = p;
    (*sz)++;
}

static Node* list_pop_head(Node **head, Node **tail, int *sz) {
    Node *p = *head;
    if (!p) return NULL;
    *head = p->next;
    if (*tail == p) *tail = NULL;
    p->next = NULL;
    (*sz)--;
    return p;
}

// Remove node by id from a singly linked list; returns the removed node or NULL
static Node* list_remove_by_id(Node **head, Node **tail, int *sz, int id) {
    Node *prev = NULL, *cur = *head;
    while (cur) {
        if (cur->page_id == id) {
            if (prev) prev->next = cur->next;
            else *head = cur->next;

            if (*tail == cur) *tail = prev;

            cur->next = NULL;
            (*sz)--;
            return cur;
        }
        prev = cur;
        cur = cur->next;
    }
    return NULL;
}

/* ---------------- Core behavior ---------------- */

static int active_max_threshold(void) {
    // active size > 0.7n triggers moving pages to inactive
    return (int)(0.7 * (double)n);
}

static int move_count_to_inactive(void) {
    // move ~0.2n pages
    int k = (int)(0.2 * (double)n + 0.999999); // ceil without math.h
    if (k < 1) k = 1;
    return k;
}

// Called when a page is referenced: set ref bit, increment stats, move to rear of active
static void handle_reference(int page_id) {
    Node *p = NULL;

    // Remove from whichever list it is in
    if (pages[page_id].where == 1) {
        p = list_remove_by_id(&active_head, &active_tail, &active_size, page_id);
    } else {
        p = list_remove_by_id(&inactive_head, &inactive_tail, &inactive_size, page_id);
    }
    if (!p) {
        // Should not happen, but keep safe
        p = &pages[page_id];
        p->next = NULL;
    }

    p->reference_bit = 1;
    p->total_referenced++;
    p->where = 1;
    list_append(&active_head, &active_tail, &active_size, p);

    // If active too large, move ~0.2n pages from front of active to inactive
    if (active_size > active_max_threshold()) {
        int k = move_count_to_inactive();
        for (int i = 0; i < k; i++) {
            Node *q = list_pop_head(&active_head, &active_tail, &active_size);
            if (!q) break;
            q->where = 2;
            list_append(&inactive_head, &inactive_tail, &inactive_size, q);
        }
    }
}

/* ---------------- Threads ---------------- */

void *player_thread_func() {
    // @Add code for player
    for (int i = 0; i < ref_len; i++) {
        int page_id = ref_str[i];

        pthread_mutex_lock(&list_lock);
        handle_reference(page_id);
        pthread_mutex_unlock(&list_lock);

        usleep(10);
    }

    player_done = 1;
    pthread_exit(0);
}

void *checker_thread_func() {
    // @Add code for checker
    while (!player_done) {
        usleep(m);

        pthread_mutex_lock(&list_lock);

        // scan active list: count ref_bit=1 and clear it
        unsigned long referenced_count = 0;
        for (Node *cur = active_head; cur != NULL; cur = cur->next) {
            if (cur->reference_bit == 1) {
                referenced_count++;
                cur->reference_bit = 0;
            }
        }

        pthread_mutex_unlock(&list_lock);
    }

    pthread_exit(0);
}

/* ---------------- Printing helpers ---------------- */

static void print_list(const char *label, Node *head) {
    printf("%s", label);
    for (Node *cur = head; cur != NULL; cur = cur->next) {
        printf("%d ", cur->page_id);
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    n = atoi(argv[1]);
    m = atoi(argv[2]);

    if (argc != 3 || n <= 0 || m <= 0) {
        fprintf(stderr, "Usage: %s n m_microseconds\n", argv[0]);
        return 1;
    }

    // @create a random reference string of N (1000 references in [0, n-1])
    srand((unsigned)time(NULL));
    ref_str = (int*)malloc((size_t)ref_len * sizeof(int));
    pages = (Node*)calloc((size_t)n, sizeof(Node));
    if (!ref_str || !pages) {
        perror("malloc/calloc");
        return 1;
    }

    for (int i = 0; i < ref_len; i++) ref_str[i] = rand() % n;

    // Initialize lists: start with all pages in inactive list
    active_head = active_tail = NULL; active_size = 0;
    inactive_head = inactive_tail = NULL; inactive_size = 0;

    for (int i = 0; i < n; i++) {
        pages[i].page_id = i;
        pages[i].reference_bit = 0;
        pages[i].total_referenced = 0;
        pages[i].next = NULL;
        pages[i].where = 2; // inactive
        list_append(&inactive_head, &inactive_tail, &inactive_size, &pages[i]);
    }

    /* Create two workers */
    pthread_t player;
    pthread_t checker;

    pthread_create(&player, NULL, player_thread_func, NULL);
    pthread_create(&checker, NULL, checker_thread_func, NULL);

    pthread_join(player, NULL);
    pthread_join(checker, NULL);

    printf("Page_Id, Total_Referenced\n");
    // @Print out the statistics of page references
    for (int i = 0; i < n; i++) {
        printf("%d, %lu\n", pages[i].page_id, pages[i].total_referenced);
    }

    printf("Pages in active list: ");
    // @Print out the list of pages in active list
    for (Node *cur = active_head; cur != NULL; cur = cur->next) {
        printf("%d ", cur->page_id);
    }
    printf("\n");

    printf("Pages in inactive list: ");
    // @Print out the list of pages in inactive list
    for (Node *cur = inactive_head; cur != NULL; cur = cur->next) {
        printf("%d ", cur->page_id);
    }
    printf("\n");

    /* free up resources properly */
    free(ref_str);
    free(pages);
    return 0;
}
