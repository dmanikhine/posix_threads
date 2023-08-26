/*
 * crew.c
 *
 * Demonstrate a work crew implementing a simple parallel search
 * through a directory tree.
 *
 * Special notes: On a Solaris 2.5 uniprocessor, this test will
 * not produce interleaved output unless extra LWPs are created
 * by calling thr_setconcurrency(), because threads are not
 * timesliced.
 */
#include <sys/types.h>
#include <pthread.h>
#include <sys/stat.h>
#include <dirent.h>
#include "errors.h"

#include "crew_create.h"
#include "crew_start.h"
#include "worker_routine.h"
#include "crew.h"


/* The main program to "drive" the crew... */
int main (int argc, char *argv[])
{
    crew_t my_crew;
    char line[128], *next;
    int status;

    if (argc < 3) { fprintf (stderr, "Usage: %s string path\n", argv[0]); return -1; }

#ifdef sun
    /*
     * On Solaris 2.5, threads are not timesliced. To ensure
     * that our threads can run concurrently, we need to
     * increase the concurrency level to CREW_SIZE.
     */
    DPRINTF (("Setting concurrency level to %d\n", CREW_SIZE));
    thr_setconcurrency (CREW_SIZE);
#endif
    status = crew_create (&my_crew, CREW_SIZE); if (status != 0) err_abort(status, "Create crew");

    status = crew_start (&my_crew, argv[2], argv[1]); if (status != 0) err_abort (status, "Start crew");

    return 0;
}