#ifndef _HTTP_SERVER_WIN_H_
#define _HTTP_SERVER_WIN_H_
//#include <sys/types.h>
//#include <sys/wait.h>

#define MAX_SERVER 5
// console.cgi function
int console_main();
//output html
std::string outputHtml(std::vector<shell_request> &shell_requests);

#endif