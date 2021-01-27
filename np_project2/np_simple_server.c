#include <stdio.h>
#include <string.h>
#include <sys/types.h>      
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include "npShell.h"  

int main(int argc, char *argv[]) {
  //port number
  int port_num = 7001;
  //socket address struct for client and server
  struct sockaddr_in c_addr, s_addr;
  //fd for master socket and slave socket
  int msock, ssock;
  //listen queue size
  int qlen = 0;
  int status;
  //len for accept
  socklen_t c_addr_len;
  //set argv[1] as port number
  if(argc == 2) {
    port_num = atoi(argv[1]);
  }
  printf("port: %d\n", port_num);
  //socket option
  int option = 1;
  //set server sock address
  memset(&s_addr, 0, sizeof(s_addr));
  s_addr.sin_family = AF_INET;
	s_addr.sin_addr.s_addr = INADDR_ANY;
  s_addr.sin_port = htons(port_num);
  //create socket
  msock = socket(AF_INET, SOCK_STREAM, 0);
  if(msock < 0)
    fprintf(stderr, "cannot create socket\n");
  //set socket address reused option
  if(setsockopt(msock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0) {
    fprintf(stderr, "cannot set socket option\n");
  }
  //bind socket to port
  if(bind(msock, (struct sockaddr *)&s_addr, sizeof(s_addr)) < 0) 
    fprintf(stderr, "cannot bind socket to port %d\n", port_num);
  //listen to port, and prepare request queue
  if(listen(msock, qlen) < 0)
    fprintf(stderr, "cannot listen on port %d\n", port_num);
  //Concurrent, Connection-Oriented Servers
  while(1) {
    memset(&c_addr, 0, sizeof(c_addr));
    c_addr_len = sizeof(c_addr);
    //accept connection request
    ssock = accept(msock, (struct sockaddr *)&c_addr, &c_addr_len);
    if(ssock < 0) 
      fprintf(stderr, "cannot create slave socket\n");
    //check process limit, if close to limit, need to wait to release
    //fork for slave socket
    pid_t pid = fork();
    while( pid < 0 ) {
      waitpid(-1, &status, 0);
      pid = fork();   
    } 
    if(pid == 0) {
      //child process
      //slave socket
      close(msock);
      //redirect stdin, stdout, stderr to slave socket
      close(0);
      dup(ssock);
      close(1);
      dup(ssock);
      close(2);
      dup(ssock);
      close(ssock);
      //execute npshell
      npShell();
      
      exit(0);
    }
    else if(pid > 0) {
      //parent process
      //master socket
      close(ssock);
    }

    waitpid(pid, &status, 0);
    
  }

  return 0;
}
