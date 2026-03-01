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

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return EXIT_FAILURE;
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

            if (metadata->mask & FAN_OPEN)
                printf("Event: OPEN\n");

            if (metadata->mask & FAN_MODIFY)
                printf("Event: MODIFY\n");

            if (metadata->mask & FAN_CLOSE_WRITE)
                printf("Event: CLOSE_WRITE\n");

            print_path_from_fd(metadata->fd);
            printf("\n");

            close(metadata->fd);
        }
    }

    close(fan_fd);
    return EXIT_SUCCESS;
}