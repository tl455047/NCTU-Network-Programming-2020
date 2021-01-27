#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>      
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <vector>
#include <map>
#include <sstream>
#include "np_single_proc_server.h" 

int main(int argc, char *argv[]) {
  //port number
  int port_num = 7001;
  //socket address struct for client and server
  struct sockaddr_in c_addr, s_addr;
  //fd for master socket and slave socket
  int msock, ssock;
  //listen queue size
  int qlen = MAXIMUM_USER;
  //len for accept
  socklen_t c_addr_len;
  //fd table size
  int nfds = getdtablesize(); 
  //fd set for read, active fds, in select
  fd_set rfds, afds;
  //symbol 
  const char sym[] = "% ";
  //set argv[1] as port number
  if(argc == 2) {
    port_num = atoi(argv[1]);
  }
  printf("port: %d\n", port_num);
  //STL for 30 clients
  std::vector<client> clients;
  //resize
  clients.resize(MAXIMUM_USER);
  //initialize active state
  std::vector<client>::iterator it;
  for(it = clients.begin(); it != clients.end(); ++it) {
    it->active = 0;
  }
  //map for fd and id
  std::map<int, int>fd_map;
  fd_map.clear();
  //user pipe 2 dimension array
  int user_pipe[MAXIMUM_USER][MAXIMUM_USER];
  //initialize
  memset(user_pipe, 0, sizeof(int)*MAXIMUM_USER*MAXIMUM_USER);
  //welcome message
  char welcome[130] = "****************************************\n** Welcome to the information server. **\n****************************************\n";
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
  //set active fds
  FD_ZERO(&afds);
  FD_SET(msock, &afds);
  //copy stdin
  int stdin_copy = dup(0);
  //loop
  while(1) {
    //set afds to rfds
    memcpy(&rfds, &afds, sizeof(rfds));
    //monitor read fd set
    if(select(nfds, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0) {
      fprintf(stderr, "cannot select\n");
      continue;
    }
    //a new connection request
    if(FD_ISSET(msock, &rfds)) {
      //get client address
      memset(&c_addr, 0, sizeof(c_addr));
      c_addr_len = sizeof(c_addr); 
      //accept connection request
      ssock = accept(msock, (struct sockaddr *)&c_addr, &c_addr_len); 
      if(ssock < 0) 
        fprintf(stderr, "cannot create slave socket\n");
      //set this connect to active fds
      FD_SET(ssock, &afds);
      //login
      //find unused id, and set active
      int id = findSmallestUnusedId(clients);
      //create new client
      createClient(clients[id], id, ssock, c_addr);
      //insert fd_map
      insertFdMap(fd_map, ssock, id);
      //debug message
      printClientMessage(clients[id]);
      printMap(fd_map);
      //print welcome message
      write(ssock, welcome, strlen(welcome));
      //broadcastLoginMessage
      broadcastLoginMessage(clients[id], nfds, &afds, msock);
      //print first symbol
      write(ssock, sym, 2);
    } 
    //deal with client request
    int fd = 0;
    for(fd = 0; fd < nfds; ++fd) {
			if(fd != msock && FD_ISSET(fd, &rfds)) {
        //redirect stdin, stdout, stderr to fd
        /*int stdin_copy = dup(0);*/
        close(0);
        dup(fd);
        int id = findIdByFd(fd_map, fd);
        if(id >= 0) {
          //execute shell
          int client_exit = npShell_one_commmand_line(&clients[id], clients, user_pipe);
          //clean env
          std::map<std::string, std::string>::iterator it;
          for(it = clients[id].env_map.begin(); it != clients[id].env_map.end(); ++it) {
            if(unsetenv(it->first.c_str()) != 0) {
              printf("set env error\n");
            }
          }
          //set default env
          if(setenv("PATH", "bin:.", 1) != 0) {
            printf("set env error\n");
          }
          //client exit, clean the data
          if(client_exit == 1) {
            //set active table
            clients[id].active = 0;
            //delete fd map
            fd_map.erase(fd);
            //delete number pipe list
            clients[id].pipeN_list_head = deletePipeList(clients[id].pipeN_list_head);
            //clean environment map
            clients[id].env_map.clear();
            //close connection
            close(fd);
            //remove fd fromm afds
            FD_CLR(fd, &afds);
            //broadcast logout message
            broadcastLogoutMessage(clients[id], nfds, &afds, msock);
            //unset and close user pipe
            for(int i = 0; i < MAXIMUM_USER; i++) {
              if(user_pipe[id][i] != 0) {
                close(user_pipe[id][i]);
                user_pipe[id][i] = 0;

              }
            }
            for(int i = 0; i < MAXIMUM_USER; i++) {
              if(user_pipe[i][id] != 0) {
                close(user_pipe[i][id]);
                user_pipe[i][id] = 0;

              }
            }
          }
        }
        //if client exit, close fd      
        //restore stdin, stdout, stderr
        close(0);
        dup(stdin_copy);  
      }
    }
  }
  return 0;
}
//find smallest unused id
int findSmallestUnusedId(std::vector<struct client> &clients) {
  for(int i = 0; i < clients.size(); ++i) {
    if(clients[i].active == 0) {
      clients[i].active = 1;
      return i;
    }
  }
  return -1;
}
//create new client
void createClient(client &client, int id, int sockfd, struct sockaddr_in c_addr) {
    client.id = id+1;
    client.fd = sockfd;
    client.env_map.clear();
    //client.indicate_me = "<- me";
    client.name = "(no name)";
    std::stringstream ss;
    ss << ntohs(c_addr.sin_port);
    client.port = ss.str(); 
    client.ip = std::string(inet_ntoa(c_addr.sin_addr));
    client.pipeN_list_head = NULL;
}
//print client message
void printClientMessage(client client) {

  printf("id: %d\n", client.id);
  printf("sockfd: %d\n", client.fd);
  printf("name: %s\n", client.name.c_str());
  //printf("indicate_me: %s\n", client.indicate_me.c_str());
  printf("port: %s\n", client.port.c_str());
  printf("ip: %s\n", client.ip.c_str());

  return;
}
//insert pair<fd, id> to fdmap 
void insertFdMap(std::map<int, int> &fd_map, int fd, int id) {
  fd_map.insert(std::pair<int, int>(fd, id));
  return;
}
//find client id giving fd
int findIdByFd(std::map<int, int> &fd_map, int fd) {
  std::map<int, int>::iterator it;
  it = fd_map.find(fd);
  if(it != fd_map.end()) {
    return it->second;
  }
  else
  {
    return -1;
  }
}
//print map
void printMap(std::map<int, int> &fd_map) {
  std::map<int, int>::iterator it;
  for(it = fd_map.begin(); it != fd_map.end(); ++it) {
    printf("%d => %d\n", it->first, it->second);
  }
  return;
}
//broadcast login message
void broadcastLoginMessage(client client, int nfds, fd_set *afds, int msock) {
  std::string m = "*** User '";
  m.append(client.name);
  m.append("' entered from ");
  m.append(client.ip);
  m.push_back(':');
  m.append(client.port);
  m.append(". ***\n");
  //broadcast
  int fd = 0;
  for(fd = 0; fd < nfds; ++fd) {
		if(fd != msock && FD_ISSET(fd, afds)) {
      write(fd, m.c_str(), strlen(m.c_str()));
    }
  } 
  return;
}
//broadcast logout message
void broadcastLogoutMessage(client client, int nfds, fd_set *afds, int msock) {
  std::string m = "*** User '";
  m.append(client.name);
  m.append("' left. ***\n");
  //broadcast
  int fd = 0;
  for(fd = 0; fd < nfds; ++fd) {
		if(fd != msock && FD_ISSET(fd, afds)) {
      write(fd, m.c_str(), strlen(m.c_str()));
    }
  } 
  return;
}