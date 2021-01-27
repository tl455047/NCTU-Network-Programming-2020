#ifndef _CGI_ENVIRON_WIN_H_
#define _CGI_ENVIRON_WIN_H_
#include <iostream>
#include <string>
//structure for cgi environ setting
struct cgi_environ {
  std::string REQUEST_METHOD;
  std::string REQUEST_URI;
  std::string QUERY_STRING;
  std::string SERVER_PROTOCOL;
  std::string HTTP_HOST;
  std::string SERVER_ADDR;
  unsigned short SERVER_PORT;
  std::string REMOTE_ADDR;
  unsigned short REMOTE_PORT;
  std::string CGI;
};
//structure for shell request
struct shell_request{
  std::string host;
  std::string port;
  std::string file;
  std::string session_id;
  int valid;
};
//print cgi environ structure
void printCgiEnviron(struct cgi_environ cgi_envir);
//print cgi env
void printEnv();
//set envir
void setCgiEnv(struct cgi_environ envir);

#endif