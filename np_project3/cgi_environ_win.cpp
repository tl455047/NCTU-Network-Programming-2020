#include "cgi_environ_win.hpp"
#include <iostream>
#include <cstdio>
#include <sstream>
#include <string>
#include <cstdlib>
//print cgi environ structure
void printCgiEnviron(struct cgi_environ cgi_envir) {
  std::cout<<cgi_envir.REQUEST_METHOD<<std::endl;
  std::cout<<cgi_envir.REQUEST_URI<<std::endl;
  std::cout<<cgi_envir.QUERY_STRING<<std::endl;
  std::cout<<cgi_envir.SERVER_PROTOCOL<<std::endl;
  std::cout<<cgi_envir.HTTP_HOST<<std::endl;
  std::cout<<cgi_envir.SERVER_ADDR<<std::endl;
  std::cout<<cgi_envir.SERVER_PORT<<std::endl;
  std::cout<<cgi_envir.REMOTE_ADDR<<std::endl;
  std::cout<<cgi_envir.REMOTE_PORT<<std::endl;
}
//print cgi env
void printEnv() {
  char* env = getenv("REQUEST_METHOD");
  printf("REQUEST_METHOD: %s\n", env);
  env = getenv("REQUEST_URI");
  printf("REQUEST_URI: %s\n", env);
  env = getenv("QUERY_STRING");
  printf("QUERY_STRING: %s\n", env);  
  env = getenv("SERVER_PROTOCOL");
  printf("SERVER_PROTOCOL: %s\n", env);
  env = getenv("HTTP_HOST");
  printf("HTTP_HOST: %s\n", env);
  env = getenv("SERVER_ADDR");
  printf("SERVER_ADDR: %s\n", env);
  env = getenv("SERVER_PORT");
  printf("SERVER_PORT: %s\n", env);
  env = getenv("REMOTE_ADDR");
  printf("REMOTE_ADDR: %s\n", env);
  env = getenv("REMOTE_PORT");
  printf("REMOTE_PORT: %s\n", env);
}
//set envir
void setCgiEnv(struct cgi_environ envir) {
  _putenv_s("REQUEST_METHOD", envir.REQUEST_METHOD.c_str());
  _putenv_s("REQUEST_URI", envir.REQUEST_URI.c_str());
  _putenv_s("QUERY_STRING", envir.QUERY_STRING.c_str());
  _putenv_s("SERVER_PROTOCOL", envir.SERVER_PROTOCOL.c_str());
  _putenv_s("HTTP_HOST", envir.HTTP_HOST.c_str());
  _putenv_s("SERVER_ADDR", envir.SERVER_ADDR.c_str());
  std::stringstream ss;
  ss<<envir.SERVER_PORT;
  _putenv_s("SERVER_PORT", ss.str().c_str());
  ss.str("");
  _putenv_s("REMOTE_ADDR", envir.REMOTE_ADDR.c_str());
  ss<<envir.REMOTE_PORT;
  _putenv_s("REMOTE_PORT", ss.str().c_str());
}