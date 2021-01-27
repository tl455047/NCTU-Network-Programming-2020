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

using boost::asio::ip::tcp;

class npshell_session
  : public std::enable_shared_from_this<npshell_session>
{
public:
  npshell_session(boost::asio::io_context& io_context, tcp::endpoint& end_point, std::string test_case, std::string session_id)
    : socket_(io_context)
    , endpoint_(std::move(end_point))
  {
    //get test_case file name
    test_case_ = test_case;
    //get session id
    session_id_ = session_id;

  }

  void start()
  {
    //do connection
    do_connect();
  }

private:
  void do_connect() {
    
    //std::cout << endpoint_ << std::endl;
    
    auto self(shared_from_this());
    socket_.async_connect(endpoint_,
        [this, self](boost::system::error_code ec)
        {
          if (!ec)
          {
            //open testcase file
            file.open(test_case_, std::ios::in);
            
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
            // output shell result to client
            std::string data = std::string(data_);
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

 

int main(int argc, char* argv[])
{
  try
  {
    boost::asio::io_context io_context;
    tcp::resolver resolver(io_context);
    tcp::endpoint end_point;
    //for each npshell connection
    std::vector<struct shell_request>shell_requests;
    shell_requests.resize(MAX_SERVER);
    
    //printEnv();
    //parse query string
    request_parser parser;
    parser.parseQueryString(getenv("QUERY_STRING"), shell_requests);
    //for html s id
    int session_num = 0;
    for(int i = 0; i < MAX_SERVER; i++) {
      //resolve host name, for valid requests
      if(shell_requests[i].valid == 1) {
        //std::cout<<i<<": "<<shell_requests[i].host<<" "<<shell_requests[i].port<<" "<<shell_requests[i].file<<std::endl;
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
        resolver.async_resolve(
          query,
          [&end_point, &io_context, test_case, session_id](boost::system::error_code ec, tcp::resolver::iterator ep_iter)
            {
              if (!ec)
              { 
                end_point = std::move(ep_iter->endpoint());
                std::shared_ptr<npshell_session> s = std::make_shared<npshell_session>(io_context, end_point, test_case, session_id);
                s->start();
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