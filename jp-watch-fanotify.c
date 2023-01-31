#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
// This code basically does "ls -R". Next the fanotify code will be added.

struct node {
  char *data;
  struct node *next;
};

struct node *head = NULL;

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
  printf("ls() path=%s",path);
  while (NULL != (e = readdir(d))) {
    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;

    char *p = malloc(strlen(path) + strlen(e->d_name) + 2);// p=new path, freed in main()
    sprintf(p, "%s/%s", path, e->d_name);

    struct stat sb;
    stat(p, &sb);

    if (S_ISDIR(sb.st_mode)) {
      push(p);
    } else {
      printf("%s\n", p);
      free(p);// free path created in ls()
    }
  }
  closedir(d);
}

int main(int argc, char *argv[]) {
  push(strdup(argv[1]));
  while (NULL != head) {
    char *c = pop();
    puts(c);
    ls(c);
    free(c);// free path created in ls()
  }

  return 0;
}