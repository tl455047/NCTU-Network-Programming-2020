#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sstream>
#include <errno.h>
#include "npShell_single_proc.h"

void childHandler(int signo);

int npShell_one_commmand_line(struct client* client, std::vector<struct client> &clients, int (*user_pipe)[MAXIMUM_USER]) {
  
  const char sym[] = "% ";
  const char del[2] = " ";

  char commands[15012];
  char* token;
  //char command[257];
  //used for command output
  char print_commands[15012];
  int status;
  proc* proc_list_head = NULL;
  pipeN* pipe_list_head = NULL;
  pipeN* pipeN_list_head = client->pipeN_list_head;
  //set default env
  if(setenv("PATH", "bin:.", 1) != 0) {
    printf("set env error\n");
  }
  //set client env
  std::map<std::string, std::string>::iterator it;
  for(it = client->env_map.begin(); it != client->env_map.end(); ++it) {
    if(setenv(it->first.c_str(), it->second.c_str(), 1) != 0) {
      printf("set env error\n");
    }
  }
//while(1) {
  //printf("%2s", sym);
  //fflush(stdout);
  //get commands
  fgets(commands, 15010, stdin);
  //int num = readLine(client->fd, commands, sizeof(commands));
  //printf("%d\n", num);
  //read EOF, end
  /*if(num == 0) {
    //close all number pipe
    pipeN* piN = pipeN_list_head;
    while(piN != NULL) {
      close(piN->fd[0]);
      close(piN->fd[1]);

      piN = piN->next;
    }
    signal(SIGCHLD, childHandler);
    //release memory
    //pipeN_list_head = deletePipeList(pipeN_list_head);
    proc_list_head = deleteProcList(proc_list_head);
    pipe_list_head = deletePipeList(pipe_list_head);
    return 1;
  }*/
  //remove '\n'
  commands[strlen(commands)-1] = '\0';
  //deal with '\r'
  if(commands[strlen(commands)-1] == '\r') 
    commands[strlen(commands)-1] = '\0'; 
  //used for command output
  strncpy(print_commands, commands, strlen(commands)+1);
  //wait for number pipe line processes
  signal(SIGCHLD, childHandler);
  //parse commands
  token = strtok(commands, del);
  //if command exists
  if(token != NULL) {
    //build in function who
    if(strncmp(token, "who", 3) == 0) {
      //Show information of all users
      who(client->fd, clients);
      write(client->fd, sym, 2);
      return 0;
    }
    //build in function tell
    else if(strncmp(token, "tell", 4) == 0) {
      //get send user id
      token = strtok(NULL, del);
      //no id
      if(token == NULL) {
        write(client->fd, sym, 2);
        return 0;
      }
      int send_id = atoi(token);
      //check if user exist
      if(send_id <= MAXIMUM_USER && send_id >= 1 && clients[send_id-1].active == 1) {
        //get message
        token = strtok(NULL, "");
        //no message
        if(token == NULL) {
          write(client->fd, sym, 2);
          return 0;
        }
        std::string s;
        s.append("*** ");
        s.append(client->name);
        s.append(" told you ***: ");
        s.append(token);
        s.push_back('\n');
        //send message to certain user
        write(clients[send_id-1].fd, s.c_str(), strlen(s.c_str()));
      }
      else {
        //user does not exist
        printUserNotExistMessage(client->fd, send_id);
      }
      write(client->fd, sym, 2);
      return 0;
    }
    //build in function yell
    else if(strncmp(token, "yell", 4) == 0) {
      //get message
      token = strtok(NULL, "");
      //no message
      if(token == NULL) {
        write(client->fd, sym, 2);
        return 0;
      }
      //prepare message
      std::string s;
      s.append("*** ");
      s.append(client->name);
      s.append(" yelled ***: ");
      s.append(token);
      s.push_back('\n');
      //broadcast message
      broadcastMessage(s, clients);
      write(client->fd, sym, 2);
      return 0;
    }
    //build in function name
    else if(strncmp(token, "name", 4) == 0) {
      //get name
      token = strtok(NULL, "");
      //no name argument
      if(token == NULL) {
        write(client->fd, sym, 2);
        return 0;
      }
      //check if new name exists
      std::vector<struct client>::iterator it;
      for(it = clients.begin(); it != clients.end(); ++it) {
        //user active and is not itself
        if(it->active == 1 && it->fd != client->fd) {   
          if(it->name.compare(token) == 0) {
            //new name exist cannot used
            std::string s;
            s.append(" *** User '");
            s.append(it->name);
            s.append("' already exists. ***\n");
            write(client->fd, s.c_str(), strlen(s.c_str()));
            write(client->fd, sym, 2);
            return 0;
          }
        }  
      }
      //set new name
      client->name.assign(token);
      //prepare message
      std::stringstream ss;
      std::string s;
      s.append("*** User from ");
      s.append(client->ip);
      s.push_back(':');
      ss << client->port;
      s.append(ss.str());
      s.append(" is named '");
      s.append(token);
      s.append("'. ***\n");
      //broadcast message
      broadcastMessage(s, clients);
      write(client->fd, sym, 2);
      return 0;
    }
    //continue parsing
    proc_list_head = createProcList(token);
  }
  //set pre_token for parsing
  char* pre_token = token;
  int argc = 1; //calculate argc  
  //second token
  token = strtok(NULL, del);
  //parse commands
  while(token != NULL) {  
    //find a pipe
    if(token[0] == '|' || token[0] == '>' || token[0] == '!' || token[0] == '<') {
      //create pipe struct according to pipe type
      //calculate N for number pipe
      int n = (int)atoi(token+1);
      //if token+1 is space, n will be 0
      int type;
      switch(token[0]) {
        case '|':
          if(n == 0)
            type = '|';
          else
            type = 'N';
          break;
        case '>':
          if(n == 0)
            type = '>';
          else
            type = 'U';
          break;
        default:
          type = token[0];
          break;
      }
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
        if(setenv(variable_name, value, 1) == 0) {
          std::map<std::string, std::string>::iterator it;
          it = client->env_map.find(std::string(variable_name));
          if(it != client->env_map.end()) {
            it->second = std::string(value);
          }
          else {
            client->env_map.insert(std::pair<std::string, std::string>(std::string(variable_name), std::string(value)));
          }
        }
        else {
          fprintf(stderr, "setenv error\n");
        }
      }
      //release memory
      proc_list_head = deleteProcList(proc_list_head);
      pipe_list_head = deletePipeList(pipe_list_head);
      write(client->fd, sym, 2);
      return 0;
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
          write(client->fd, env, strlen(env));
          write(client->fd, "\n", 1);
          //printf("%s\n", env);
          //fflush(stdout);
        }
      }
      //release memory
      proc_list_head = deleteProcList(proc_list_head);
      pipe_list_head = deletePipeList(pipe_list_head);
      write(client->fd, sym, 2);
      return 0;
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
      //pipeN_list_head = deletePipeList(pipeN_list_head);
      proc_list_head = deleteProcList(proc_list_head);
      pipe_list_head = deletePipeList(pipe_list_head);
      return 1;
    }
  }
  //used for fork, pre_process, process 
  proc* p = proc_list_head;
  proc* pre_p = NULL;
  //used for fork, pre_pipe, pipe 
  pipeN* pi = pipe_list_head;
  pipeN* pre_pi = NULL;

  int has_pipeN = 0; // this line has number pipe, not wait for it
  //if write user pipe exit, store pipe write end
  int write_fd = 0;
  //if read user pipe exit, store pipe read end
  int read_fd = 0;
  //if read user pipe exists, move forward pipe list
  if(pi != NULL && pi->type == '<') {
    pre_pi = pi;
    pi = pi->next;
  }
   //open pipe, fork and execute
  while(p != NULL) {
    // if pre process exists, move pipe to pre pipe.
    if(pre_p != NULL) {
      pre_pi = pi;
      pi = pi->next;
    }
    //this pipe or next pipe is read user pipe
    if((pre_pi != NULL && pre_pi->type == '<') || (pi != NULL && pi->next != NULL && pi->next->type == '<')) {
      pipeN* read_pi = pre_pi;
      if(pi != NULL && pi->next != NULL && pi->next->type == '<') {
        read_pi = pi->next;
      }
      //check if sender exist
      if(read_pi->count <= MAXIMUM_USER && read_pi->count >= 1 && clients[read_pi->count-1].active == 1) {
        //check if pipe is open
        if(user_pipe[read_pi->count-1][client->id-1] > 0) {
          //direct read user pipe to stdin
          read_fd = user_pipe[read_pi->count-1][client->id-1];
          //update user pipe table
          user_pipe[read_pi->count-1][client->id-1] = 0;
          //prepare message
          std::stringstream ss;
          std::string s;
          s.append("*** ");
          s.append(client->name);
          s.append(" (#");
          ss << client->id;
          s.append(ss.str());
          s.append(") just received from ");
          s.append(clients[read_pi->count-1].name);
          s.append(" (#");
          ss.str("");
          ss << read_pi->count;
          s.append(ss.str());
          s.append(") by '");
          //repeat command
          s.append(print_commands);
          s.append("' ***\n");
          //broadcast message
          broadcastMessage(s, clients);
        }
        //pipe not open, error
        else {
          std::stringstream ss;
          std::string s;
          s.append("*** Error: the pipe #");
          ss << read_pi->count;
          s.append(ss.str());
          s.append("->#");
          ss.str("");
          ss << client->id;
          s.append(ss.str());
          s.append(" does not exist yet. ***\n");
          //print message
          write(client->fd, s.c_str(), strlen(s.c_str()));
        }  
      }
      //sender not exist, error
      else {
        //print user not exit message
        printUserNotExistMessage(client->fd, read_pi->count);
      }
    }
    // if next process exits, open pipe. 
    if(pi != NULL) {
      if(p->next != NULL && pi->type == '|') {
        int fd[2];
        if(pipe(fd) == 0) {
          openPipe(pi, fd);
        }
        else {
          fprintf(stderr, "failed to open pipe.\n");
        }
      } 
      // if no next process, might have number pipe.
      else if(pi->type == 'N' || pi->type == '!') {
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
      //this pipe or next pipe is write user pipe
      else if(pi->type == 'U') {
        //has user pipe not wait
        has_pipeN = 1;
        //check receiver exist
        if(pi->count <= MAXIMUM_USER && pi->count >= 1 && clients[pi->count-1].active == 1) {
          //check if pipe is open
          if(user_pipe[client->id-1][pi->count-1] == 0) {
            //open write pipe 
            int fd[2];
            if(pipe(fd) == 0) {
              //store read end
              user_pipe[client->id-1][pi->count-1] = fd[0];
              write_fd = fd[1];
              //prepare message
              std::stringstream ss;
              std::string s;
              s.append("*** ");
              s.append(client->name);
              s.append(" (#");
              ss << client->id;
              s.append(ss.str());
              s.append(") just piped '");
              //repeat command
              s.append(print_commands);
              s.append("' to ");
              s.append(clients[pi->count-1].name);
              s.append(" (#");
              ss.str("");
              ss << pi->count;
              s.append(ss.str());
              s.append(") ***\n");
              //broadcast message
              broadcastMessage(s, clients);
            }
            else {
              fprintf(stderr, "failed to open pipe.\n");
            }
          }
          //already open, error
          else {
            //set write fd to /dev/null
            write_fd = 0;
            //prepare message
            std::stringstream ss;
            std::string s;
            s.append("*** Error: the pipe #");
            ss << client->id;
            s.append(ss.str());
            s.append("->#");
            ss.str("");
            ss << pi->count;
            s.append(ss.str());
            s.append(" already exists. ***\n");
            //print message
            write(client->fd, s.c_str(), strlen(s.c_str()));
          }
        }
        //receiver has not exist yet
        else {
          //set write fd to /dev/null
          write_fd = 0;
          //print user not exit message
          printUserNotExistMessage(client->fd, pi->count);
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
      //set stdout, stderr to sockfd
      close(1);
      dup(client->fd);
      close(2);
      dup(client->fd);
      //set pipe
      //set pre pipe to stdin, close pre pipe. 
      if(pre_pi != NULL && pre_pi->type == '|') {
        close(0);
        dup(pre_pi->fd[0]);
        close(pre_pi->fd[0]);
        close(pre_pi->fd[1]);
      } 
      //check number pipe, if exists and count = 0, connect it
      else if(pre_pi == NULL) {
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
      //read user pipe
      if((pre_pi != NULL && pre_pi->type == '<') || (pi != NULL && pi->next != NULL && pi->next->type == '<')) {
        //direct read user pipe to stdin
        if(read_fd > 0) {
          close(0);
          dup(read_fd);
          close(read_fd);
        }
        // no user pipe open    
        else {
          //set stdin to /dev/null
          int dev_null_fd = open("/dev/null", O_RDONLY);
          if(dev_null_fd != -1) {
            close(0);
            dup(dev_null_fd);
            close(dev_null_fd);
          }
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
        //pipe 'U', set stdout, stderr to user pipe
        else if(pi->type == 'U') {
          //redirect to user pipe
          if(write_fd > 0) {
            close(1);
            dup(write_fd);
            close(write_fd);
            close(user_pipe[client->id-1][pi->count-1]);
          }
          //redirect to /dev/null
          else {
            int dev_null_fd = open("/dev/null", O_WRONLY);
            if(dev_null_fd != -1) {
              close(1);
              dup(dev_null_fd);
              close(dev_null_fd);
            }
          }
        }
        //close dup client fd
        close(client->fd);
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
      if(pre_pi != NULL && pre_pi->type == '|') {
        close(pre_pi->fd[1]);
        close(pre_pi->fd[0]);
      }
      else if(pre_pi == NULL){
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
      //pipe 'U', parent close user pipe write end
      if(write_fd > 0) {
        //user pipe open, close write end      
        close(write_fd);
        write_fd = 0;
      }
      //if read user pipe open, close read end
       if(read_fd > 0) {
        //user pipe open, close read end
        close(read_fd);
        read_fd = 0;
      }
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
//}
  client->pipeN_list_head = pipeN_list_head;
  //print symbol
  write(client->fd, sym, 2);
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
//Show information of all users
void who(int fd, std::vector<struct client> &clients) {
  std::vector<client>::iterator it;
  std::stringstream ss;
  std::string s;
  char tab[3] = "\t";
  s.append("<ID>");
  s.append(tab);
  s.append("<nickname>");
  s.append(tab);
  s.append("<IP:port>");
  s.append(tab);
  s.append("<indicate me>\n");
  write(fd, s.c_str(), strlen(s.c_str()));
  for(it = clients.begin(); it != clients.end(); ++it) {
    ss.str("");
    s.clear();
    if(it->active == 1) {
      ss << it->id;
      s = ss.str();
      s.append(tab);
      s.append(it->name);
      s.append(tab);
      s.append(it->ip);
      s.push_back(':');
      s.append(it->port);
      if(it->fd == fd) {
        s.append(tab);
        s.append("<-me");
      }
      s.push_back('\n');
      write(fd, s.c_str(), strlen(s.c_str()));
    }
  }
}
//broadcast message
void broadcastMessage(std::string message, std::vector<client> &clients) {
  std::vector<struct client>::iterator it;
  for(it = clients.begin(); it != clients.end(); ++it) {
    if(it->active == 1) {   
      //send message to all active user
    write(it->fd, message.c_str(), strlen(message.c_str()));
    }  
  }
  return;
}
//print user not exit message
void printUserNotExistMessage(int fd, int id) {
  std::stringstream ss;
  std::string s;
  ss << id;
  s.append("*** Error: user #");
  s.append(ss.str());
  s.append(" does not exist yet. ***\n");
  write(fd, s.c_str(), strlen(s.c_str()));
}
//readline
ssize_t readLine(int fd, char* buffer, size_t n)
{
  //read every time
  ssize_t num_Read; 
  //total read byte                  
  size_t total_Read; 
  //char read every time
  char ch;
  //invalid input
  if (n <= 0 || buffer == NULL) {
    errno = EINVAL;
    return -1;
  }
  //initialize total read number                       
  total_Read = 0;
  //read line
  while(1) {
    //read one byte
    num_Read = read(fd, &ch, 1);
    //if interrupted by signal
    if(num_Read == -1) {
      if (errno == EINTR)         
        continue;
      else
        return -1;          
    } 
    //read EOF
    else if(num_Read == 0) {      
      if (total_Read == 0)           
        return 0;
      else                        
        break;
    }
    //read one byte store to commands 
    else {                        
      if(total_Read < n - 1) {     
        total_Read++;
        *buffer++ = ch;
      }
    //read \n, stop reading
    if(ch == '\n')
      break;
    }
  }
  *buffer = '\0';
  return total_Read;
}