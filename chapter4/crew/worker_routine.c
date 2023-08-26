#include <sys/stat.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/types.h>

#include "errors.h"
#include "crew.h"


void *worker_routine (void *arg)
{
    worker_p mine = (worker_t*)arg;
    crew_p crew = mine->crew;
    work_p work, new_work;
    struct stat filestat;
    struct dirent *entry;
    int status;

    /*
     * "struct dirent" is funny, because POSIX doesn't require
     * the definition to be more than a header for a variable
     * buffer. Thus, allocate a "big chunk" of memory, and use
     * it as a buffer.
     */
    entry = (struct dirent*)malloc (sizeof (struct dirent) + name_max); if (entry == NULL) errno_abort ("Allocating dirent");
    
    status = pthread_mutex_lock (&crew->mutex); if (status != 0) err_abort (status, "Lock crew mutex");

    /*
     * There won't be any work when the crew is created, so wait
     * until something's put on the queue.
     */
    while (crew->work_count == 0) {
        status = pthread_cond_wait (&crew->go, &crew->mutex); if (status != 0) err_abort (status, "Wait for go");
    }

    status = pthread_mutex_unlock (&crew->mutex);  if (status != 0) err_abort (status, "Unlock mutex");

    DPRINTF (("Crew %d starting\n", mine->index));

    /* Now, as long as there's work, keep doing it. */
    while (1) {
        /*
         * Wait while there is nothing to do, and the hope of something coming along later. If
         * crew->first is NULL, there's no work. But if crew->work_count goes to zero, we're done.
         */
        status = pthread_mutex_lock (&crew->mutex); if (status != 0) err_abort (status, "Lock crew mutex");
        DPRINTF (("Crew %d top: first is %#lx, count is %d\n", mine->index, crew->first, crew->work_count));

        while (crew->first == NULL) {
            status = pthread_cond_wait(&crew->go, &crew->mutex); if (status != 0) err_abort (status, "Wait for work");
        }
        DPRINTF (("Crew %d woke: %#lx, %d\n", mine->index, crew->first, crew->work_count));

        /* Remove and process a work item */
        work = crew->first;
        crew->first = work->next;
        if (crew->first == NULL) crew->last = NULL;
        DPRINTF (("Crew %d took %#lx, leaves first %#lx, last %#lx\n", mine->index, work, crew->first, crew->last));

        status = pthread_mutex_unlock (&crew->mutex); if (status != 0) err_abort (status, "Unlock mutex");

        /* We have a work item. Process it, which may involve queuing new work items. */
        status = lstat (work->path, &filestat);

        if (S_ISLNK (filestat.st_mode))  printf ( "Thread %d: %s is a link, skipping.\n", mine->index, work->path);
        else if (S_ISDIR (filestat.st_mode)) {
            DIR *directory;
            struct dirent *result;

            /* If the file is a directory, search it and place all files onto the queue as new work items. */
            directory = opendir (work->path);
            if (directory == NULL) {
                fprintf ( stderr, "Unable to open directory %s: %d (%s)\n", work->path, errno, strerror (errno));
                continue;
            }
            
            while (1) {
                status = readdir_r (directory, entry, &result);
                if (status != 0) {
                    fprintf (stderr, "Unable to read directory %s: %d (%s)\n", work->path, status, strerror (status));
                    break;
                }
                if (result == NULL)  break;  /* End of directory */
                
                /* Ignore "." and ".." entries. */

                if (strcmp (entry->d_name, ".") == 0) continue;
                if (strcmp (entry->d_name, "..") == 0) continue;
                new_work = (work_p)malloc (sizeof (work_t)); if (new_work == NULL) errno_abort ("Unable to allocate space");
                new_work->path = (char*)malloc (path_max); if (new_work->path == NULL) errno_abort ("Unable to allocate path");
                strcpy (new_work->path, work->path);
                strcat (new_work->path, "/");
                strcat (new_work->path, entry->d_name);
                new_work->string = work->string;
                new_work->next = NULL;
                status = pthread_mutex_lock (&crew->mutex); if (status != 0) err_abort (status, "Lock mutex");
                if (crew->first == NULL) {
                    crew->first = new_work;
                    crew->last = new_work;
                } else {
                    crew->last->next = new_work;
                    crew->last = new_work;
                }
                crew->work_count++;
                DPRINTF (( "Crew %d: add work %#lx, first %#lx, last %#lx, %d\n", mine->index, new_work, crew->first, crew->last, crew->work_count));
                status = pthread_cond_signal (&crew->go);
                status = pthread_mutex_unlock (&crew->mutex); if (status != 0) err_abort (status, "Unlock mutex");
            }
            
            closedir (directory);
        } else if (S_ISREG (filestat.st_mode)) {
            FILE *search;
            char buffer[256], *bufptr, *search_ptr;

            /* If this is a file, not a directory, then search it for the string.  */
            search = fopen (work->path, "r");
            if (search == NULL)
                fprintf ( stderr, "Unable to open %s: %d (%s)\n", work->path, errno, strerror (errno));
            else {

                while (1) {
                    bufptr = fgets (
                        buffer, sizeof (buffer), search);
                    if (bufptr == NULL) {
                        if (feof (search))
                            break;
                        if (ferror (search)) {
                            fprintf (
                                stderr,
                                "Unable to read %s: %d (%s)\n",
                                work->path,
                                errno, strerror (errno));
                            break;
                        }
                    }
                    search_ptr = strstr (buffer, work->string);
                    if (search_ptr != NULL) {
                        flockfile (stdout);
                        printf ( "Thread %d found \"%s\" in %s\n", mine->index, work->string, work->path);
#if 0
                        printf ("%s\n", buffer);
#endif
                        funlockfile (stdout);
                        break;
                    }
                }
                fclose (search);
            }
        } else
            fprintf (stderr, "Thread %d: %s is type %o (%s))\n", mine->index, work->path,
                filestat.st_mode & S_IFMT, (S_ISFIFO (filestat.st_mode) ? "FIFO"
                 : (S_ISCHR (filestat.st_mode) ? "CHR"
                    : (S_ISBLK (filestat.st_mode) ? "BLK"
                       : (S_ISSOCK (filestat.st_mode) ? "SOCK"
                          : "unknown")))));

        free (work->path);              /* Free path buffer */
        free (work);                    /* We're done with this */

        /*
         * Decrement count of outstanding work items, and wake
         * waiters (trying to collect results or start a new
         * calculation) if the crew is now idle.
         *
         * It's important that the count be decremented AFTER
         * processing the current work item. That ensures the
         * count won't go to 0 until we're really done.
         */
        status = pthread_mutex_lock (&crew->mutex); if (status != 0) err_abort (status, "Lock crew mutex");

        crew->work_count--;
        DPRINTF (("Crew %d decremented work to %d\n", mine->index, crew->work_count));

        if (crew->work_count <= 0) {
            DPRINTF (("Crew thread %d done\n", mine->index));
            status = pthread_cond_broadcast (&crew->done); if (status != 0) err_abort (status, "Wake waiters");
            status = pthread_mutex_unlock (&crew->mutex); if (status != 0) err_abort (status, "Unlock mutex");
            break;
        }

        status = pthread_mutex_unlock (&crew->mutex); if (status != 0) err_abort (status, "Unlock mutex");

    }

    free (entry);
    return NULL;
}
