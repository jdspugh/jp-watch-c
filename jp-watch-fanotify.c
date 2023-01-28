#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/fanotify.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
// Debug
//   $ gcc -g jp-watch-fanotify.c
// Production
//   $ gcc -Oz jp-watch-fanotify.c

#define BUF_LEN 4096

int g_root_fd;// file descriptor of the filesystem to monitor
int g_fanotify_fd;// fanotify file descriptor
char g_path[4096];// path from command line argument
char g_path_buffer[4096];

void die(char *msg){fprintf(stderr,"%s: (%d) ",msg,errno);perror(NULL);exit(1);}// exit (with error msg)

void cleanup() {
  puts("Cleanup...");
  close(g_fanotify_fd);
  fanotify_mark(g_fanotify_fd,FAN_MARK_FLUSH,0,g_root_fd,0);
  puts("Done!");
}

int main(int argc, char *argv[]) {
  //if(argc<2)die("Missing directory argument");
  puts("Starting...");

  // init fanotify
  g_fanotify_fd=fanotify_init(
        FAN_UNLIMITED_QUEUE// remove the limit of 16384 events for the event queue
      | FAN_UNLIMITED_MARKS// remote the limit of 8192 marks
      | FAN_REPORT_DFID_NAME
    , O_RDONLY// read only
  );
  if(-1==g_fanotify_fd){die("Fanotify init failed");}else{atexit(cleanup);}
  
  // fanotify mark
  g_root_fd=open(".",O_RDONLY);
  if(-1==g_root_fd)die("Opening root directory failed");
  if(-1==fanotify_mark(
      g_fanotify_fd
    , FAN_MARK_ADD
      //| FAN_MARK_MOUNT// FAN_MARK_ADD also required for this to work (note a bunch of events won't work with a mounted file system)
    , FAN_MODIFY 
//      | FAN_OPEN// require to report directory creation and deletion
      | FAN_ATTRIB | FAN_CREATE | FAN_DELETE// (since Linux 5.1 <-- not supported in Ubutnu 22)
//      | FAN_ONDIR// require in order to create events when subdirectory entries are modified (i.e., mkdir/rmdir)
//      | FAN_EVENT_ON_CHILD// events for the immediate children of marked directories shall be created
      // | FAN_MOVE | FAN_MOVED_FROM | FAN_MOVED_TO// (not compatible with FAN_MARK_MOUNT)
      // untested: FAN_DELETE_SELF | FAN_MOVE_SELF
    , g_root_fd
    , NULL
  ))die("Fanotify mark failed");

  char buf[BUF_LEN];
  ssize_t numRead;
  while(numRead=read(g_fanotify_fd,buf,BUF_LEN)>0) {
    fputs(".",stdout);fflush(stdout);
    /*for(char *p=buf;p<buf+numRead;) {
      struct fanotify_event *event=(struct fanotify_event *)p;
      if(event->len) {
        //sprintf(g_path_buffer,"%s/%s",g_path, event->name);
        //printf("%d %s\n",event->mask,g_path_buffer);
      }
      p+=sizeof(struct fanotify_event)+event->len;
    }*/
  }

  //sleep(60);
  pause();
}