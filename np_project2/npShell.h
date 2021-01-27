#ifndef _NPSHELL_H_
#define _NPSHELL_H_

#include <stdio.h>
#include <stdlib.h>

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
struct pipeN{
  int fd[2];
  int count;
  char type;
  struct pipeN* next;
};

typedef struct pipeN pipeN;
//npshell 
int npShell();
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

#endif