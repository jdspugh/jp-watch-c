#define _GNU_SOURCE// required for open_by_handle_at()
#include <stdlib.h>
#include <stdio.h>
#include <sys/fanotify.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>

void die(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char *argv[]) {
  int fd = fanotify_init(
    FAN_CLASS_NOTIF// set by default (doesn't require admin privledges)
    | FAN_REPORT_FID 
    //| FAN_REPORT_DFID_NAME// = FAN_REPORT_DIR_FID | FAN_REPORT_NAME  <-- with this setting events return dir name rather than file name
    //| FAN_REPORT_TARGET_FID// additional information about the child correlated with directory entry modification events
    //| FAN_REPORT_DFID_NAME_TARGET
    , O_RDONLY
  );
  if (-1 == fd) die("Fanotify init failed");
  
  if (-1 == fanotify_mark(
    fd,
    FAN_MARK_ADD// add new marks
    | FAN_MARK_FILESYSTEM// monitor the whole filesystem
    , FAN_MODIFY
    | FAN_ATTRIB// since Linux 5.1
    | FAN_CREATE// since Linux 5.1
    | FAN_DELETE// since Linux 5.1
    | FAN_MOVE// since Linux 5.1, = FAN_MOVED_FROM | FAN_MOVED_TO
    | FAN_ONDIR// require in order to create events when subdirectory entries are modified (i.e., mkdir/rmdir)
    | FAN_EVENT_ON_CHILD// events for the immediate children of marked directories shall be created
    , AT_FDCWD
    , "/"// path (not needed cause monitoring the whole file system)
  )) die("Fanotify mark failed");

  // fanotify process events
  char b[8192];// byte buffer
  ssize_t n;
  while ((n = read(fd, &b, sizeof(b))) > 0) {
    struct fanotify_event_metadata *m = (struct fanotify_event_metadata *)b;
    while (FAN_EVENT_OK(m, n)) {
      struct fanotify_event_info_fid *fid = (struct fanotify_event_info_fid *)(m + 1);
      // get fd from handle
      int event_fd = open_by_handle_at(AT_FDCWD, (struct file_handle *)fid->handle, O_RDONLY);

      // get filename from fd
      char p[PATH_MAX];// /proc/self/fd/%d path
      char f[PATH_MAX];// filename
      sprintf(p, "/proc/self/fd/%d", event_fd);
      int l = readlink(p, f, PATH_MAX);
      f[l] = '\0';

      fputs(f, stdout);
      if (m->mask & FAN_ONDIR) putchar('/');
      putchar('\n');
      //if (m->mask & FAN_ACCESS) puts("FAN_ACCESS");
      //if (m->mask & FAN_MODIFY) puts("FAN_MODIFY");
      //if (m->mask & FAN_ATTRIB) puts("FAN_ATTRIB");
      //if (m->mask & FAN_CREATE) puts("FAN_CREATE");
      //if (m->mask & FAN_DELETE) puts("FAN_DELETE");
      //if (m->mask & FAN_MOVE) puts("FAN_MOVE");
      //if (m->mask & FAN_ONDIR) { puts("/"); puts("FAN_ONDIR"); } else { putchar('\n'); }
      //if (m->mask & FAN_EVENT_ON_CHILD) puts("FAN_EVENT_ON_CHILD");
      m = FAN_EVENT_NEXT(m, n);
    }
  }
  die("Fanotify read failed");
}