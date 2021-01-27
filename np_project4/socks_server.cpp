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
#include <fstream>
#include <boost/asio.hpp>
#include <regex>
#include <boost/algorithm/string/replace.hpp>
#include <boost/xpressive/xpressive.hpp>
#include "socks_server.hpp"

#define SOCKS4_REPLY_LEN 8
#define REQUEST_LENGTH 1024

using boost::asio::ip::tcp;

class connect_session
  : public std::enable_shared_from_this<connect_session>
{
  public:
    connect_session(tcp::socket socket, boost::asio::io_context& io_context, std::string domain, std::string port)
      : socket_c(std::move(socket))
      , socket_s(io_context)
      , resolver_(io_context)
    {
      domain_ = domain;
      port_ = port;
      end_flag[0] = 0;
      end_flag[1] = 0;
    }
    void start() {
      do_resolve(domain_, port_);
    }
  private:
    void do_resolve(std::string domain, std::string port) {
      //set query
      tcp::resolver::query query(domain, port);
      auto self(shared_from_this());     
      resolver_.async_resolve(
      query,
      [this, self](boost::system::error_code ec, tcp::resolver::iterator ep_iter)
        {
          if (!ec)
          { 
            //std::cout<<ep_iter->endpoint().address().to_string()<<std::endl;
            //std::cout<<ep_iter->endpoint().port()<<std::endl;
            tcp::resolver::iterator end;
            tcp::endpoint endpoint;
            for(; ep_iter != end; ep_iter++) {
              if(ep_iter->endpoint().address().is_v4()) {
                endpoint = ep_iter->endpoint();
                break;
              }
            }
            do_connect(endpoint);
          }
          
        }); 
    }
    //do connect
    void do_connect(tcp::endpoint endpoint_) {
      auto self(shared_from_this());
      socket_s.async_connect(endpoint_,
        [this, self](boost::system::error_code ec)
        {
          int cd = 0;
          if (!ec)
          {
            cd = 90;
          }
          else
          {
            cd = 91;
          }
          
          sendSock4Reply(cd);
          
        });        
    }
    //send sock4 reply
    void sendSock4Reply(int cd) {
      //print server message for connect operation
      printMessage(cd);
      //perpare reply
      unsigned char data[SOCKS4_REPLY_LEN];
      data[SOCKS4_REPLY_LEN] = '\0';
      data[0] = 0;
      data[1] = cd;
      for(int i = 2; i < SOCKS4_REPLY_LEN; i++) {
        data[i] = 0;
      }
      auto self(shared_from_this());
      boost::asio::async_write(socket_c, boost::asio::buffer(data, SOCKS4_REPLY_LEN),
        [this, self, cd](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            if(cd == 90) {
              do_echo_read();
              do_remote_read();
            }
          }
        });
      return;
    }
    //print server message
    void printMessage(int cd) {
      std::cout<<"<S_IP>: "<<socket_c.remote_endpoint().address().to_v4().to_string()<<std::endl;
      std::cout<<"<S_PORT>: "<<socket_c.remote_endpoint().port()<<std::endl;
      std::cout<<"<D_IP>: "<<socket_s.remote_endpoint().address().to_v4().to_string()<<std::endl;
      std::cout<<"<D_PORT>: "<<socket_s.remote_endpoint().port()<<std::endl;
      std::cout<<"<Command>: "<<"CONNECT"<<std::endl;
      if(cd == 90)
        std::cout<<"<Reply>: "<<"Accept"<<std::endl;
      else
        std::cout<<"<Reply>: "<<"Reject"<<std::endl;
      std::cout<<std::endl;
      return;
    }
    //do echo read
    //read data from client 
    void do_echo_read() {
      
      auto self(shared_from_this());
      socket_c.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            data_[length] = '\0';
            //printf("echo read length: %d\n", (int)length);
            do_remote_write(length);
          }
          else
          {
            //std::cout<<"error: "<<ec<<std::endl;
            end_flag[0] = 1;
            socket_c.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
            socket_s.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ec);
            if(end_flag[1] == 1) {
              socket_c.close();
              socket_s.close();
            }
          }
          
        });
    }
    //do remote write
    //write data read from client to remote server
    void do_remote_write(std::size_t length) {
      auto self(shared_from_this());
      boost::asio::async_write(socket_s, boost::asio::buffer(data_, length),
        [this, self, length](boost::system::error_code ec, std::size_t r_length)
        {
          if (!ec)
          {
            do_echo_read();
          }
          else
          {
            //std::cout<<"error: "<<ec<<std::endl;
          }
        });
    }
    //do remote read 
    //read data from remote server
    void do_remote_read() {
      auto self(shared_from_this());
      socket_s.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            data_[length] = '\0';
            //printf("remote read length: %d\n", (int)length);
            do_echo_write(length);
          }
          else
          {
            //std::cout<<"error: "<<ec<<std::endl;
            end_flag[1] = 1;
            socket_c.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ec);
            socket_s.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
            if(end_flag[0] == 1) {
              socket_c.close();
              socket_s.close();
            }
          }
        });
    }
    //do echo write
    //echo data read from remote server to client
    void do_echo_write(size_t length) {
      auto self(shared_from_this());
      boost::asio::async_write(socket_c, boost::asio::buffer(data_, length),
        [this, self, length](boost::system::error_code ec, std::size_t r_length)
        {
          if (!ec)
          {
            do_remote_read();
          }
          else
          {
            //std::cout<<"error: "<<ec<<std::endl;
          }
        });
    }
  tcp::socket socket_c;
  tcp::socket socket_s;
  tcp::resolver resolver_;
  enum { max_length = 15020 };
  unsigned char data_[max_length];
  std::string domain_;
  std::string port_;
  int end_flag[2];
};

