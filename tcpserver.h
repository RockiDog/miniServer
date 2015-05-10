#ifndef MINIFTP_TCP_SERVER_
#define MINIFTP_TCP_SERVER_

#include "tcpserver.h"

#include <WinSock2.h>

#include <list>
#include <string>
#include <utility>

namespace miniserver {

typedef char Byte;
const int MAX_INT = 0x7FFFFFFF;

class TCPServer {
 public:
  TCPServer();

 public:
  bool Listen(const int port);
  bool Accept();
  bool Send(const Byte* data, long long length);
  bool Send(const std::list< std::pair<Byte*, int> >& blocks, long long length);
  bool Recv(char* buffer, long long length);
  bool Recv(const std::list< std::pair<Byte*, int> >& blocks, long long length);
  void Close();

 private:
  WSADATA wsa_data_;
  SOCKET client_socket_;
  SOCKET listen_socket_;

  SOCKADDR_IN client_addr_;
  std::string client_name_;
  int port_;
};
}

#endif