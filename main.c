#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/fanotify.h>
#include <sys/stat.h>

#define BUF_SIZE 8192





static void print_path_from_fd(int fd)
{
    char path[PATH_MAX];
    char proc_path[64];

    snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", fd);

    ssize_t len = readlink(proc_path, path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        printf("Path: %s\n", path);
    } else {
        perror("readlink");
    }
}

void print_exe_from_pid(pid_t pid)
{
    char exe_path[64];
    char resolved[PATH_MAX];

    snprintf(exe_path, sizeof(exe_path), "/proc/%d/exe", pid);

    ssize_t len = readlink(exe_path, resolved, sizeof(resolved) - 1);
    if (len != -1) {
        resolved[len] = '\0';
        printf("Executable: %s\n", resolved);
    } else {
        printf("Executable: [unknown]\n");
    }

}



void print_cmdline_from_pid(pid_t pid)
{
    char cmd_path[64];
    char buffer[4096];

    char parentPid[64];

    snprintf(cmd_path, sizeof(cmd_path), "/proc/%d/cmdline", pid);

    FILE *f = fopen(cmd_path, "r");
    if (!f) {
        printf("Cmdline: [unavailable]\n");
        return;
    }

    size_t len = fread(buffer, 1, sizeof(buffer) - 1, f);
    fclose(f);

    if (len > 0) {
        buffer[len] = '\0';

        /* cmdline is null-separated */
        for (size_t i = 0; i < len; i++) {
            if (buffer[i] == '\0')
                buffer[i] = ' ';
        }

        printf("Cmdline: %s\n", buffer);
    } else {
        printf("Cmdline: [empty]\n");
    }


    // checking to see if there's a parent process
    snprintf(parentPid, sizeof(parentPid), "/proc/%d/status", pid);

    FILE *ftwo = fopen(parentPid, "r");
    if (!ftwo) {
        printf("Parent process ID: [unavailable]\n");
        return;
    }

    len = fread(buffer, 1, sizeof(buffer) - 1, ftwo);
    fclose(ftwo);

    if (len > 0) {
        buffer[len] = '\0';

        /* cmdline is null-separated */
        for (size_t i = 0; i < len; i++) {
            if (buffer[i] == '\0')
                buffer[i] = ' ';
        }

        // split into lines
        char *token = strtok(buffer, "\n");

        while(token != NULL) {
            if (strstr(token, "PPid")){
                printf("%s\n", token);

            }
            token = strtok(NULL, "\n"); // get next token
        }
    } else {
        printf("Cmdline: [empty]\n");
    }
}

int main(int argc, char *argv[])
{

    char logFile[512];

    if (argc < 2) {
        fprintf(stderr, "Minimal usage: %s <file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int i = 0;
    while (argv[i] != NULL){
        if (strstr(argv[i], "-o")) {
            // output logs to another file
            strcpy(logFile, argv[++i]);
        }
        i++;
    }

    const char *path = argv[1];

    /* Initialize fanotify in notification mode (no permission events) */
    int fan_fd = fanotify_init(FAN_CLASS_NOTIF | FAN_CLOEXEC,
                               O_RDONLY | O_LARGEFILE);
    if (fan_fd == -1) {
        perror("fanotify_init");
        return EXIT_FAILURE;
    }

    /* Add mark for the specific file */
    if (fanotify_mark(fan_fd,
                      FAN_MARK_ADD,
                      FAN_OPEN | FAN_CLOSE_WRITE | FAN_MODIFY,
                      AT_FDCWD,
                      path) == -1) {
        perror("fanotify_mark");
        close(fan_fd);
        return EXIT_FAILURE;
    }

    printf("Watching %s ...\n", path);

    char fileData[2048];
    FILE* tmpFile = fopen(path, "r"); // Going to keep the file open so we can read from it.. hopefully this doesn't cause a loop...
    if (!(tmpFile == NULL)){
        while (fgets(fileData, sizeof(fileData), tmpFile) != NULL) { 
            if (logFile == NULL || logFile[0] == '\0')
                printf("%s", fileData); // Print the line
            else {
                FILE *logFilePtr = fopen(logFile, "a");
                if (!(logFilePtr == NULL)){
                    char output[4096];
                    snprintf(output, sizeof(output), "original contents:'\n%s", fileData);
                    fputs(output, logFilePtr);
                    // logged contents to output file
                    fclose(logFilePtr);
                }

            } 
        }
    }
    fclose(tmpFile);

    char buffer[BUF_SIZE];

    while (1) {
        ssize_t len = read(fan_fd, buffer, sizeof(buffer));
        if (len == -1) {
            if (errno == EINTR)
                continue;
            perror("read");
            break;
        }

        struct fanotify_event_metadata *metadata;

        for (metadata = (struct fanotify_event_metadata *)buffer;
             FAN_EVENT_OK(metadata, len);
             metadata = FAN_EVENT_NEXT(metadata, len)) {

            if (metadata->vers != FANOTIFY_METADATA_VERSION) {
                fprintf(stderr, "Mismatch of fanotify metadata version\n");
                exit(EXIT_FAILURE);
            }

            if (metadata->fd == FAN_NOFD)
                continue;

            printf("Event detected\n");
            printf("PID: %d\n", metadata->pid);
            print_exe_from_pid(metadata->pid);
            print_cmdline_from_pid(metadata->pid);

            if (metadata->mask & FAN_OPEN)
                printf("Event: OPEN\n");
                
            if (metadata->mask & FAN_MODIFY) {
                printf("Event: MODIFY\n");
                
            }
                
            if (metadata->mask & FAN_CLOSE_WRITE) {
                printf("Event: CLOSE_WRITE\n");
                tmpFile = fopen(path, "r"); // Going to keep the file open so we can read from it.. hopefully this doesn't cause a loop...
                if (!(tmpFile == NULL)){
                    while (fgets(fileData, sizeof(fileData), tmpFile) != NULL) {
                        if (logFile == NULL || logFile[0] == '\0') // checking if we need to log to file or not
                            printf("%s", fileData); // Print the line
                        else {
                            FILE *logFilePtr = fopen(logFile, "a");
                            if (!(logFilePtr == NULL)){
                                
                                char output[4096];
                                snprintf(output, sizeof(output), "New contents:'\n%s", fileData);
                                printf("Writing this to file:%s\n", output);
                                fputs(output, logFilePtr);
                                // logged contents to output file
                                fclose(logFilePtr);
                            }

                        } 
                    }
                }
            }

            print_path_from_fd(metadata->fd);
            printf("\n");

            close(metadata->fd);
        }
    }

    close(fan_fd);
    return EXIT_SUCCESS;
} 


/**
 * 
 * Some instructions on compilation, just incase I forget
 * 
 * #Compille
 * gcc fanotify_watch.c -o fanotify_watch
 * 
 * # run
 * sudo ./fanotify_watch testfile.txt
 * 
 * 
 * A working result will print a bunch of data about the process that is changing the target file
 * and also the parent process if this is found.
 */