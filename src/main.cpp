#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <string>
#include <vector>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <fstream>

using boost::asio::ip::tcp;

#pragma pack(push, 1)
struct LogRecord {
    char sensor_id[32]; // supondo um ID de sensor de até 32 caracteres
    std::time_t timestamp; // timestamp UNIX
    double value; // valor da leitura
};
#pragma pack(pop)

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
    read_message();
  }

private:
  
  bool starts_with(const std::string& prefix,const std::string& message){
    if (prefix.length() > message.length()) {
        return false;
    }
    for (size_t i = 0; i < prefix.length(); i++) {
        if (prefix[i] != message[i]) {
            return false; 
        }
    }
    return true;
  }

  std::vector<std::string> splitString(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> substrings;
    boost::split(substrings, str, boost::is_any_of(delimiter), boost::token_compress_on);
    return substrings;
  }

  std::time_t string_to_time_t(const std::string& time_string) {
    std::tm tm = {};
    std::istringstream ss(time_string);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return std::mktime(&tm);
  }

  std::string time_t_to_string(std::time_t time) {
    std::tm* tm = std::localtime(&time);
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
  }

  void read_message()
  {
    auto self(shared_from_this());
    boost::asio::async_read_until(socket_, buffer_, "\r\n",
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            std::istream is(&buffer_);
            std::string message(std::istreambuf_iterator<char>(is), {});
            std::cout << "Received: " << message << std::endl;
            if(starts_with("LOG", message))
            {
                std::vector<std::string> splitedMessage = splitString(message, "|");
                LogRecord log;
                strcpy(log.sensor_id, splitedMessage[1].c_str());
                log.timestamp = string_to_time_t(splitedMessage[2]);
                log.value = std::stod(splitedMessage[3]);
                std::string fileName = splitedMessage[1] + ".dat";
                std::fstream file(fileName, std::fstream::out | std::fstream::in | std::fstream::binary | std::fstream::app); 
                if (file.is_open())
                {
                    file.write((char*)&log, sizeof(LogRecord));
                    file.close();
                }
                read_message();
            }
            else if(starts_with("GET", message))
            {
                std::vector<std::string> splitedMessage = splitString(message, "|");
                int numberLogs = std::stoi(splitedMessage[2]); 
                std::string fileName = splitedMessage[1] + ".dat";
                std::fstream file(fileName, std::fstream::in | std::fstream::binary); 
                std::string result;
                if (file.is_open())
                {
                    file.seekg(0, std::ios::end);
                    int file_size = file.tellg();
                    file.seekg(0, std::ios::beg);
		                int n = file_size/sizeof(LogRecord);
                    int readLogs = numberLogs > n ? n : numberLogs;
                    std::cout << file_size << " asdasd " << sizeof(LogRecord) << std::endl;
                    result = std::to_string(readLogs);
                    for(int i = 0; i < readLogs; i++)
                    {
                      LogRecord log;
		                  file.read((char*)&log, sizeof(LogRecord));
                      std::string time = time_t_to_string(log.timestamp);
                      std::string value = std::to_string(log.value);
                      result = result + ";" + time + "|" + value;
                    } 
                    file.close();
                }
              write_message(result);
            }
            else
            {

            }
          }
        });
  }

  void write_message(const std::string& message)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(message),
        [this, self, message](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            read_message();
          }
        });
  }

  tcp::socket socket_;
  boost::asio::streambuf buffer_;
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
    accept();
  }

private:
  void accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<session>(std::move(socket))->start();
          }

          accept();
        });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
  if (argc != 2)
  {
    std::cerr << "Usage: chat_server <port>\n";
    return 1;
  }

  boost::asio::io_context io_context;

  server s(io_context, std::atoi(argv[1]));

  io_context.run();

  return 0;
}
