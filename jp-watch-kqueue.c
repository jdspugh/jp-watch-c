#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
// kqueue
#include <sys/event.h>
#include <sys/types.h>
#include <sys/time.h>

void watch_directory(const char* path) {
    int kq = kqueue();
    if (kq == -1) {
        perror("kqueue");
        exit(1);
    }

    struct kevent change;
    EV_SET(&change, open(path, O_RDONLY), EVFILT_VNODE,
           EV_ADD | EV_CLEAR, NOTE_WRITE | NOTE_RENAME | NOTE_DELETE, 0, 0);

    struct timespec timeout = {1, 0}; // 1 second timeout
    for (;;) {
        int nchanges = kevent(kq, &change, 1, NULL, 0, &timeout);
        if (nchanges == -1) {
            perror("kevent");
            exit(1);
        } else if (nchanges > 0) {
            printf("Directory %s changed\n", path);
        }
    }
}