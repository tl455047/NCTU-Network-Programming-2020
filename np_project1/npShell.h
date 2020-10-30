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

//create new process list with command
proc* createProcList(char* command) {
  proc* p = (proc*)malloc(sizeof(proc));
  p->next = NULL;
  p->command = command;
  p->argc = 1;
  return p;
}
//find the last process in linked list
proc* getProcListTail(proc *p) {
  while(p != NULL) {
    if(p->next == NULL)
      break;
    p = p->next;
  }
  return p;
}
//append process linked list, with linked list head and command
proc* addProc(proc* p, char* command) {
  proc* new_proc = (proc*)malloc(sizeof(proc));
  new_proc->next = NULL;
  new_proc->command = command;
  new_proc->argc = 1;
  proc* tail = getProcListTail(p);
  tail->next = new_proc;
  
  return new_proc;
}
//set last process's argc
void setProcArgc(proc* p, int argc) {
  proc* tail = getProcListTail(p);
  tail->argc = argc;
  return;
}
//print every process
void printProcList(proc* p) {
  while(p != NULL) {
    printf("command: %s argc: %d\n", p->command, p->argc);
    p = p->next;
  }
}
//free all element in linked list
proc* deleteProcList(proc* p) {
  while(p != NULL) {
    proc* next_p = p->next;
    free(p);
    p = next_p;
  }
  return NULL;
}
//pipefd[0] refers to the read end of the pipe.
//pipefd[1] refers to the write end of the pipe.
//create new pipe list with type and number pipe N
pipeN* createPipeList(char type, int n) {
  pipeN* p = (pipeN*)malloc(sizeof(pipeN));
  p->type = type;
  if(type == '|' || type == '>') 
    p->count = 0;
  else 
    p->count = n;
  p->next = NULL;
  return p;
}
//find the last element in list
pipeN* getPipeListTail(pipeN *p) {
  while(p != NULL) {
    if(p->next == NULL)
      break;
    p = p->next;
  }
  return p;
}
//add element to list with head of list, type, and N
pipeN* addPipe(pipeN* p, char type, int n) {
  pipeN* new_p = (pipeN*)malloc(sizeof(pipeN));
  new_p->type = type;
  if(type == '|' || type == '>') 
    new_p->count = 0;
  else 
    new_p->count = n;
  new_p->next = NULL;

  pipeN* tail = getPipeListTail(p);
  tail->next = new_p;
  
  return new_p;
}
//print all pipe
void printPipeList(pipeN* p) {
  while(p != NULL) {
    printf("type: %c count: %d ", p->type, p->count);
    printf("read pipe: %d write pipe: %d\n", p->fd[0], p->fd[1]);
    p = p->next;
  }
}
//free all element in pipe list
pipeN* deletePipeList(pipeN* p) {
  while(p != NULL) {
    pipeN* next_p = p->next;
    free(p);
    p = next_p;
  }
  return NULL;
}
//set pipe fd with head of list and fd[2] array
void openPipe(pipeN* p, int fd[2]) {
  if(p != NULL) {
    p->fd[0] = fd[0];
    p->fd[1] = fd[1];
  }
}
//create a number pipe list with a pipeN struct
pipeN* createPipeNList(pipeN* p) {
  pipeN* piN = (pipeN*)malloc(sizeof(pipeN));
  piN->type = p->type;
  piN->count = p->count;
  piN->next = NULL;
  piN->fd[0] = p->fd[0];
  piN->fd[1] = p->fd[1];
  return piN;
}
//add a number pipe with head of list and a pipeN struct
pipeN* addPipeN(pipeN* p1, pipeN* p2) {
  pipeN* new_p = (pipeN*)malloc(sizeof(pipeN));
  new_p->type = p2->type;
  new_p->count = p2->count;
  new_p->next = NULL;
  new_p->fd[0] = p2->fd[0];
  new_p->fd[1] = p2->fd[1];
  pipeN* tail = getPipeListTail(p1);
  
  tail->next = new_p;
  
  return new_p;
}
//delete certain pipeN which count = 0 in list 
pipeN* deletePipeN(pipeN* p) {
  pipeN* pre = NULL;
  pipeN* head = p;
  while(p != NULL) {
    
    if(p->count == 0) {
      if(pre != NULL) {
        pre->next = p->next;
      }
      else {
        head = p->next;
      }
      free(p);
      break;
    }
    pre = p;
    p = p->next;

  }
  return head;
}
//set process pid
void setProcPid(proc* p, pid_t pid) {
  if(p != NULL) {
    p->pid = pid;
  }
  return;
}
#endif