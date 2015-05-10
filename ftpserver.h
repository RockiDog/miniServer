#ifndef MINIFTP_FTP_SERVER_
#define MINIFTP_FTP_SERVER_

#include "tcpserver.h"

#include <string>
#include <vector>

namespace miniserver {

const int DEFAULT_CTRL_PORT = 21;
const int MAX_FILE_INFO_LENGHT = 40;

class FTPServer {
 public:
  FTPServer(const int ctrl_port);
  ~FTPServer();

 public:
  bool init();
  void run();

 private:
  bool put(const std::string& server_path);
  bool get(const std::string& server_path);
  bool chdir(const std::string& dir);
  std::string lsfile(const std::string& dir);
  bool quit();

 private:
  std::vector<std::string> split(const std::string& str, char separator);
  std::string to_string(int v);
  //void updatedir();

 private:
  int ctrl_port_;
  std::string current_path_;
  std::string root_path_;
  TCPServer ctrl_server_;
  TCPServer data_server_;
};

}

#endif
