#include "pthread.h"

#include "errors.h"
#include "server.h"

/*
 * The server start routine. It waits for a request to appear
 * in tty_server.requests using the request condition variable.
 * It processes requests in FIFO order. If a request is marked
 * "synchronous" (synchronous != 0), the server will set done_flag
 * and signal the request's condition variable. The client is
 * responsible for freeing the request. If the request was not
 * synchronous, the server will free the request on completion.
 */
void *tty_server_routine (void *arg)
{
    static pthread_mutex_t prompt_mutex = PTHREAD_MUTEX_INITIALIZER;
    request_t *request;
    int operation, len;
    int status;

    while (1) {
        status = pthread_mutex_lock (&tty_server.mutex); if (status != 0) err_abort (status, "Lock server mutex");

        while (tty_server.first == NULL) { /* Wait for data  */
            status = pthread_cond_wait ( &tty_server.request, &tty_server.mutex);
            if (status != 0) err_abort (status, "Wait for request");
        }
        request = tty_server.first; tty_server.first = request->next;

        if (tty_server.first == NULL) tty_server.last = NULL;
        status = pthread_mutex_unlock (&tty_server.mutex); if (status != 0) err_abort (status, "Unlock server mutex");

        operation = request->operation; /* Process the data */
        switch (operation) {
            case REQ_QUIT:  break;
            case REQ_READ:
                if (strlen (request->prompt) > 0) printf (request->prompt);
                if (fgets (request->text, 128, stdin) == NULL)  request->text[0] = '\0';
                /*
                 * Because fgets returns the newline, and we don't want it,
                 * we look for it, and turn it into a null (truncating the
                 * input) if found. It should be the last character, if it is
                 * there.
                 */
                len = strlen (request->text);
                if (len > 0 && request->text[len-1] == '\n') request->text[len-1] = '\0';
                break;
            case REQ_WRITE:
                puts (request->text);
                break;
            default:
                break;
        }

        if (request->synchronous) {
            status = pthread_mutex_lock (&tty_server.mutex);  if (status != 0)  err_abort (status, "Lock server mutex");
            request->done_flag = 1;
            status = pthread_cond_signal (&request->done); if (status != 0) err_abort (status, "Signal server condition");
            status = pthread_mutex_unlock (&tty_server.mutex); if (status != 0) err_abort (status, "Unlock server mutex");
        } else
            free (request);

        if (operation == REQ_QUIT) break;
    }
    return NULL;
}