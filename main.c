
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <stdarg.h>

#include <sys/inotify.h> // system library to watch the file for changes



#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

const char *STANDARD_LOGFILE_NAME = "projectMonitorOut.txt";


// Storing current file tracker information
struct trackerEnum {
    
    char file_to_track[1024];
    char log_file_out[1024];
    
};

typedef struct trackerEnum FTracker;



// prototype declarations
int checkValidFileIn(int argc, char *argv[], FTracker *newTracker);

void displayInotifyEvent(struct inotify_event *i) {
    printf("    wd =%2d; ", i->wd);
    if (i->cookie > 0)
        printf("cookie =%4d; ", i->cookie);

    printf("mask = ");
    if (i->mask & IN_ACCESS)        printf("IN_ACCESS ");
    if (i->mask & IN_ATTRIB)        printf("IN_ATTRIB ");
    if (i->mask & IN_CLOSE_NOWRITE) printf("IN_CLOSE_NOWRITE ");
    if (i->mask & IN_CLOSE_WRITE)   printf("IN_CLOSE_WRITE ");
    if (i->mask & IN_CREATE)        printf("IN_CREATE ");
    if (i->mask & IN_DELETE)        printf("IN_DELETE ");
    if (i->mask & IN_DELETE_SELF)   printf("IN_DELETE_SELF ");
    if (i->mask & IN_IGNORED)       printf("IN_IGNORED ");
    if (i->mask & IN_ISDIR)         printf("IN_ISDIR ");
    if (i->mask & IN_MODIFY)        printf("IN_MODIFY ");
    if (i->mask & IN_MOVE_SELF)     printf("IN_MOVE_SELF ");
    if (i->mask & IN_MOVED_FROM)    printf("IN_MOVED_FROM ");
    if (i->mask & IN_MOVED_TO)      printf("IN_MOVED_TO ");
    if (i->mask & IN_OPEN)          printf("IN_OPEN ");
    if (i->mask & IN_Q_OVERFLOW)    printf("IN_Q_OVERFLOW ");
    if (i->mask & IN_UNMOUNT)       printf("IN_UNMOUNT ");
    printf("\n");

    if (i->len > 0)
        printf("        name = %s\n", i->name);
}


int main(int argc, char *argv[]) {

    /**
     * Main
     * 
     * Arguments:
     *  -f for new file to watch
     *  -o for log to output this too // defaults to the standard log out 
     */


    char file_to_track[1024];
    int found = 0;
    
    FTracker newTracker;
    FTracker *ntptr = &newTracker;
    struct inotify_event *event;


    printf("Starting file\n");
    printf("Found %d arguments\n", argc);

    if (checkValidFileIn(argc, argv, ntptr) == 1) {
        return 1;
    }
    
    
    

    return 0;
}


int checkValidFileIn(int argc, char *argv[], FTracker *newTracker) {

    int inotifyFd, wd, j;
    size_t numRead;
    FILE *newFile;
    int found = 0;
    

    for (int i = 0 ; i < argc; i++) {
        if (strcmp("-f", argv[i]) == 0) {
            if (i + 1 == argc) {
                printf("-f argument is missing a filename after it. Exiting\n");
                return 1;
            }
            printf("Found -f, next arg is %s\n", argv[i+1]);
            strcpy(newTracker->file_to_track, argv[i+1]);
            found = 1; 
        }
    }

    if (found == 0) {
        printf("Program requires a -f file for input. Exiting.\n");
        return 1;
    }


    // checking the file specified on the 
    if (!(newFile = fopen(newTracker->file_to_track, "r"))) {

        printf("Failed to open the file to track : \"%s\". Exiting\n", newTracker->file_to_track);

        // closing the file on exit
        // fclose(newFile);
        return 1;
        
    }


    /**
     * NEWLY ADDED STUFF IS HERE
     * 
     */

    inotifyFd = inotify_init();                 // Create inotify instance 
    if (inotifyFd == -1){
        printf("inotify init");
        exit(EXIT_FAILURE);
    }        

    
    wd = inotify_add_watch(inotifyFd, newTracker->file_to_track, IN_ALL_EVENTS);
    if (wd == -1){
        printf("inotify_add_watch");
        exit(EXIT_FAILURE);
    }

    printf("Watching %s using wd %d\n", argv[j], wd);
    

    

    // while(1){
        
    // }


    // closing the file on exit
    fclose(newFile);
    
    exit(EXIT_SUCCESS);

    return 0; // VALID

}

