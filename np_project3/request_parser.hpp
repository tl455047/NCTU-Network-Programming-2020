#ifndef _REQUEST_PARSER_H_
#define _REQUEST_PARSER_H_
#include <string>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <regex>
#include <vector>
#include "cgi_environ.hpp"
//clase for HTTP request header parsing
class request_parser {
  public:
  //constructor
  request_parser();
  //reset state
  void reset();
  //check is control characters
  int isControlCharacter(char c);
  //check if uri has control characters
  int checkHasControlCharacter(char * token);
  //parse http headers, if valid return 1, if invalid return 0
  int parseHttpHeader(char* data, struct cgi_environ& cgi_envir);
  //parse query string
  void parseQueryString(char* data, std::vector<struct shell_request>& shell_requests);
  private:
  //states for http header parser
  enum state 
  {
    REQUEST_METHOD,
    REQUEST_URI,
    SERVER_PROTOCOL,
    PRE_HTTP_HOST,
    HTTP_HOST
  } state_;

  
};
#endif