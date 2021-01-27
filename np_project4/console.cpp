#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <regex>
#include <fstream>
#include <sstream>
#include "request_parser.hpp"
#include "cgi_environ.hpp"
#include "console.hpp"

#define MAX_SERVER 5
#define SOCKS4_REQUEST_LEN 8
#define SOCKS4_REPLY_LEN 8

using boost::asio::ip::tcp;

class npshell_session
  : public std::enable_shared_from_this<npshell_session>
{
public:
  npshell_session(boost::asio::io_context& io_context, tcp::endpoint& end_point, tcp::socket socket, std::string test_case, std::string session_id)
    : socket_(std::move(socket))
    , endpoint_(std::move(end_point))
  {
    //get test_case file name
    test_case_ = test_case;
    //get session id
    session_id_ = session_id;
  }

  void start()
  {
    //open testcase file
    file.open(test_case_, std::ios::in);
    //do read
    do_read();
  }

private:
  void do_connect() {
    auto self(shared_from_this());
    socket_.async_connect(endpoint_,
        [this, self](boost::system::error_code ec)
        {
          if (!ec)
          {
            do_read();
          }
        });
          
  }
  
  void do_read()
  {
    auto self(shared_from_this());
    socket_.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          { 
            //set data_ 
            data_[length] = '\0';
            std::string data = std::string(data_);
            // output shell result to client
            data = std::regex_replace(data, std::regex("&"), "&amp;");
            data = std::regex_replace(data, std::regex("<"), "&lt;");
            data = std::regex_replace(data, std::regex(">"), "&gt;");
            data = std::regex_replace(data, std::regex("\""), "&quot;");
            data = std::regex_replace(data, std::regex("\'"), "&#39;");
            data = std::regex_replace(data, std::regex("\n"), "&NewLine;");
            //script
            std::string script = "<script>document.getElementById('";
            script.append(session_id_);
            script.append("').innerHTML += '");
            script.append(data);
            script.append("';</script>");
            
            std::cout<<script<<std::endl;
            //if read % , send command, else continue read
            if(!std::regex_search(data_, std::regex("% ")))
            {  
              do_read(); 
            } 
            else {
              do_write(); 
            }
          }
          else {
            //std::cout<<"read error"<<ec.message()<<std::endl;
          }
        });
  }

  void do_write()
  {
    //read test_case file
    if(!std::getline(file, command)) 
      return;
    
    //remove '\r'
    if(command[command.size()-1] == '\r')
      command.pop_back(); 
    //add '\n'
    command.push_back('\n');
    //set command to data_
    strncpy(data_, command.c_str(), command.size());
    data_[command.size()] = '\0';
    std::size_t length = command.size();

    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(data_, length),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            //output command to client
            std::string data = command;
            data = std::regex_replace(data, std::regex("&"), "&amp;");
            data = std::regex_replace(data, std::regex("<"), "&lt;");
            data = std::regex_replace(data, std::regex(">"), "&gt;");
            data = std::regex_replace(data, std::regex("\""), "&quot;");
            data = std::regex_replace(data, std::regex("\'"), "&#39;");
            data = std::regex_replace(data, std::regex("\n"), "&NewLine;");
            //script
            std::string script = "<script>document.getElementById('";
            script.append(session_id_);
            script.append("').innerHTML += '<b>");
            script.append(data);
            script.append("</b>';</script>");
            
            std::cout<<script<<std::endl;
            do_read();
          }
          else {
            //std::cout<<"write error"<<ec.message()<<std::endl;
          }
        });
  }
  
  tcp::socket socket_;
  tcp::endpoint endpoint_;
  enum { max_length = 15010 };
  char data_[max_length];
  std::string test_case_;
  std::fstream file;
  std::string command;
  std::string session_id_;
};

