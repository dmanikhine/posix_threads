#include "server.h"
#include "tty_server_request.h"

/* Client routine -- multiple copies will request server. */
void *client_routine (void *arg)
{
    int my_number = (int)arg, loops;
    char prompt[32];
    char string[128], formatted[128];
    int status;

    sprintf (prompt, "Client %d> ", my_number);
    while (1) {
        tty_server_request (REQ_READ, 1, prompt, string);
        if (strlen (string) == 0)  break;
        for (loops = 0; loops < 4; loops++) {
            sprintf (formatted, "(%d#%d) %s", my_number, loops, string);
            tty_server_request (REQ_WRITE, 0, NULL, formatted);
            sleep (1);
        }
    }
    status = pthread_mutex_lock (&client_mutex); if (status != 0) err_abort (status, "Lock client mutex");
    client_threads--;
    if (client_threads <= 0) {
        status = pthread_cond_signal (&clients_done);  if (status != 0) err_abort (status, "Signal clients done");
    }
    status = pthread_mutex_unlock (&client_mutex); if (status != 0) err_abort (status, "Unlock client mutex");
    return NULL;
}