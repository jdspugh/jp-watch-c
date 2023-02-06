#define _GNU_SOURCE// required for open_by_handle_at()
#include <stdlib.h>
#include <stdio.h>
#include <sys/fanotify.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
// Usage:
//   $ gcc -O3 fanotify.c -o jp-notify
//   $ sudo ./jp-notify

void die(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char *argv[]) {
  // help
  for (int i = 1; i < argc; i++) {
    if (0 == strcmp("--help", argv[i])) {
      printf(
"jp-notify v0.0.1\n"
"\n"
"Usage: sudo jp-notify [ DIR1 | FILE1 ] [ DIR2 | FILE2 ] [ ... ]\n"
"\n"
"Arguments:\n"
"  DIR | FILE        One or more directories to watch. If no directories are specified, the current working directory will be used.\n"
"\n"
"Description:\n"
"  This command recursively watches the specified directories for changes to files and/or directories or their attributes. It prints the file/directory path and name when a change occurs. If no directories are specified, it will use the current working directory.\n"
"\n"
"  Note: This command needs to be run as the superuser.\n"
      );
      return 0;
    }
  }

  // if no args, set default path to cwd
  char *cwd;
  if (1 == argc) {
    cwd = (char *)malloc(PATH_MAX);
    getcwd(cwd, PATH_MAX);
    argc = 2; 
    argv[1] = cwd;
  }

  // fanotify init
  int fd = fanotify_init(FAN_REPORT_FID, O_RDONLY);
  if (-1 == fd) die("Fanotify init failed");
  
  // fanotify mark
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
    , "/"// path to monitor
  )) die("Fanotify mark failed");

  // fanotify process events
  char b[8192];// byte buffer (docs recommend a big one e.g. 4096)
  char p[PATH_MAX];// /proc/self/fd/%d path
  char f[PATH_MAX];// filename
  ssize_t n;
  while ((n = read(fd, &b, sizeof(b))) > 0) {
    struct fanotify_event_metadata *m = (struct fanotify_event_metadata *)b;
    while (FAN_EVENT_OK(m, n)) {
      struct fanotify_event_info_fid *fid = (struct fanotify_event_info_fid *)(m + 1);
      // get fd from handle
      int event_fd = open_by_handle_at(AT_FDCWD, (struct file_handle *)fid->handle, O_RDONLY);

      // get filename from fd
      sprintf(p, "/proc/self/fd/%d", event_fd);
      int l = readlink(p, f, PATH_MAX);
      f[l] = '\0';

      // filter by paths in argv
      int i;
      for (i = 1; i < argc; i++) {
        if (strncmp(f, argv[i], strlen(argv[i])) == 0) break;// matching prefix found, so stop comparing
      }
      if (argc != i) {// in the path set?
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
      }

      m = FAN_EVENT_NEXT(m, n);
    }
  }
  die("Fanotify read failed");
}