class bind_session
  : public std::enable_shared_from_this<bind_session>
{
  public:
    bind_session(tcp::socket socket, boost::asio::io_context& io_context)
      : socket_c(std::move(socket))
      , socket_s(io_context)
      , acceptor_(io_context)
    {
      end_flag[0] = 0;
      end_flag[1] = 0;
    }
    void start() {
      do_bind();
    }
  private:
    //bind operation
    void do_bind() {
      //bind and listen on port
      auto self(shared_from_this());
      tcp::endpoint endpoint_ = tcp::endpoint(tcp::v4(), 0);
      acceptor_.open(endpoint_.protocol());
      acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
      acceptor_.bind(endpoint_);
      acceptor_.listen();
      //send socks reply
      sendSock4Reply(acceptor_.local_endpoint().port());
    }
    //send sock4 reply
    void sendSock4Reply(unsigned short port) {
      //prepare reply
      reply[SOCKS4_REPLY_LEN] = '\0';
      reply[0] = 0;
      reply[1] = 90;
      reply[2] = port / 256;
      reply[3] = port % 256;
      for(int i = 4; i < SOCKS4_REPLY_LEN; i++) {
        reply[i] = 0;
      }
      auto self(shared_from_this());
      boost::asio::async_write(socket_c, boost::asio::buffer(reply, SOCKS4_REPLY_LEN),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            do_accept();
          }
        });
      return;
    }
    //do accept
    void do_accept() {
      auto self(shared_from_this());
      acceptor_.async_accept(
        [self, this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            socket_s = std::move(socket);
            boost::asio::async_write(socket_c, boost::asio::buffer(reply, SOCKS4_REPLY_LEN),
              [this, self](boost::system::error_code ec, std::size_t /*length*/)
              {
                if (!ec)
                {
                  //print server message
                  printMessage(90);
                  do_echo_read();
                  do_remote_read();
                }
              });       
          }      
        });
    
    }
    //print server message
    void printMessage(int cd) {
      std::cout<<"<S_IP>: "<<socket_c.remote_endpoint().address().to_v4().to_string()<<std::endl;
      std::cout<<"<S_PORT>: "<<socket_c.remote_endpoint().port()<<std::endl;
      std::cout<<"<D_IP>: "<<socket_s.remote_endpoint().address().to_v4().to_string()<<std::endl;
      std::cout<<"<D_PORT>: "<<socket_s.remote_endpoint().port()<<std::endl;
      std::cout<<"<Command>: "<<"BIND"<<std::endl;
      if(cd == 90)
        std::cout<<"<Reply>: "<<"Accept"<<std::endl;
      else
        std::cout<<"<Reply>: "<<"Reject"<<std::endl;
      std::cout<<std::endl;
      return;
    }
    //do echo read
    //read data from client 
    void do_echo_read() {   
      auto self(shared_from_this());
      socket_c.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            data_[length] = '\0';
            //printf("echo read length: %d\n", (int)length);
            do_remote_write(length);
          }
          else
          {
            //std::cout<<"error: "<<ec<<std::endl;
            end_flag[0] = 1;
            socket_c.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
            socket_s.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ec);
            if(end_flag[1] == 1) {
              socket_c.close();
              socket_s.close();
            }
          }
          
        });
    }
    //do remote write
    //write data read from client to remote server
    void do_remote_write(std::size_t length) {
      auto self(shared_from_this());
      boost::asio::async_write(socket_s, boost::asio::buffer(data_, length),
        [this, self, length](boost::system::error_code ec, std::size_t r_length)
        {
          if (!ec)
          {
            do_echo_read();
          }
          else
          {
            //std::cout<<"error: "<<ec<<std::endl;
          }
        });
    }
    //do remote read 
    //read data from remote server
    void do_remote_read() {
      auto self(shared_from_this());
      socket_s.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            data_[length] = '\0';
            //printf("remote read length: %d\n", (int)length);
            do_echo_write(length);
          }
          else
          {
            //std::cout<<"error: "<<ec<<std::endl;
            end_flag[1] = 1;
            socket_c.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ec);
            socket_s.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
            if(end_flag[0] == 1) {
              socket_c.close();
              socket_s.close();
            }
          }
        });
    }
    //do echo write
    //echo data read from remote server to client
    void do_echo_write(size_t length) {
      auto self(shared_from_this());
      boost::asio::async_write(socket_c, boost::asio::buffer(data_, length),
        [this, self, length](boost::system::error_code ec, std::size_t r_length)
        {
          if (!ec)
          {
          
            do_remote_read();
          }
          else
          {
            //std::cout<<"error: "<<ec<<std::endl;
          }
        });
    }
  tcp::socket socket_c;
  tcp::socket socket_s;
  enum { max_length = 15020 };
  unsigned char data_[max_length];
  tcp::acceptor acceptor_;
  unsigned char reply[REQUEST_LENGTH];
  int end_flag[2];
};

