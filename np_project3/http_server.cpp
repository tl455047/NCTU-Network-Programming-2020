#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <cstring>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <boost/asio.hpp>
#include "request_parser.hpp"
#include "http_server.hpp"

using boost::asio::ip::tcp;

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_(std::move(socket))
  {
    
  }

  void start()
  {
    do_read();
  }

private:
  void do_read()
  {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            //parsing http headers
            if(parser.parseHttpHeader(data_, envir)) {
              //write status code
              std::string status_code = "HTTP/1.1 200 OK\r\n";
              do_write(status_code);
              //set server address and port
              envir.SERVER_ADDR = std::string(socket_.local_endpoint().address().to_string());
              envir.SERVER_PORT = socket_.local_endpoint().port();
              //set remote address and port
              envir.REMOTE_ADDR = std::string(socket_.remote_endpoint().address().to_string());
              envir.REMOTE_PORT = socket_.remote_endpoint().port();
              printCgiEnviron(envir);
              //fork
              //check process limit, if close to limit, need to wait to release
              int status;
              pid_t pid = fork();
              while( pid < 0 ) {
                waitpid(-1, &status, 0);
                pid = fork();   
              } 
              //child process, for execute cgi
              if(pid == 0) {  
                //dup stdin, stdout, stderr
                close(0);
                dup(socket_.native_handle());
                close(1);
                dup(socket_.native_handle());
                close(2);
                dup(socket_.native_handle());
                //set cgi environment variables
                setCgiEnv(envir);
                printEnv();
                //set exec arguments
                char* command_argv[2];
                command_argv[0] = (char* )malloc(envir.CGI.size()+2);
                std::string s = ".";
                s.append(envir.CGI);
                strncpy(command_argv[0], s.c_str(), s.size());
                command_argv[0][s.size()] = '\0';
                command_argv[1] = nullptr;
                //execvp
                int e = execvp(command_argv[0], command_argv);
                if(e == -1) {
                  //fprintf(stderr, "Unknown command: [%s].\n", command_argv[0]);
                  free(command_argv[0]);
                  exit(-1);
                }
                else {
                  exit(0);
                }
              }
              //parent process
              else if(pid > 0) {
                //close socket fd
                close(socket_.native_handle());
              }
            }
            //bad request
            else {
              //write status code
              std::string status_code = "HTTP/1.1 403 Forbidden\r\n";
              do_write(status_code);
              fprintf(stderr, "bad request\n");
            }
            //wait for child
            signal(SIGCHLD, childHandler);
            //echo 
            //do_write(length);
          }
        });
  }

  void do_write(std::string data)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(data.c_str(), data.size()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            //do_read();
          }
        });
  }

  tcp::socket socket_;
  enum { max_length = 1024 };
  char data_[max_length];
  request_parser parser;
  struct cgi_environ envir;
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    do_accept();
  }

private:
  void do_accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<session>(std::move(socket))->start();
          }

          do_accept();
        });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: async_tcp_echo_server <port>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    server s(io_context, std::atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}