class connect_session
  : public std::enable_shared_from_this<connect_session>
{
  public:
    connect_session(boost::asio::io_context& io_context, std::string socks_domain, std::string socks_port, tcp::endpoint &endpoint, std::string test_case, std::string session_id)
      : resolver_(io_context)
      , socket_(io_context)
      , io_context_(io_context)
      , endpoint_(std::move(endpoint))
      
    {
      domain_ = socks_domain;
      port_ = socks_port;
      test_case_ = test_case;
      session_id_ = session_id;
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
            tcp::endpoint endpoint = std::move(ep_iter->endpoint());
            do_connect(endpoint);
          }
          
        }); 
    }
    //send socks request
    void sendSocksRequest() {
      //set request
      //version
      data_[0] = 4;
      //connect operation
      data_[1] = 1;
      //port
      unsigned short port = endpoint_.port();
      data_[2] = port / 256;
      data_[3] = port % 256;
      //destination ip
      data_[4] = endpoint_.address().to_v4().to_bytes()[0];
      data_[5] = endpoint_.address().to_v4().to_bytes()[1];
      data_[6] = endpoint_.address().to_v4().to_bytes()[2];
      data_[7] = endpoint_.address().to_v4().to_bytes()[3];
      data_[8] = 0;
      auto self(shared_from_this());
      boost::asio::async_write(socket_, boost::asio::buffer(data_, SOCKS4_REQUEST_LEN),
        [this, self](boost::system::error_code ec, std::size_t r_length)
        {
          if (!ec)
          {
            readSocksReply();           
          }
          else
          {
            //std::cout<<"error: "<<ec<<std::endl;
          }
        });
    }
    //read socks reply 
    void readSocksReply(){
      auto self(shared_from_this());
      socket_.async_read_some(boost::asio::buffer(data_, SOCKS4_REPLY_LEN),
      [this, self](boost::system::error_code ec, std::size_t length)
      {
        if (!ec)
        {
          //request accept
          if(data_[1] == 90) {
            std::shared_ptr<npshell_session> s = std::make_shared<npshell_session>(io_context_, endpoint_, std::move(socket_), test_case_, session_id_);
            s->start();
          } 
        }
      }); 
    }
    //do connect
    void do_connect(tcp::endpoint endpoint) {
      auto self(shared_from_this());
      socket_.async_connect(endpoint,
        [this, self](boost::system::error_code ec)
        {
          if (!ec)
          {
            sendSocksRequest();
          }
          else
          {
          }
          
        });
            
    }
    //do write
    void do_write(std::string data) {
      
      auto self(shared_from_this());
      boost::asio::async_write(socket_, boost::asio::buffer(data.c_str(), data.size()),
        [this, self](boost::system::error_code ec, std::size_t r_length)
        {
          if (!ec)
          {
           
          }
          else
          {
            //std::cout<<"error: "<<ec<<std::endl;
          }
        });
    }
    
    
    //tcp::socket socket_;
    enum { max_length = 15020 };
    unsigned char data_[max_length];
    std::string domain_;
    std::string port_;
    tcp::resolver resolver_;
    tcp::socket socket_;
    boost::asio::io_context &io_context_;
    tcp::endpoint endpoint_;
    std::string test_case_;
    std::string session_id_;
};

int main(int argc, char* argv[])
{
  try
  {
    boost::asio::io_context io_context;
    tcp::resolver resolver_(io_context);
    //for each npshell connection
    std::vector<struct shell_request>shell_requests;
    shell_requests.resize(MAX_SERVER);
    //socks host, socks port
    std::string socks_host, socks_port;
    socks_host.clear();
    socks_port.clear();
    //parse query string
    request_parser parser;
    parser.parseQueryString(getenv("QUERY_STRING"), shell_requests, socks_host, socks_port);
    //resolve npshell domain name
    int session_num = 0;
    for(int i = 0; i < MAX_SERVER; i++) {
      //resolve host name, for valid requests
      if(shell_requests[i].valid == 1) {
        //std::cout<<i<<": "<<shell_requests_[i].host<<" "<<shell_requests_[i].port<<" "<<shell_requests_[i].file<<std::endl;
        tcp::resolver::query query(shell_requests[i].host, shell_requests[i].port); 
        std::string test_case = shell_requests[i].file;
        std::string session_id = "s";
        std::stringstream ss;
        ss.str("");
        //get session id for html 
        ss<<session_num;
        session_id.append(ss.str());
        shell_requests[i].session_id = session_id;
        session_num++;
        //async resolve      
        resolver_.async_resolve(
          query,
          [&io_context, test_case, session_id, socks_host, socks_port](boost::system::error_code ec, tcp::resolver::iterator ep_iter)
            {
              if (!ec)
              { 
                tcp::endpoint endpoint = std::move(ep_iter->endpoint());
                //connect to socks server
                std::make_shared<connect_session>(io_context, socks_host, socks_port, endpoint, test_case, session_id)->start();
              }
              
            });
      }
    }
    //output html
    outputHtml(shell_requests);

    io_context.run();

  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}

// send html to client
void outputHtml(std::vector<shell_request> &shell_requests) {
  //content type
  std::string html = "Content-type: text/html\r\n\r\n";
  html.append(R"(
  <!DOCTYPE html>
  <html lang="en">
    <head>
      <meta charset="UTF-8" />
      <title>NP Project 3 Sample Console</title>
      <link
        rel="stylesheet"
        href="https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css"
        integrity="sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2"
        crossorigin="anonymous"
      />
      <link
        href="https://fonts.googleapis.com/css?family=Source+Code+Pro"
        rel="stylesheet"
      />
      <link
        rel="icon"
        type="image/png"
        href="https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png"
      />
      <style>
        * {
          font-family: 'Source Code Pro', monospace;
          font-size: 1rem !important;
        }
        body {
          background-color: #212529;
        }
        pre {
          color: #cccccc;
        }
        b {
          color: #01b468;
        }
      </style>
    </head>
    <body>
      <table class="table table-dark table-bordered">
        <thead>
          <tr>
  )");
  for(int i = 0; i < MAX_SERVER; i++) {
    if(shell_requests[i].valid == 1) {
          html.append("<th scope=\"col\">");
          html.append(shell_requests[i].host);
          html.push_back(':');
          html.append(shell_requests[i].port);
          html.append("</th>");
    }
  }
  html.append(R"(
    </tr>
      </thead>
      <tbody>
        <tr>
  )");
  for(int i = 0; i < MAX_SERVER; i++) {
    if(shell_requests[i].valid == 1) {
      html.append("<td><pre id=\"");
      html.append(shell_requests[i].session_id);
      html.append("\" class=\"mb-0\"></pre></td>");
    }
  }
  html.append(R"(
    </tr>
        </tbody>
      </table>
    </body>
  </html>
  )");
  std::cout<<html<<std::endl;
}
