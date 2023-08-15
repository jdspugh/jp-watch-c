#if defined(__APPLE__)
  #include <CoreServices/CoreServices.h>
  #include <dispatch/dispatch.h>
#else
  #define _GNU_SOURCE// required for open_by_handle_at()
  #include <sys/fanotify.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

void die(char *msg) { fprintf(stderr, "%s: %s (%d).\n", msg, strerror(errno), errno); exit(1); }
void die_memory() { die("Out of memory"); }

#if defined(__APPLE__)
// -----
// APPLE
// -----
char gPathBuffer[PATH_MAX];

static void callback(
  ConstFSEventStreamRef streamRef,
  void *clientCallBackInfo,
  size_t numEvents,
  void *eventPaths,
  const FSEventStreamEventFlags eventFlags[],
  const FSEventStreamEventId eventIds[]
) {
  for (unsigned long i=numEvents;i--;) {
    CFStringGetCString(CFArrayGetValueAtIndex(eventPaths,i),gPathBuffer,sizeof(gPathBuffer),kCFStringEncodingUTF8);
#ifdef DEBUG
    printf("%u %s%s\n",(unsigned int)eventFlags[i],gPathBuffer,eventFlags[i]&kFSEventStreamEventFlagItemIsDir?"/":"");
#endif
    fputs(gPathBuffer, stdout);
    if (eventFlags[i] & kFSEventStreamEventFlagItemIsDir) putchar('/');
    putchar('\n');
  }
  fflush(stdout);
}

void fsstream_process_events(int paths_n, char *paths[]) {
  // put paths params into Core Foundation (CF) Array
  CFMutableArrayRef cf_paths = CFArrayCreateMutable(kCFAllocatorDefault, paths_n, &kCFTypeArrayCallBacks);
  for (uint i = 1; i < paths_n; i++) {
    CFStringRef p = CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, paths[i], kCFStringEncodingUTF8, kCFAllocatorDefault);
    CFArrayAppendValue(cf_paths, p);
  }

  // monitor the path recursively (and also keep track of new files/folders created within it)
  FSEventStreamRef stream = FSEventStreamCreate(
    NULL,// memory allocator (NULL=default)
    &callback,// FSEventStreamCallback
    NULL,// context
    cf_paths,// paths to watch
    kFSEventStreamEventIdSinceNow,// since when
    0,// latency (seconds)
    kFSEventStreamCreateFlagUseCFTypes// The framework will invoke your callback function with CF types rather than raw C types (i.e., a CFArrayRef of CFStringRefs, rather than a raw C array of raw C string pointers). See FSEventStreamCallback.
    | kFSEventStreamCreateFlagFileEvents// Request file-level notifications. Your stream will receive events about individual files in the hierarchy you're watching instead of only receiving directory level notifications. Use this flag with care as it will generate significantly more events than without it.
  );
  dispatch_queue_t d = dispatch_queue_create("jp-watch", NULL);
  FSEventStreamSetDispatchQueue(stream, d);
  FSEventStreamStart(stream);

#ifdef DEBUG
  sleep(10);// sleep so can see the results of "leaks" for detecting memory leaks
#else
  pause();// The pause function suspends program execution until a signal arrives whose action is either to execute a handler function, or to terminate the process.
#endif

  // free memory and resources
  //   paths
  for (uint i = 0; i < paths_n - 1; i++) CFRelease(CFArrayGetValueAtIndex(cf_paths, i));
  CFRelease(cf_paths);
  //   stream
  dispatch_release(d);
  FSEventStreamStop(stream);
  FSEventStreamInvalidate(stream);
  FSEventStreamRelease(stream);
}
// -----
#else
// -----
// Linux
// -----
void fanotify_process_events(int paths_n, char *paths[]) {
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
  )) {
    die((1 == errno) ? "Must run as superuser on Linux systems" : "Fanotify mark failed");
  };

  char b[8192];// byte buffer (docs recommend a big one e.g. 4096)
  char p[PATH_MAX];// /proc/self/fd/%d path
  char f[PATH_MAX];// filename
#ifdef __ssize_t_defined
  ssize_t n;
#else
  int n;// some non-POSTIX Linux versions (e.g. ChromeOS) use int
#endif
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

      // filter by paths
      int i;
      for (i = 1; i < paths_n; i++) {
        if (strncmp(f, paths[i], strlen(paths[i])) == 0) break;// matching prefix found, so stop comparing
      }
      if (paths_n != i) {// in the path set?
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
    fflush(stdout);
  }
  die("Fanotify mark failed");
}
// -----
#endif

void handle_args(int argc, char *argv[]) {
  // print help, if requested
  for (int i = 1; i < argc; i++) {
    if (0 == strcmp("--help", argv[i])) {
      printf(
"jp-notify v0.8.1\n"
"\n"
"Usage:\n"
"jp-notify [PATH1] [PATH2] ...\n"
"\n"
"Description:\n"
"This command recursively watches the specified paths for changes to the files and/or directories they point to and their attributes. It prints the path when a change occurs. If no paths are specified it will use the current working directory.\n"
"\n"
"Note:\n"
"Must run as superuser on Linux systems. This is a limitation of the Linux kernel's fanotify system.\n"
      );
      exit(0);
    }
  }
}

int main(int argc, char *argv[]) {
  handle_args(argc, argv);

  uint paths_n = argc;// number of paths
  char **paths = argv;// first path always ignored
#ifdef DEBUG
  printf("argc=%d\n", argc);
#endif

  // deal with no supplied args
  if (1 == argc) {
    paths_n = 2; 
    paths = malloc(sizeof(*paths) * paths_n);
    if (!paths) die_memory();
    paths[1] = ".";
  }

  {
    // convert all paths to absolute paths (so we can handle path args like "../../")
    char p[PATH_MAX];// absolute path
    for (uint i = 1; i < paths_n; i++) {
      (void)realpath(paths[i], p);// doesn't seem to return null ever
      paths[i] = malloc(strlen(p) + 1);// TODO: replace malloc+strcpy with strdup
      if (!paths[i]) die_memory();
      strcpy(paths[i], p);
    }
  }

#ifdef DEBUG
  for (uint i = 1; i < paths_n; i++) printf("paths[%d]=%s\n", i, paths[i]);
  fflush(stdout);
#endif

#if defined(__APPLE__)
  fsstream_process_events(paths_n, paths);
#else
  fanotify_process_events(paths_n, paths);
#endif

  if (1 == argc) free(paths);

  return 0;
}
