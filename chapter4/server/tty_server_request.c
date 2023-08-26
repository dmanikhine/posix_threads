#include "server.h"
#include "tty_server_routine.h"

/* Request an operation */
void tty_server_request (int operation, int sync, const char  *prompt, char *string)
{
    request_t *request;
    int status;

    status = pthread_mutex_lock (&tty_server.mutex);  if (status != 0) err_abort (status, "Lock server mutex");
    if (!tty_server.running) {
        pthread_t thread;
        pthread_attr_t detached_attr;

        status = pthread_attr_init (&detached_attr); if (status != 0) err_abort (status, "Init attributes object");
        status = pthread_attr_setdetachstate (&detached_attr, PTHREAD_CREATE_DETACHED); if (status != 0) err_abort (status, "Set detach state");
        tty_server.running = 1;
        status = pthread_create (&thread, &detached_attr, tty_server_routine, NULL); if (status != 0) err_abort (status, "Create server");

        /* Ignore an error in destroying the attributes object. It's unlikely to fail, there's nothing useful we can
         * do about it, and it's not worth aborting the program over it.
         */
        pthread_attr_destroy (&detached_attr);
    }

    /*
     * Create and initialize a request structure.
     */
    request = (request_t*)malloc (sizeof (request_t)); if (request == NULL)  errno_abort ("Allocate request");
    request->next = NULL;
    request->operation = operation;
    request->synchronous = sync;
    if (sync) {
        request->done_flag = 0;
        status = pthread_cond_init (&request->done, NULL); if (status != 0) err_abort (status, "Init request condition");
    }
    if (prompt != NULL) strncpy (request->prompt, prompt, 32);
    else request->prompt[0] = '\0';
    if (operation == REQ_WRITE && string != NULL) strncpy (request->text, string, 128);
    else request->text[0] = '\0';

    /* Add the request to the queue, maintaining the first and last pointers. */
    if (tty_server.first == NULL) {
        tty_server.first = request;
        tty_server.last = request;
    } else {
        (tty_server.last)->next = request;
        tty_server.last = request;
    }

    /* Tell the server that a request is available. */
    status = pthread_cond_signal (&tty_server.request);  if (status != 0) err_abort (status, "Wake server");

    /* If the request was "synchronous", then wait for a reply. */
    if (sync) {
        while (!request->done_flag) {
            status = pthread_cond_wait ( &request->done, &tty_server.mutex); if (status != 0) err_abort (status, "Wait for sync request");
        }
        if (operation == REQ_READ) {
            if (strlen (request->text) > 0) strcpy (string, request->text);
            else string[0] = '\0';
        }
        status = pthread_cond_destroy (&request->done);  if (status != 0) err_abort (status, "Destroy request condition");
        free (request);
    }
    status = pthread_mutex_unlock (&tty_server.mutex); if (status != 0) err_abort (status, "Unlock mutex");
}