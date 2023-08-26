
#include <sys/stat.h>
#include <dirent.h>
#include <pthread.h>

#include "errors.h"
#include "crew.h"


/*
 * The thread start routine for crew threads. Waits until "go"
 * command, processes work items until requested to shut down.
 */

/*
 * Pass a file path to a work crew previously created
 * using crew_create
 */
int crew_start ( crew_p crew, char *filepath, char *search)
{
    work_p request;
    int status;

    status = pthread_mutex_lock (&crew->mutex); if (status != 0) return status;

    /* If the crew is busy, wait for them to finish. */
    while (crew->work_count > 0) {
        status = pthread_cond_wait (&crew->done, &crew->mutex);
        if (status != 0) { pthread_mutex_unlock (&crew->mutex); return status; }
    }

    errno = 0;
    path_max = pathconf (filepath, _PC_PATH_MAX);
    if (path_max == -1) {
        if (errno == 0) path_max = 1024;             /* "No limit" */
        else  errno_abort ("Unable to get PATH_MAX");
    }
    errno = 0;
    name_max = pathconf (filepath, _PC_NAME_MAX);
    if (name_max == -1) {
         if (errno == 0) name_max = 256;             /* "No limit" */
        else errno_abort ("Unable to get NAME_MAX");
    }
    DPRINTF (("PATH_MAX for %s is %ld, NAME_MAX is %ld\n", filepath, path_max, name_max));
    path_max++;                         /* Add null byte */
    name_max++;                         /* Add null byte */

    request = (work_p)malloc (sizeof (work_t)); if (request == NULL) errno_abort ("Unable to allocate request");
    DPRINTF (("Requesting %s\n", filepath));
    request->path = (char*)malloc (path_max); if (request->path == NULL) errno_abort ("Unable to allocate path");
    strcpy (request->path, filepath);
    request->string = search;
    request->next = NULL;
    if (crew->first == NULL) {
        crew->first = request;
        crew->last = request;
    } else {
        crew->last->next = request;
        crew->last = request;
    }
    crew->work_count++;

    status = pthread_cond_signal (&crew->go);
    if (status != 0) {
        free (crew->first);
        crew->first = NULL;
        crew->work_count = 0;
        pthread_mutex_unlock (&crew->mutex);
        return status;
    }
    while (crew->work_count > 0) {
        status = pthread_cond_wait (&crew->done, &crew->mutex);
        if (status != 0) err_abort (status, "waiting for crew to finish");
    }
    status = pthread_mutex_unlock (&crew->mutex);
    if (status != 0) err_abort (status, "Unlock crew mutex");
    return 0;
}