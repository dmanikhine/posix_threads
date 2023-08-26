#include <pthread.h>


#define CLIENT_THREADS  4               /* Number of clients */

#define REQ_READ        1               /* Read with prompt */
#define REQ_WRITE       2               /* Write */
#define REQ_QUIT        3               /* Quit server */

/* Internal to server "package" -- one for each request. */
typedef struct request_tag {
    struct request_tag  *next;          /* Link to next */
    int                 operation;      /* Function code */
    int                 synchronous;    /* Non-zero if synchronous */
    int                 done_flag;      /* Predicate for wait */
    pthread_cond_t      done;           /* Wait for completion */
    char                prompt[32];     /* Prompt string for reads */
    char                text[128];      /* Read/write text */
} request_t;

/* Static context for the server */
typedef struct tty_server_tag {
    request_t           *first;
    request_t           *last;
    int                 running;
    pthread_mutex_t     mutex;
    pthread_cond_t      request;
} tty_server_t;

tty_server_t tty_server = { NULL, NULL, 0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};

/* Main program data */

int client_threads;
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t clients_done = PTHREAD_COND_INITIALIZER;