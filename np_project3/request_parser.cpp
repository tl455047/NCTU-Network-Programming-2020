
#include <string>
#include <cstring>
#include <iostream>
#include <cstdlib>
#include <regex>
#include "cgi_environ.hpp"
#include "request_parser.hpp"

//clase for HTTP request header parsing
request_parser::request_parser()
    : state_(REQUEST_METHOD)
  {

  }
  //reset state
  void request_parser::reset() 
  {
    state_ = REQUEST_METHOD;
  }
  //check is control characters
  int request_parser::isControlCharacter(char c) {
    if((int)c < 31 || (int)c == 127) {
      return 1;
    }
    else {
      return 0;
    }
  }
  //check if uri has control characters
  int request_parser::checkHasControlCharacter(char * token) {
    for(int i = 0; i < int(strlen(token)); i++) {
      if(isControlCharacter(token[i]) == 1) {
        return 1;
      }
    }
    return 0;
  }
  //parse http headers, if valid return 1, if invalid return 0
  int request_parser::parseHttpHeader(char* data, struct cgi_environ& cgi_envir)
  {
    //reset state
    reset();
    char* token;
    char* query_string;
    token = strtok(data, " \n");
    //parse request
    while(token != NULL) {    
      //remove '\r'
      if(token[strlen(token)-1] == '\r')
        token[strlen(token)-1] = '\0';
      printf("state: %d, %s, len: %d\n", state_, token, (int)strlen(token));
      
      switch(state_) {
        //request method state, get request method
        case REQUEST_METHOD:
          //check if the method is valid  
          if(strncmp(token, "GET", 3) != 0) {
            fprintf(stderr, "invalid method\n");
            return 0;
          }
          cgi_envir.REQUEST_METHOD = std::string(token);
          state_ = REQUEST_URI;
          break;
        //request uri & query string state, get request uri 
        case REQUEST_URI:
          //check if uri is valid
          if(checkHasControlCharacter(token)) {
            fprintf(stderr, "invalid uri\n");
            return 0;
          }
          query_string = token;
          state_ = SERVER_PROTOCOL;
          break;
        //request server protocol state, get request server protocol
        case SERVER_PROTOCOL:
          //check if version is HTTP/
          if(strncmp(token, "HTTP/", 5) == 0) {
            float version = atof(token+5);
            //HTTP 1.0, 1.1, 2.0
            if(version <= 0) {
              fprintf(stderr, "invalid version\n");
              return 0;
            }
          }
          else {
            fprintf(stderr, "invalid protocol\n");
            return 0;
          }
          cgi_envir.SERVER_PROTOCOL = "HTTP/1.1";
          state_ = PRE_HTTP_HOST;
          break;
        // pre http host state, for "HOST: " string
        case PRE_HTTP_HOST:
          if(strcmp(token, "Host:") == 0) {
            state_ = HTTP_HOST;
          }
          else {
            fprintf(stderr, "not HOST: \n"); 
            return 0;
          }
          break;
        //request http host state, get request http host
        case HTTP_HOST:  
          //check if the host name is valid  
          if(checkHasControlCharacter(token)) {
            fprintf(stderr, "invalid host name\n");
            return 0;
          }
          cgi_envir.HTTP_HOST = std::string(token);
          //parse query string
          cgi_envir.REQUEST_URI = std::string(query_string);
          query_string = strtok(query_string, "?"); 
          cgi_envir.CGI = std::string(query_string);
          query_string = strtok(NULL, "");
          if(query_string == nullptr) {
            cgi_envir.QUERY_STRING = std::string("");
          }
          else {
            cgi_envir.QUERY_STRING = std::string(query_string);
          }
          return 1;
          break;
        //default
        default:
          break;
      }

      token = strtok(NULL, " \n");
    }
    //request format incorrect
    fprintf(stderr, "format incorrect\n");
    return 0;
  }
  //parse query string
  void request_parser::parseQueryString(char* data, std::vector<struct shell_request>& shell_requests) {
    
    for(unsigned int i = 0; i < shell_requests.size(); i++) {
      shell_requests[i].valid = 1;
    }
    char* token;
    token = strtok(data, "&");
    //parse shell request
    int num = 0;
    while(token != NULL) {
      num = atoi(token+1);
      //check has param
      if(token[3] == '\0') 
        shell_requests[num].valid = 0;
      //set host, port, file
      switch(token[0]) {
        //host
        case 'h':
          shell_requests[num].host = std::string(token+3);
          break;
        //port
        case 'p':
          shell_requests[num].port = std::string(token+3);
          break;
        //file
        case 'f':
          shell_requests[num].file = std::string("./test_case/").append(std::string(token+3));
          break;
        default:
          break;
      }        
      token = strtok(NULL, "&");
    }
  }