class session
  : public std::enable_shared_from_this<session>
{
  public:
    session(tcp::socket socket, boost::asio::io_context& io_context)
      : socket_c(std::move(socket))
      , io_context_(io_context)
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
      socket_c.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            //set data
            data_[length] = '\0';
            //parsing http headers
            parse_sock4_request(data_, length);
            //printSock4Request();
            //check valid operation
            if(req.cd != 1 && req.cd != 2) {
              fprintf(stderr, "error operation\n");
              return;
            }
            //resolve destination ip
            tcp::resolver resolver(io_context_);
            std::string domain;
            //if is 4 version, resolve domain name
            if(req.is_4a == 1) 
              domain = req.domain_name;
            else
              domain = req.dip;
            tcp::resolver::query query(domain, req.dport);
            tcp::resolver::iterator iterator = resolver.resolve(query);
            tcp::resolver::iterator end;
            tcp::endpoint endpoint;
            //find ipv4
            for(; iterator != end; iterator++) {
              if(iterator->endpoint().address().is_v4()) {
                endpoint = iterator->endpoint();
                break;
              }
            }
            //check fierwall
            if(checkFirewall(endpoint.address().to_v4().to_string()) == 0) {
              //print server message
              printMessage(endpoint);
              //prepare reject reply
              data_[0] = 0;
              data_[1] = 91;
              //port
              unsigned short port = endpoint.port();
              data_[2] = port / 256;
              data_[3] = port % 256;
              //destination ip
              data_[4] = endpoint.address().to_v4().to_bytes()[0];
              data_[5] = endpoint.address().to_v4().to_bytes()[1];
              data_[6] = endpoint.address().to_v4().to_bytes()[2];
              data_[7] = endpoint.address().to_v4().to_bytes()[3];
              data_[8] = 0;
              boost::asio::async_write(socket_c, boost::asio::buffer(data_, SOCKS4_REPLY_LEN),
                [](boost::system::error_code ec, std::size_t r_length)
                {
                  if (!ec)
                  {
                    
                  }
                
                });
              return;
            }
            //connect operation
            if(req.cd == 1) {
              std::make_shared<connect_session>(std::move(socket_c), io_context_, domain, req.dport)->start();
            }
            //bind operation
            else if(req.cd == 2){
              std::make_shared<bind_session>(std::move(socket_c), io_context_)->start();
            }  
          }
        });
    }
    //print server message
    void printMessage(tcp::endpoint endpoint) {
      std::cout<<"<S_IP>: "<<socket_c.remote_endpoint().address().to_v4().to_string()<<std::endl;
      std::cout<<"<S_PORT>: "<<socket_c.remote_endpoint().port()<<std::endl;
      std::cout<<"<D_IP>: "<<endpoint.address().to_v4().to_string()<<std::endl;
      std::cout<<"<D_PORT>: "<<endpoint.port()<<std::endl;
      if(req.cd == 1)
        std::cout<<"<Command>: "<<"CONNECT"<<std::endl;
      else 
         std::cout<<"<Command>: "<<"BIND"<<std::endl;
      std::cout<<"<Reply>: "<<"Reject"<<std::endl;
      std::cout<<std::endl;
      return;
    }
    //parse sock4 request
    void parse_sock4_request(unsigned char* data, std::size_t length) {
      //sock4 version
      req.vn = data[0];
      //connect or bind
      req.cd = data[1];
      //destination port
      int port = (data[2])*256 + data[3];
      std::stringstream ss;
      ss.str("");
      ss<<port;
      req.dport = std::string(ss.str());
      //destination ip
      req.dip.clear();
      ss.str(""); 
      //to_string(bytetoint(_request[4]))+"."+to_string(bytetoint(_request[5]))+"."+to_string(bytetoint(_request[6]))+"."+to_string(bytetoint(_request[7]));
      ss<<(unsigned int)data[4]<<'.'<<(unsigned int)data[5]<<'.'<<(unsigned int)data[6]<<'.'<<(unsigned int)data[7];
      req.dip = std::string(ss.str());
      //user id
      req.user_id = std::string((char*)data + 8);
      req.is_4a = 0;
      //need domain name
      if(data[4] == 0 && data[5] == 0 && data[6] == 0 && (data[7] - 0 > 0)) {
        req.is_4a = 1;
        //get domain name
        req.domain_name = std::string((char*)data + 8 + req.user_id.size() + 1);
      }
      return;
    }
    //print sock4 request
    void printSock4Request() {
      std::cout<<"version number: "<<req.vn<<std::endl;
      std::cout<<"connection or bind: "<<req.cd<<std::endl;
      std::cout<<"destination port: "<<req.dport<<std::endl;
      std::cout<<"destination ip: "<<req.dip<<std::endl;
      std::cout<<"user id: "<<req.user_id<<std::endl;
      std::cout<<"domain name: "<<req.domain_name<<std::endl;
      std::cout<<"is 4a: "<<req.is_4a<<std::endl;
      return;
    }
    //check firewall
    int checkFirewall(std::string dip) {
      int cd;
      file.open("./socks.conf", std::ios::in);
      std::string grant, op, ip;
      int permit = 0;
      //check socks.conf contains rules
      if(file) {
        while(file >> grant >> op >> ip) {
          if(op == "c")
            cd = 1;
          else
            cd = 2;
          //if is valid rule for operation
          if((cd == req.cd)) {
            boost::algorithm::replace_all(ip, ".", "\\.");
            boost::algorithm::replace_all(ip, "*", ".*");
            boost::xpressive::sregex re = boost::xpressive::sregex::compile(ip, boost::xpressive::regex_constants::icase);
            boost::xpressive::smatch w;
            std::string dst_ip;
            if(boost::xpressive::regex_match(dip, w, re)) {
              if(grant == "permit") {
                //std::cout<<"permit: "<<grant<<" "<<op<<" "<<ip<<std::endl;
                permit = 1;
              }
              else
                permit = 0;
              return permit;
            }
          }
        } 
      }
      else {
        permit = 1;
      }
      file.close();
      return permit;
    }
   
  tcp::socket socket_c;
  
  enum { max_length = 15020 };
  unsigned char data_[max_length];
  //sock4 request
  struct sock4_request{
    int vn; //version number
    int cd; //connect or bind
    std::string dport; //destination port
    std::string dip; //destination ip
    std::string user_id; //user id
    std::string domain_name; //domain name
    int is_4a; //is version 4a
  };
  struct sock4_request req;
  std::fstream file;
  boost::asio::io_context& io_context_;
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
   
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    do_accept(io_context);
  }

private:
  void do_accept(boost::asio::io_context& io_context)
  {
    acceptor_.async_accept(
        [this, &io_context](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            io_context.notify_fork(boost::asio::io_context::fork_prepare);
            pid_t pid = fork();
            //child process
            if(pid == 0) {  

              io_context.notify_fork(boost::asio::io_context::fork_child);
              acceptor_.close();
              std::make_shared<session>(std::move(socket), io_context)->start();

            }
            //parent process
            else if(pid > 0) {

              io_context.notify_fork(boost::asio::io_context::fork_parent);
              do_accept(io_context);
     
            }
            //error
            else
            {
              fprintf(stderr, "fork error\n");
            }
            
            signal(SIGCHLD, childHandler);
          }      
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