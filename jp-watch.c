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

  //// free memory and resources
  ////   paths
  //for (uint i = 0; i < paths_n - 1; i++) CFRelease(CFArrayGetValueAtIndex(cf_paths, i));
  //CFRelease(cf_paths);
  ////   stream
  //dispatch_release(d);
  //FSEventStreamStop(stream);
  //FSEventStreamInvalidate(stream);
  //FSEventStreamRelease(stream);
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
    | FAN_ONDIR// required in order to create events when subdirectory entries are modified (i.e., mkdir/rmdir)
    | FAN_EVENT_ON_CHILD// events for the immediate children of marked directories shall be created
    , AT_FDCWD
    , "/"// path to monitor (path not used in whole filesystem mode i.e. with FAN_MARK_FILESYSTEM)
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
  //const char *d = " (deleted)";// fanotify "deleted" annotation
  while ((n = read(fd, &b, sizeof(b))) > 0) {
    struct fanotify_event_metadata *m = (struct fanotify_event_metadata *)b;
    while (FAN_EVENT_OK(m, n)) {// more events in buffer b to process?
      // TODO: consider optimising by removing duplicate paths generated within this while loop (rather than sending them to the stdout and expecting the user to filter them out)

      // get filename from event
      struct fanotify_event_info_fid *fid = (struct fanotify_event_info_fid *)(m + 1);

// //printf("fsid: type=%d, val={%d, %d}\n", fid->fsid.type, fid->fsid.val[0], fid->fsid.val[1]);
// //printf("file_handle: ");
// //for (int i = 0; i < handle_bytes; i++) {
// //  printf("%02x ", fid->file_handle[i]);
// //}
// //printf("(fid=%d,%d)", fid->hdr, fid->fsid);
// printf("fid->hdr.info_type=%d, ", fid->hdr.info_type);
// printf("fid->hdr.pad=%d, ", fid->hdr.pad);
// printf("fid->hdr.len=%d, ", fid->hdr.len);
// //printf("fid->fsid=%d,%d, ", fid->fsid.val[0], fid->fsid.val[1]);// filesystem identifier

// struct file_handle *handle = (struct file_handle *)fid->handle;
// printf("fid->handle.handle_bytes=%d, ", handle->handle_bytes);
// printf("fid->handle.handle_type=%d, ", handle->handle_type);
// //printf("fid->handle.handle_bytes=%s, ", fid->handle.handle_bytes);

      int event_fd = open_by_handle_at(AT_FDCWD, (struct file_handle *)fid->handle, O_RDONLY);// get fd from handle
// printf("event_fd=%d", event_fd);
// printf("\n");
      if (-1 != event_fd) {// -1 returned if the file was deleted before this code could be reached
      f[0]='\0';// if info_type is -1 it returns the last filename unless the string is cleared here 
      sprintf(p, "/proc/self/fd/%d", event_fd);// get filename from fd
      ssize_t l = readlink(p, f, PATH_MAX);
// printf("l=%ld, ",l);
        if (l>1 || !(m->mask & FAN_ATTRIB)) {// don't process attribute changes on "/" since these events are erroneously returned upon any file deletion events before the either the file or file's parent directory is returned.
          f[l] = '\0';// null terminate to convert to C string for printing

          // filter by paths
          int i;
          for (i = 1; i < paths_n; i++) {
            if (strncmp(f, paths[i], strlen(paths[i])) == 0) break;// matching prefix found, so stop comparing
          }
          if (paths_n != i) {// in the path set?
            //// remove any fanotify "deleted" annotation
            //char *found = strstr(f, d);
            ////if (found) *found = '\0';// terminate the string at the start of the annotation

            // output path
            fputs(f, stdout);
            if (m->mask & FAN_ONDIR) putchar('/');
            putchar('\n');
            //fflush(stdout);
            //if (m->mask & FAN_ACCESS) puts("FAN_ACCESS");
            //if (m->mask & FAN_MODIFY) puts("FAN_MODIFY");
            //if (m->mask & FAN_ATTRIB) puts("FAN_ATTRIB");
            //if (m->mask & FAN_CREATE) puts("FAN_CREATE");
            //if (m->mask & FAN_DELETE) puts("FAN_DELETE");
            //if (m->mask & FAN_MOVE) puts("FAN_MOVE");
            //if (m->mask & FAN_ONDIR) { puts("/"); puts("FAN_ONDIR"); } else { putchar('\n'); }
            //if (m->mask & FAN_EVENT_ON_CHILD) puts("FAN_EVENT_ON_CHILD");
          }// if
        }// if
      }// if
      close(event_fd);// free fd

      m = FAN_EVENT_NEXT(m, n);// return pointer to the start of the next event in buffer b
    }// while
    fflush(stdout);// flush output after all events in buffer b are read
  }// while
  die("Fanotify mark failed");
}// function
// -----
#endif

void handle_args(int argc, char *argv[]) {
  // print help, if requested
  for (int i = 1; i < argc; i++) {
    if (0 == strcmp("--help", argv[i])) {
      printf(
"jp-notify v0.8.2\n"
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
      paths[i] = strdup(p);
      if (!paths[i]) die_memory();
    }
  }

#ifdef DEBUG
  // print paths
  for (uint i = 1; i < paths_n; i++) printf("paths[%d]=%s\n", i, paths[i]);
  fflush(stdout);
#endif

#if defined(__APPLE__)
  fsstream_process_events(paths_n, paths);// never returns
#else
  fanotify_process_events(paths_n, paths);// never returns
#endif

  //// free paths memory
  //for (uint i = paths_n-1; i--;) free(paths[i]);
  //if (1 == argc) free(paths);

  return 0;
}