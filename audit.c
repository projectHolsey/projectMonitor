#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <libaudit.h>

#define AUDIT_KEY "file_monitor_key"

int main()
{
    int audit_fd;
    struct audit_reply reply;
    int rc;

    audit_fd = audit_open();
    if (audit_fd < 0) {
        perror("audit_open");
        exit(EXIT_FAILURE);
    }

    printf("Listening for audit events with key: %s\n", AUDIT_KEY);

    while (1) {
        rc = audit_get_reply(audit_fd, &reply, GET_REPLY_BLOCKING, 0);

        if (rc > 0 && reply.message) {
            if (strstr(reply.message, AUDIT_KEY)) {
                printf("Event:\n%s\n\n", reply.message);
            }
        } else if (rc == -1 && errno != EINTR) {
            perror("audit_get_reply");
            break;
        }
    }

    close(audit_fd);
    return 0;
}