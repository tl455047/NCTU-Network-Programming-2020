#ifndef _NP_SINGLE_PROC_SERVER_H_
#define _NP_SINGLE_PROC_SERVER_H_
#include <stdlib.h>
#include <iostream>
#include <map>
#include <string>
#include "npShell_single_proc.h"

//find smallest unused id
int findSmallestUnusedId(std::vector<struct client> &clients);
//create new client
void createClient(client &client, int id, int sockfd, struct sockaddr_in c_addr);
//print client message
void printClientMessage(client client);
//insert pair<fd, id> to fdmap 
void insertFdMap(std::map<int, int> &fd_map, int fd, int id);
//find client id giving fd
int findIdByFd(std::map<int, int> &fd_map, int fd);
//print fd map
void printMap(std::map<int, int> &fd_map);
//broadcast login message
void broadcastLoginMessage(client client, int nfds, fd_set *afds, int msock);
//broadcast logout message
void broadcastLogoutMessage(client client, int nfds, fd_set *afds, int msock);
#endif
