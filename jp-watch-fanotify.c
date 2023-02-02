#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/fanotify.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

// stack  
struct node {
  char *data;
  struct node *next;
};
struct node *head = NULL;

// fanotify
int g_fd;

void die(char *msg) {
  perror(msg);
  exit(1);
}

// NOTE: Whoever pushes data must free the data from the pop later.
void push(char *data) {
  struct node *newNode = malloc(sizeof(struct node));// node freed in pop()
  //*newNode->data = strdup(data);
  newNode->data = data;
  newNode->next = head;
  head = newNode;
}

char *pop() {
  if (NULL == head) return NULL;

  struct node *tmp = head;
  char *popped = tmp->data;
  head = tmp->next;
  free(tmp);// free node created in push()

  return popped;
}

void mark(char *path) {
  if (-1 == fanotify_mark(
    g_fd,
    FAN_MARK_ADD// add new marks
    , FAN_MODIFY
    | FAN_ATTRIB// since Linux 5.1
    | FAN_CREATE// since Linux 5.1
    | FAN_DELETE// since Linux 5.1
    | FAN_MOVE// since Linux 5.1, = FAN_MOVED_FROM | FAN_MOVED_TO
    | FAN_ONDIR// require in order to create events when subdirectory entries are modified (i.e., mkdir/rmdir)
    | FAN_EVENT_ON_CHILD// events for the immediate children of marked directories shall be created
    , AT_FDCWD
    , path// to this path
  )) die("Fanotify mark failed");
}

void ls(char *path) {
  mark(path);
  DIR *d = opendir(path);
  struct dirent *e;
  //printf("ls() path=%s\n",path);
  while (NULL != (e = readdir(d))) {
    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;

    char *p = malloc(strlen(path) + strlen(e->d_name) + 2);// p=new path, freed in main()
    sprintf(p, "%s/%s", path, e->d_name);
    //printf("ls() p=%s\n", p);
    printf("p=%s\n",p);

    // fanotify
    mark(p);

    struct stat sb;
    stat(p, &sb);
    if (S_ISDIR(sb.st_mode)) {
      push(p);
    } else {
      free(p);// free path created in ls()
    }
  }
  closedir(d);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    puts("Missing path argument");
    exit(0);
  }

  // fanotify init
  g_fd = fanotify_init(
    FAN_REPORT_DFID_NAME// = FAN_REPORT_DIR_FID | FAN_REPORT_NAME
    , O_RDONLY
  );
  if (-1 == g_fd) die("Fanotify init failed");
  
  // fanotify add marks
  puts("Setting up watches...");
  push(strdup(argv[1]));
  while (NULL != head) {
    char *c = pop();
    //printf("main() c=%s\n",c);
    ls(c);
    free(c);// free path created in ls()
  }

  // fanotify process events
  puts("Processing events...");
  struct fanotify_event_metadata b[4096];// buffer
  ssize_t n;
  while ((n = read(g_fd, &b, sizeof(b))) > 0) {
    // process the events here
    //puts(b);
    printf("main() read() n=%ld\n", n);
    fflush(stdout);

    struct fanotify_event_metadata *m = (struct fanotify_event_metadata *)b;
    puts("a");
    while (FAN_EVENT_OK(m, n)) {
      printf("event_len=%u, vers=%u, reserved=%u, metadata_len=%u, mask=%llu, fd=%d, pid=%d\n"
        , m->event_len
        , m->vers
        , m->reserved
        , m->metadata_len
        , m->mask
        , m->fd
        , m->pid
      );
      if (m->mask & FAN_ACCESS) puts("FAN_ACCESS");
      if (m->mask & FAN_MODIFY) puts("FAN_MODIFY");
      if (m->mask & FAN_ATTRIB) puts("FAN_ATTRIB");
      if (m->mask & FAN_CREATE) puts("FAN_CREATE");
      if (m->mask & FAN_DELETE) puts("FAN_DELETE");
      if (m->mask & FAN_ONDIR) puts("FAN_ONDIR");
      if (m->mask & FAN_EVENT_ON_CHILD) puts("FAN_EVENT_ON_CHILD");
      puts("b");
      m = FAN_EVENT_NEXT(m, n);
    }
  }
  die("Fanotify read failed");

  //pause(); 
}

/*
-----------------------------
fanotify_event_metadata->mask
-----------------------------
#define FAN_ACCESS		      0x00000001	// File was accessed
#define FAN_MODIFY		      0x00000002	// File was modified
#define FAN_ATTRIB		      0x00000004	// Metadata changed
#define FAN_CLOSE_WRITE		  0x00000008	// Writtable file closed
#define FAN_CLOSE_NOWRITE	  0x00000010	// Unwrittable file closed
#define FAN_OPEN		        0x00000020	// File was opened
#define FAN_MOVED_FROM		  0x00000040	// File was moved from X
#define FAN_MOVED_TO		    0x00000080	// File was moved to Y
#define FAN_CREATE		      0x00000100	// Subfile was created
#define FAN_DELETE		      0x00000200	// Subfile was deleted
#define FAN_DELETE_SELF		  0x00000400	// Self was deleted
#define FAN_MOVE_SELF		    0x00000800	// Self was moved
#define FAN_OPEN_EXEC		    0x00001000	// File was opened for exec

#define FAN_Q_OVERFLOW		  0x00004000	// Event queued overflowed
#define FAN_FS_ERROR		    0x00008000	// Filesystem error

#define FAN_OPEN_PERM		    0x00010000	// File open in perm check
#define FAN_ACCESS_PERM		  0x00020000	// File accessed in perm check
#define FAN_OPEN_EXEC_PERM  0x00040000	// File open/exec in perm check

#define FAN_EVENT_ON_CHILD	0x08000000	// Interested in child events

#define FAN_RENAME		      0x10000000	// File was renamed

#define FAN_ONDIR		        0x40000000	// Event occurred against dir
-----------------------------
*/