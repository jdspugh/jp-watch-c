#include <stdio.h>
#include <unistd.h>
#include <sys/inotify.h>

#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

char gPathBuffer[4096];

int main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("Missing directory argument\n");
        return 1;
    }

    int inotifyFd = inotify_init();
    if(inotifyFd < 0) {
        perror("inotify_init");
        return 1;
    }

    int wd = inotify_add_watch(inotifyFd, argv[1], IN_ALL_EVENTS);
    if(wd < 0) {
        perror("inotify_add_watch");
        return 1;
    }

    char buf[BUF_LEN];
    ssize_t numRead;
    while((numRead = read(inotifyFd, buf, BUF_LEN)) > 0) {
        for(char *p = buf; p < buf + numRead; ) {
            struct inotify_event *event = (struct inotify_event *) p;
            if(event->len) {
                sprintf(gPathBuffer, "%s/%s", argv[1], event->name);
                printf("%d %s\n", (int)event->mask, gPathBuffer);
            }
            p += sizeof(struct inotify_event) + event->len;
        }
    }

    return 0;
}
