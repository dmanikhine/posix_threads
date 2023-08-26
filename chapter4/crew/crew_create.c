#include "crew.h"
#include "errors.h"
#include "worker_routine.h"

/* Create a work crew. */

int crew_create (crew_t *crew, int crew_size)
{
    int crew_index;
    int status;
    
    if (crew_size > CREW_SIZE) return EINVAL; /* We won't create more than CREW_SIZE members  */

    crew->crew_size = crew_size;
    crew->work_count = 0;
    crew->first = NULL;
    crew->last = NULL;

    /*Initialize synchronization objects */
    status = pthread_mutex_init (&crew->mutex, NULL); if (status != 0) return status;
    status = pthread_cond_init (&crew->done, NULL); if (status != 0) return status;
    status = pthread_cond_init (&crew->go, NULL); if (status != 0) return status;

    /*Create the worker threads. */
    for (crew_index = 0; crew_index < CREW_SIZE; crew_index++) {
        crew->crew[crew_index].index = crew_index;
        crew->crew[crew_index].crew = crew;
        status = pthread_create (&crew->crew[crew_index].thread, NULL, worker_routine, (void*)&crew->crew[crew_index]);
        if (status != 0) err_abort (status, "Create worker");
    }
    return 0;
}