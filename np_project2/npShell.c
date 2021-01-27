#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "npShell.h"

void childHandler(int signo);

int npShell() {
  
  const char sym[] = "% ";
  const char del[2] = " ";

  char commands[15012];
  char* token;
  //char command[257];
  int status;
  proc* proc_list_head = NULL;
  pipeN* pipe_list_head = NULL;
  pipeN* pipeN_list_head = NULL;
  //set env
  if(setenv("PATH", "bin:.",1) != 0) {
    printf("set env error\n");
  } 
  
  while(1) {
    //print symbol
    printf("%2s", sym);
    fflush(stdout);
    //get commands
    fgets(commands, 15010, stdin);
    commands[strlen(commands)-1] = '\0';
    //deal with '\r'
    if(commands[strlen(commands)-1] == '\r') 
      commands[strlen(commands)-1] = '\0'; 
    //wait for number pipe line processes
    signal(SIGCHLD, childHandler);
    //parse commands
    token = strtok(commands, del);
    //if command exists
    if(token != NULL)
      proc_list_head = createProcList(token);
    //set pre_token for parsing
    char* pre_token = token;
    int argc = 1; //calculate argc  
    //second token
    token = strtok(NULL, del);
    //parse commands
    while(token != NULL) {  
      //find a pipe
      if(token[0] == '|' || token[0] == '>' || token[0] == '!') {
        //create pipe struct according to pipe type
        //calculate N for number pipe
        int n = (int)atoi(token+1);
        //if token+1 is space, n will be 0
        int type;
        if(n == 0)
          type = token[0];
        else if(token[0] == '!')
          type = '!';
        else 
          type = 'N';
        //add pipe to list
        if(pipe_list_head == NULL) 
          pipe_list_head = createPipeList(type, n);
        else
          addPipe(pipe_list_head, type, n);
        //set argc to process
        setProcArgc(proc_list_head, argc);

      }
      //normal pipe, there is a process after it
      else if(pre_token[0] == '|' || pre_token[0] == '>') {
        //store command in linked list
        addProc(proc_list_head, token);
        argc = 1;
      }
      else {
        //concat arg in same command
        pre_token[strlen(pre_token)] = ' ';
        argc++;
     }
  
      pre_token = token;
    
      token = strtok(NULL, del); 
  
    }
    //if no command passes, if command exits, check printenv, setenv, exit
    if(proc_list_head != NULL) {
      //set last argc
      setProcArgc(proc_list_head, argc);
      // substract number pipe count
      pipeN* piN = pipeN_list_head;
        while(piN != NULL) {
          piN->count = piN->count - 1;
          piN = piN->next;
      }
      //setenv command
      if(strncmp(proc_list_head->command, "setenv", 6) == 0) {
        //ignore command
        token = strtok(proc_list_head->command, del);
        //first argument
        token = strtok(NULL, del);
        char* variable_name = token;
        //second argument
        token = strtok(NULL, del);
        char* value = token;
        //setenv
        if(variable_name != NULL && value != NULL) {
          if(setenv(variable_name, value, 1) != 0) {
            fprintf(stderr, "setenv error\n");
          }
        }
        //release memory
        proc_list_head = deleteProcList(proc_list_head);
        pipe_list_head = deletePipeList(pipe_list_head);
        continue;
      }
      //printenv command
      else if(strncmp(proc_list_head->command, "printenv", 8) == 0) {
        //ignore command
        token = strtok(proc_list_head->command, del);
        //first argument
        token = strtok(NULL, del);
        char* variable_name = token;
        //print env
        if(variable_name != NULL) {
          //getenv
          char* env = getenv(variable_name);
          if(env != NULL) {
            printf("%s\n", env);
            //fflush(stdout);
          }
        }
        //release memory
        proc_list_head = deleteProcList(proc_list_head);
        pipe_list_head = deletePipeList(pipe_list_head);
        continue;
      }
      //exit command
      else if(strncmp(proc_list_head->command, "exit", 4) == 0) {
        //close all number pipe
        pipeN* piN = pipeN_list_head;
        while(piN != NULL) {
          close(piN->fd[0]);
          close(piN->fd[1]);

          piN = piN->next;
        }
        signal(SIGCHLD, childHandler);
        //release memory
        pipeN_list_head = deletePipeList(pipeN_list_head);
        proc_list_head = deleteProcList(proc_list_head);
        pipe_list_head = deletePipeList(pipe_list_head);

        return 0;
      }
    }
    //used for fork, pre_process, process 
    proc* p = proc_list_head;
    proc* pre_p = NULL;
    //used for fork, pre_pipe, pipe 
    pipeN* pi = pipe_list_head;
    pipeN* pre_pi = NULL;
  
    int has_pipeN = 0; // this line has number pipe, not wait for it
    //open pipe, fork and execute
    while(p != NULL) {
      // if pre process exists, move pipe to pre pipe.
      if(pre_p != NULL) {
        pre_pi = pi;
        pi = pi->next;
      }
      // if next process exits, open pipe. 
      if(p->next != NULL && pi != NULL && pi->type == '|') {
        int fd[2];
        if(pipe(fd) == 0) {
          openPipe(pi, fd);
        }
        else {
          fprintf(stderr, "failed to open pipe.\n");
        }
      } 
      // if no next process, might have number pipe.
      else if(pi != NULL && (pi->type == 'N' || pi->type == '!')) {
        pipeN* piN = pipeN_list_head;
        //has number pipe
        has_pipeN = 1;
        int flag = 0;
        //find is there a number pipe with same next n line
        while(piN != NULL) {
          if(pi->count == piN->count) {
            pi->fd[0] = piN->fd[0];
            pi->fd[1] = piN->fd[1];
            flag = 1;
            break;
          }
          piN = piN->next;
        }
        //not found, open a new number pipe
        if(flag == 0) { 
          int fd[2];
          if(pipe(fd) == 0) {
            openPipe(pi, fd);
            if(pipeN_list_head == NULL) 
              pipeN_list_head = createPipeNList(pi);
            else
              addPipeN(pipeN_list_head, pi);   
          }
          else {
            fprintf(stderr, "failed to open pipe.\n");
          }
        }
      }
      //check process limit, if close to limit, need to wait to release
      pid_t pid = fork();
      while( pid < 0 ) {
        waitpid(-1, &status, 0);
        pid = fork();   
      } 
      //set process pid
      setProcPid(p, pid);
      if(pid == 0) {
        //child process
        //set pipe
        //set pre pipe to stdin, close pre pipe. 
        if(pre_pi != NULL) {
          close(0);
          dup(pre_pi->fd[0]);
          close(pre_pi->fd[0]);
          close(pre_pi->fd[1]);
        } 
        //check number pipe, if exists and count = 0, connect it
        else {
          pipeN* piN = pipeN_list_head;
          //find number pipe count = 0
          while(piN != NULL) {
            if(piN->count == 0) {
              close(0);
              dup(piN->fd[0]);
              close(piN->fd[0]);
              close(piN->fd[1]);
              break;
            }
            piN = piN->next;
          }
        }
        //set pipe to stdout, close pipe.
        if(pi != NULL) {
          //normal pipe
          if(pi->type == '|' || pi->type == 'N') {
            close(1);
            dup(pi->fd[1]);
            close(pi->fd[0]);
            close(pi->fd[1]);
          } 
          // pipe '>' set to output file
          else if(pi->type == '>') {
            int output_fd = -1;
            //open output file 
            if(p->next != NULL) {
              output_fd = open(p->next->command, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
            }
            //set stdout to output file
            if(output_fd != -1) {
              close(1);
              dup(output_fd);
              close(output_fd);
            }
            else {
              fprintf(stderr, "open file error.\n");
            }
          }
          // '!' connect stdout and stderr
          else if(pi->type == '!') {
            close(1);
            dup(pi->fd[1]);
            close(2);
            dup(pi->fd[1]);
            close(pi->fd[0]);
            close(pi->fd[1]);
          }
        }
        //set argv
        char** command_argv = (char**)malloc(sizeof(char*)*(p->argc+1));
        token = strtok(p->command, del);
        int i = 0;
        command_argv[i] = token;

        while(token != NULL) {
          token = strtok(NULL, del);
          command_argv[++i] = token;
        }
        //execvp
        int e = execvp(command_argv[0], command_argv);
        if(e == -1) {
          fprintf(stderr, "Unknown command: [%s].\n", command_argv[0]);
          //fflush(stderr);
          free(command_argv);
          exit(-1);
        }
        else {
          exit(0);
        }
      }
      else if(pid > 0) {
        //parent process
        //close pre pipe
        if(pre_pi != NULL) {
          close(pre_pi->fd[1]);
          close(pre_pi->fd[0]);
        }
        else {
          //close number pipe
          pipeN* piN = pipeN_list_head;
          while(piN != NULL) {
            if(piN->count == 0) {
              close(piN->fd[0]);
              close(piN->fd[1]);
              break;
            }
            piN = piN->next;
          }
          //delete number pipe which count = 0
          pipeN_list_head = deletePipeN(pipeN_list_head);
        }
        // if is '>' pipe, no next process
        if(pi != NULL && pi->type == '>') {
          break;
        }
      }
      else {
        //error
        fprintf(stderr, "failed to fork process.\n");
      }
      pre_p = p;  
      p = p->next;  
    }
    //wait for output preocess if no number pipe
    p = proc_list_head;
    if(has_pipeN == 0) { 
      while(p != NULL) {
        waitpid(p->pid, &status, 0);
        p = p->next;
      }
    }
    //wait for number pipe line process
    signal(SIGCHLD, childHandler);
    //release memory
    proc_list_head = deleteProcList(proc_list_head);
    pipe_list_head = deletePipeList(pipe_list_head);
  }
  return 0;
}

void childHandler(int signo) {
  int status;
  while(waitpid(-1, &status, WNOHANG) > 0) {
    
  }
}

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
