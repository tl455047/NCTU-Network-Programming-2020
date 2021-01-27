#ifndef _NPSHELL_SINGLE_PROC_H_
#define _NPSHELL_SINGLE_PROC_H_

#include <iostream>
#include <map>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

//maximum user number
#define MAXIMUM_USER 30
//struct for process
struct proc{
  char* command;
  int argc;
  pid_t pid;
  struct proc* next;
};

typedef struct proc proc;

//struct for pipe and number pipe
//pipe type = '|', normal type
//pipe type = '>', output file
//pipe type = 'N', number pipe stdout 
//pipe type = '!', number pipe stdout, stderr
//pipe type = '<', read user pipe
//pipe type = 'U', write user pipe
struct pipeN{
  int fd[2];
  int count;
  char type;
  struct pipeN* next;
};

typedef struct pipeN pipeN;

//for client struct
struct client {
  //id
  int id;
  //sockfd
  int fd;
  //nickname
  std::string name;
  //ip
  std::string ip;
  //port
  std::string port;
  //indicate me
  //std::string indicate_me;
  //active
  int active;
  //number pipe list
  pipeN* pipeN_list_head;
  //environment variable
  std::map<std::string, std::string> env_map;
};
typedef struct client client;

int npShell_one_commmand_line(struct client* client, std::vector<struct client> &clients, int (*user_pipe)[MAXIMUM_USER]);
//child handler
void childHandler(int signo);
//create new process list with command
proc* createProcList(char* command);
//find the last process in linked list
proc* getProcListTail(proc *p);
//append process linked list, with linked list head and command
proc* addProc(proc* p, char* command);
//set last process's argc
void setProcArgc(proc* p, int argc);
//print every process
void printProcList(proc* p);
//free all element in linked list
proc* deleteProcList(proc* p);
//pipefd[0] refers to the read end of the pipe.
//pipefd[1] refers to the write end of the pipe.
//create new pipe list with type and number pipe N
pipeN* createPipeList(char type, int n);
//find the last element in list
pipeN* getPipeListTail(pipeN *p);
//add element to list with head of list, type, and N
pipeN* addPipe(pipeN* p, char type, int n);
//print all pipe
void printPipeList(pipeN* p);
//free all element in pipe list
pipeN* deletePipeList(pipeN* p);
//set pipe fd with head of list and fd[2] array
void openPipe(pipeN* p, int fd[2]);
//create a number pipe list with a pipeN struct
pipeN* createPipeNList(pipeN* p);
//add a number pipe with head of list and a pipeN struct
pipeN* addPipeN(pipeN* p1, pipeN* p2);
//delete certain pipeN which count = 0 in list 
pipeN* deletePipeN(pipeN* p);
//set process pid
void setProcPid(proc* p, pid_t pid);
//Show information of all users
void who(int fd, std::vector<struct client> &clients); 
//send message to certain user
void tell(int fd, char* message);
//broadcast message
void broadcastMessage(std::string message, std::vector<client> &clients);
// print user not exit message
void printUserNotExistMessage(int fd, int id);
//readline
ssize_t readLine(int fd, char* buffer, size_t n);
#endif