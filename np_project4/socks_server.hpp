#ifndef _SOCKS_SERVER_H_
#define _SOCKS_SERVER_H_
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
//wait for child
void childHandler(int signo) {
  int status;
  while(waitpid(-1, &status, WNOHANG) > 0) {
    
  }
}
#endif