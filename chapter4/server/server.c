/*
 * server.c
 *
 * Demonstrate a client/server threading model.
 *
 * Special notes: On a Solaris system, call thr_setconcurrency()
 * to allow interleaved thread execution, since threads are not
 * timesliced.
 */
#include <pthread.h>
#include <math.h>
#include "errors.h"

#include "server.h"
#include "client_routine.h"
#include "tty_server_request.h"


int main (int argc, char *argv[])
{
    pthread_t thread;
    int count;
    int status;

#ifdef sun
    /*
     * On Solaris 2.5, threads are not timesliced. To ensure
     * that our threads can run concurrently, we need to
     * increase the concurrency level to CLIENT_THREADS.
     */
    DPRINTF (("Setting concurrency level to %d\n", CLIENT_THREADS));
    thr_setconcurrency (CLIENT_THREADS);
#endif

    /* Create CLIENT_THREADS clients. */
    client_threads = CLIENT_THREADS;
    for (count = 0; count < client_threads; count++) {
        status = pthread_create (&thread, NULL, client_routine, (void*)count); if (status != 0) err_abort (status, "Create client thread");
    }
    status = pthread_mutex_lock (&client_mutex); if (status != 0) err_abort (status, "Lock client mutex");
    while (client_threads > 0) {
        status = pthread_cond_wait (&clients_done, &client_mutex); if (status != 0) err_abort (status, "Wait for clients to finish");
    }
    status = pthread_mutex_unlock (&client_mutex); if (status != 0) err_abort (status, "Unlock client mutex");
    printf ("All clients done\n");
    tty_server_request (REQ_QUIT, 1, NULL, NULL);
    return 0;
}