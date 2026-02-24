
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <errno.h>
#include <unistd.h>
#include <poll.h>

#include <stdarg.h>

#include <sys/inotify.h> // system library to watch the file for changes

#include <libaudit.h>
#include <sys/select.h>


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

/* Read all available inotify events from the file descriptor 'fd'.
    wd is the table of watch descriptors for the directories in argv.
    argc is the size of wd and argv.
    argv is the list of watched directories.
    Entry 0 of wd and argv is unused.  */

static void handle_events(int fd, int *wd, int argc, char* argv[]) {
    /* Some systems cannot read integer variables if they are not
        properly aligned.  On other systems, incorrect alignment may
        decrease performance.  Hence, the buffer used for reading from
        the inotify file descriptor should have the same alignment as
        struct inotify_event.  */

    char buf[4096]
        __attribute__ ((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event;
    ssize_t size;

    /* Loop while events can be read from inotify file descriptor.  */

    for (;;) {

        /* Read some events.  */

        size = read(fd, buf, sizeof(buf));
        if (size == -1 && errno != EAGAIN) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        /* If the nonblocking read() found no events to read, then
            it returns -1 with errno set to EAGAIN.  In that case,
            we exit the loop.  */

        if (size <= 0)
            break;

        /* Loop over all events in the buffer.  */

        for (char *ptr = buf; ptr < buf + size;
                ptr += sizeof(struct inotify_event) + event->len) {

            event = (const struct inotify_event *) ptr;

            /* Print event type.  */

            if (event->mask & IN_OPEN)
                printf("IN_OPEN: ");
            if (event->mask & IN_CLOSE_NOWRITE)
                printf("IN_CLOSE_NOWRITE: ");
            if (event->mask & IN_CLOSE_WRITE)
                printf("IN_CLOSE_WRITE: ");
            if (event->mask & IN_MODIFY)
                printf("IN_MODIFY: ");
            if (event->mask & IN_ACCESS)
                printf("IN_ACCESS: ");

            /* Print the name of the watched directory.  */

            for (size_t i = 1; i < argc; ++i) {
                if (wd[i] == event->wd) {
                    printf("%s/", argv[i]);
                    break;
                }
            }

            /* Print the name of the file.  */

            if (event->len)
                printf("%s", event->name);

            /* Print type of filesystem object.  */

            if (event->mask & IN_ISDIR)
                printf(" [directory]\n");
            else
                printf(" [file]\n");
        }
    }
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

    char buf;
    int fd; // file descriptor 
    int i;
    int poll_num;
    int *wd;
    nfds_t nfds;
    struct pollfd fds[2];

    // Creating the file descriptor for accessing inotify api
    fd = inotify_init1(IN_NONBLOCK);
    if (fd == -1) {
        perror("inotify_init1");
        exit(EXIT_FAILURE);
    }

    /* Allocate memory for watch descriptors.  */
    wd = calloc(argc, sizeof(int));
    if (wd == NULL) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }


    printf("Starting file\n");
    printf("Found %d arguments\n", argc);

    if (checkValidFileIn(argc, argv, ntptr) == 1) {
        return 1;
    }

    // Watch the new file
    wd[i] = inotify_add_watch(fd, ntptr->file_to_track, IN_OPEN | IN_CLOSE | IN_ACCESS | IN_MODIFY);
    

    // Checking the watcher started correctly
    if (wd[i] == -1) {
        fprintf(stderr, "Cannot watch '%s': %s\n", ntptr->file_to_track, strerror(errno));
        exit(EXIT_FAILURE);
    }


    int audit_fd = audit_open();
    if (audit_fd < 0) {
        perror("audit_open");
        return 1;
    }
    struct audit_rule_data *rule = audit_rule_create_data();
    printf("size of rule : %d\n", sizeof(*rule));

    // Add an libaudit watcher to the file we're tracking
    if (audit_add_watch(&rule, ntptr->file_to_track) < 0 ){
        perror("audit_add_watch");
        return 1;
    }

    /* Set permissions (like -p wa) */
    audit_update_watch_perms(rule, AUDIT_PERM_READ | AUDIT_PERM_WRITE | AUDIT_PERM_ATTR);

     // install the rule in kernel audit
    if (audit_add_rule_data(audit_fd, rule, 8, 2) < 0) {
        perror("audit_add_rule_data");
        printf("errno=%d\n", errno);
        return 1;
    }

    

     /* Prepare for polling.  */


    fds[0].fd = STDIN_FILENO;       /* Console input */
    fds[0].events = POLLIN;

    fds[1].fd = fd;                 /* Inotify input */
    fds[1].events = POLLIN;

    /* Wait for events and/or terminal input.  */

    printf("Listening for events.\n");
    while (1) {

        // from /sys/select.h library
        fd_set rfds;                // file descriptor struct
        FD_ZERO(&rfds);             // clears it
        FD_SET(audit_fd, &rfds);    // Watches our descriptor

        poll_num = poll(fds, nfds, -1);
        if (poll_num == -1) {
            if (errno == EINTR)
                continue;
            perror("poll");
            exit(EXIT_FAILURE);
        }

        if (poll_num > 0) {

            if (fds[0].revents & POLLIN) {

                /* Console input is available.  Empty stdin and quit.  */

                while (read(STDIN_FILENO, &buf, 1) > 0 && buf != '\n')
                    continue;
                break;
            }
            

            if (fds[1].revents & POLLIN) {

                /* Inotify events are available.  */

                handle_events(fd, wd, argc, argv);

                struct timeval tv = {5, 0};  // 5 seconds
                // using the libaudit only after we found something from inotify
                int ret = select(audit_fd + 1, &rfds, NULL, NULL, &tv); 
                // Select is used instead of while, but there **should** already be something to read if inotify found something
                    // the &tv is a 5 second timeout, just incase
                if (ret < 0) {
                    perror("select");
                    break;
                }
                // Checking if the fil descriptor is ready
                if (FD_ISSET(audit_fd, &rfds)) {
                    struct audit_reply rep;
                    // Getting the reply and printing the message
                    int len = audit_get_reply(audit_fd, &rep, GET_REPLY_NONBLOCKING, 0);
                    if (len > 0) {
                        if (rep.type != AUDIT_EOE &&
                            rep.type != AUDIT_PROCTITLE &&
                            rep.type != AUDIT_PATH) {
                            printf("Audit event: %s\n", rep.message);
                        }
                    }
                }

            }
        }
    }


    // Cleaning up the libaudit watcher
    audit_close(audit_fd);
    free(rule);
    

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
    

    // closing the file on exit
    fclose(newFile);
    

    return 0; // VALID

}

