#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <cstring>
#include <sstream>
#include <fstream>
//#include <unistd.h>
//#include <sys/types.h>
//#include <sys/wait.h>
//#include <fcntl.h>
#include <process.h>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include "request_parser_win.hpp"
#include "http_server_win.hpp"

using boost::asio::ip::tcp;
class npshell_session
  : public std::enable_shared_from_this<npshell_session>
{
public:
  npshell_session(boost::asio::io_context& io_context, tcp::endpoint& end_point, std::string test_case, std::string session_id,  std::shared_ptr<tcp::socket> sock)
    : socket_(io_context)
    , cl_socket(sock)
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
            
            //write to client
            do_cl_write(script);

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
    if(!std::getline(file, command)){
      return;
    }
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
            
            //write to client
            do_cl_write(script);

            do_read();
          }
        });
  }
  
  void do_cl_write(std::string data)
  {
     
    auto self(shared_from_this());
    boost::asio::async_write(*cl_socket, boost::asio::buffer(data.c_str(), data.size()),
        [](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            
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

  std::shared_ptr<tcp::socket> cl_socket;
};

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
  {
    socket_ = std::make_shared<tcp::socket>(std::move(socket));
  }

  void start()
  {
    //read HTTP request
    do_read();
  }

private:

  void do_read()
  {
    auto self(shared_from_this());
    socket_->async_read_some(boost::asio::buffer(data_, max_length),
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
              envir.SERVER_ADDR = std::string(socket_->local_endpoint().address().to_string());
              envir.SERVER_PORT = socket_->local_endpoint().port();
              //set remote address and port
              envir.REMOTE_ADDR = std::string(socket_->remote_endpoint().address().to_string());
              envir.REMOTE_PORT = socket_->remote_endpoint().port();
              printCgiEnviron(envir);
              //set cgi environment variables
              setCgiEnv(envir);
              if(envir.CGI.compare("/console.cgi")  == 0) {
                //call console_main 
                console_main();
              }
              else if(envir.CGI.compare("/panel.cgi") == 0) {
                //call panel cgi
                std::string html = panel();
                do_write(html);
              }
              else {

              }
            }
            //bad request
            else {
              //write status code
              std::string status_code = "HTTP/1.1 403 Forbidden\r\n";
              do_write(status_code);
              fprintf(stderr, "bad request\n");
            }
            //echo 
            //do_write(length);  
          }
        });
  }

  void do_write(std::string data)
  {
    auto self(shared_from_this());
    boost::asio::async_write(*socket_, boost::asio::buffer(data.c_str(), data.size()),
        [](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
          }
        });
  }
  // console.cgi function
  int console_main()
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
          //share socket ptr
          std::shared_ptr<tcp::socket> sock(socket_);
          //async resolve
        auto self(shared_from_this());
          resolver.async_resolve(
            query,
            [&end_point, &io_context, test_case, session_id, &sock](boost::system::error_code ec, tcp::resolver::iterator ep_iter)
              {
                if (!ec)
                { 
                  end_point = std::move(ep_iter->endpoint());
                  std::shared_ptr<npshell_session> s = std::make_shared<npshell_session>(io_context, end_point, test_case, session_id, sock);
                  s->start();
                }
              });
        }
      }
      //output html
      std::string html = outputHtml(shell_requests);
      do_write(html);
      
      io_context.run();
    }
    catch (std::exception& e)
    {
      std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
  }
  // send html to client
  std::string outputHtml(std::vector<shell_request> &shell_requests) {
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
  return html;
  }
  std::string panel() {
    std::string panel = "Content-type: text/html\r\n\r\n";

    panel.append(R"(
    <!DOCTYPE html>
    <html lang="en">
    <head>
      <title>NP Project 3 Panel</title>
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
        href="https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/512/dashboard-512.png"
      />
      <style>
        * {
          font-family: 'Source Code Pro', monospace;
        }
      </style>
    </head>
    <body class="bg-secondary pt-5">)");
   
    panel.append(R"(
    <form action="console.cgi" method="GET">
      <table class="table mx-auto bg-light" style="width: inherit">
        <thead class="thead-dark">
          <tr>
            <th scope="col">#</th>
            <th scope="col">Host</th>
            <th scope="col">Port</th>
            <th scope="col">Input File</th>
          </tr>
        </thead>
        <tbody>)");

    for(int i = 0; i < MAX_SERVER; i++) {
      panel.append(R"(
      <tr>
        <th scope="row" class="align-middle">Session )");
      std::stringstream ss;
      //get session id for html 
      ss.str("");
      ss<<i+1;
      panel.append(ss.str());
      ss.str("");
      panel.append(R"(</th>
      <td>
        <div class="input-group">
          <select name="h)");
      ss<<i;
      panel.append(ss.str());
      
      panel.append(R"(" class="custom-select">
      <option></option>)");

      for(int j = 0; j < 12; j++) {
        panel.append("<option value=\"nplinux");
        std::stringstream ss;
        ss.str("");
        ss<<j+1;
        panel.append(ss.str());
       
        panel.append(".cs.nctu.edu.tw\">nplinux");
        
        panel.append(ss.str());
        panel.append("</option>");
      }
      // {host_menu}
      panel.append(R"(</select>
          <div class="input-group-append">
            <span class="input-group-text">.cs.nctu.edu.tw</span>
          </div>
        </div>
      </td>
      <td>
        <input name="p)");
      
      panel.append(ss.str());
     
      panel.append(R"(" type="text" class="form-control" size="5" />
      </td>
      <td>
        <select name="f)");
    
      panel.append(ss.str());
    
      panel.append(R"(" class="custom-select">
      <option></option>)");
              
      //{test_case_menu}
      for(int j = 0; j < 10; j++) {
        panel.append("<option value=\"t");
        std::stringstream ss;
        ss<<j+1;
        panel.append(ss.str());
        panel.append(".txt\">t");
         panel.append(ss.str());
        panel.append(".txt</option>");
      }
      panel.append(R"(</select>
        </td>
      </tr>)");
    }  
    panel.append(R"(
              <tr>
                <td colspan="3"></td>
                <td>
                  <button type="submit" class="btn btn-info btn-block">Run</button>
                </td>
              </tr>
            </tbody>
          </table>
        </form>
      </body>
    </html>
    )");

    return panel;
  }

  std::shared_ptr<tcp::socket> socket_;
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
            //create thread
            //boost::thread th = boost::thread(boost::bind(&session::start, std::make_shared<session>(std::move(socket)))); 
             std::make_shared<session>(std::move(socket))->start();
            //th.detach();
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
