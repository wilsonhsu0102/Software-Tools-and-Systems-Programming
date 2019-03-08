#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAXLINE 256
#define MAX_PASSWORD 10

#define SUCCESS "Password verified\n"
#define INVALID "Invalid password\n"
#define NO_USER "No such user\n"

int main(void) {
  char user_id[MAXLINE];
  char password[MAXLINE];

  if(fgets(user_id, MAXLINE, stdin) == NULL) {
      perror("fgets");
      exit(1);
  }
  if(fgets(password, MAXLINE, stdin) == NULL) {
      perror("fgets");
      exit(1);
  }
  int fd[2];
  int stat;
  if (pipe(fd) == -1) {
    perror("pipe");
    exit(1);
  }
  int r = fork();
  if (r < 0) {
    perror("fork");
    exit(1);
  }
  else if (r == 0) {
    if (close(fd[1]) == -1) {
      perror("close");
      exit(1);
    }
    if (dup2(fd[0], STDIN_FILENO) == -1) {
      perror("dup2");
      exit(1);
    }
    if (execl("./validate", "validate", NULL) == -1) {
      perror("execl");
      exit(1);
    }
    if (close(fd[0]) == -1) {
      perror("close");
      exit(1);
    }
    
  } else {
    if (close(fd[0]) == -1) {
      perror("close");
      exit(1);
    }
    if (write(fd[1], user_id, MAX_PASSWORD) != MAX_PASSWORD) {
      perror("write");
      exit(1);
    } 
    if (write(fd[1], password, MAX_PASSWORD) != MAX_PASSWORD) {
      perror("write");
      exit(1);
    }
    if (wait(&stat) == -1) {
      perror("wait");
      exit(1);
    }
    int wait_stat = WEXITSTATUS(stat);
    if (wait_stat == 0) {
      printf(SUCCESS);
    } else if (wait_stat == 1) {
      perror("validate");
      exit(1);
    } else if (wait_stat == 2) {
      printf(INVALID);
    } else {
      printf(NO_USER);
    }
    if (close(fd[1]) == -1) {
      perror("close");
      exit(1);
    }
  }
  return 0;
}
