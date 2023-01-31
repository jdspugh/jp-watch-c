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

void ls(char *path) {
  DIR *d = opendir(path);
  struct dirent *e;
  //printf("ls() path=%s\n",path);
  while (NULL != (e = readdir(d))) {
    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;

    char *p = malloc(strlen(path) + strlen(e->d_name) + 2);// p=new path, freed in main()
    sprintf(p, "%s/%s", path, e->d_name);
    //printf("ls() p=%s\n", p);
    puts(p);

    // fanotify
    if (-1 == fanotify_mark(
      g_fd, 
      FAN_MARK_ADD// add new marks
      , FAN_MODIFY
      | FAN_ATTRIB// since Linux 5.1
      | FAN_CREATE// since Linux 5.1
      | FAN_DELETE// since Linux 5.1
      | FAN_ONDIR// require in order to create events when subdirectory entries are modified (i.e., mkdir/rmdir)
      | FAN_EVENT_ON_CHILD// events for the immediate children of marked directories shall be created
      , AT_FDCWD
      , p// to this path
    )) die("Fanotify mark failed");

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
  char buf[4096];
  ssize_t n;
  while (n = read(g_fd, buf, sizeof(buf)) > 0) {
    // process the events here
    puts(buf);
    putchar('.');
    fflush(stdout);
  }

  pause();
}