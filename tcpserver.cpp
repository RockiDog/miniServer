#include "TCPServer.h"

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <list>
#include <utility>

using namespace std;
using namespace miniserver;

TCPServer::TCPServer()
    : client_socket_(INVALID_SOCKET),
      listen_socket_(INVALID_SOCKET) {}

bool TCPServer::Listen(const int port) {
  port_ = port;

  /*****************************/
  /* Initialize Windows socket */
  /*****************************/
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data_) != 0)
    return false;

  /***************************************/
  /* Resolve the server address and port */
  /***************************************/
  client_addr_.sin_family = AF_INET;
  client_addr_.sin_port = htons(port_);
  if (inet_pton(AF_INET, client_name_.c_str(), &client_addr_.sin_addr.s_addr) != 0)
    return false;

  /*******************/
  /* Create a socket */
  /*******************/
  listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listen_socket_ == INVALID_SOCKET) {
    WSACleanup();
    return false;
  }

  /*******************/
  /* Attempt to bind */
  /*******************/
  if (bind(listen_socket_, (const SOCKADDR*)(&client_addr_), sizeof(client_addr_)) == SOCKET_ERROR) {
    WSACleanup();
    return false;
  }

  /************************/
  /* Listen to the client */
  /************************/
  if (listen(listen_socket_, SOMAXCONN) == SOCKET_ERROR) {
    closesocket(listen_socket_);
    WSACleanup();
    return false;
  }

  return true;
}

bool TCPServer::Accept() {

  /**************************/
  /* Accept a client socket */
  /**************************/
  client_socket_ = accept(listen_socket_, 0, 0);
  if (client_socket_ == INVALID_SOCKET) {
    Close();
    return false;
  }

  /********************************/
  /* No longer need listen socket */
  /********************************/
  closesocket(listen_socket_);
  listen_socket_ = INVALID_SOCKET;
  return true;
}

void TCPServer::Close() {
  if (listen_socket_ != INVALID_SOCKET)
    closesocket(listen_socket_);
  if (client_socket_ != INVALID_SOCKET)
    closesocket(client_socket_);
  WSACleanup();
}

bool TCPServer::Send(const Byte* buffer, long long length) {
  const Byte* cursor = buffer;

  /***************************/
  /* Send the length of data */
  /***************************/
  if (send(client_socket_, (const Byte*)(&length), sizeof(length), 0) == SOCKET_ERROR) {
    Close();
    return false;
  }

  /********************/
  /* Send the content */
  /********************/
  for (int i = 0; i < length / MAX_INT; ++i) {
    if (send(client_socket_, cursor, MAX_INT, 0) == SOCKET_ERROR) {
      Close();
      return false;
    }
    cursor += MAX_INT;
  }
  if (length % MAX_INT != 0) {
    if (send(client_socket_, cursor, length % MAX_INT, 0) == SOCKET_ERROR) {
      Close();
      return false;
    }
  }

  return true;
}

bool TCPServer::Send(const list< pair<Byte*, int> >& blocks, long long length) {

  /***************************/
  /* Send the length of data */
  /***************************/
  if (send(client_socket_, (const Byte*)(&length), sizeof(length), 0) == SOCKET_ERROR) {
    Close();
    return false;
  }

  /********************/
  /* Send the content */
  /********************/
  for (list< pair<Byte*, int> >::const_iterator it = blocks.cbegin(); it != blocks.cend(); ++it) {
    if (send(client_socket_, it->first, it->second, 0) == SOCKET_ERROR) {
      Close();
      return false;
    }
  }

  return true;
}

bool TCPServer::Recv(Byte* buffer, long long length) {
  Byte* cursor = buffer;
  int buffer_size;

  /****************************************/
  /* In case that the length is too large */
  /****************************************/
  for (int i = 0; i < length / MAX_INT; ++i) {
    buffer_size = MAX_INT;
    while (buffer_size > 0) {
      int result = recv(client_socket_, cursor, buffer_size, 0);
      if (result > 0) {
        buffer_size -= result;
        printf("%d bytes received...\n", result);
      } else if (result == 0) {
        printf("Connection closed!\n");
        return false;
      } else {
        printf("Receive failed!\n");
        return false;
      }
    }
  }
  buffer_size = length % MAX_INT;
  while (buffer_size > 0) {
    int result = recv(client_socket_, cursor, buffer_size, 0);
    if (result > 0) {
      buffer_size -= result;
      printf("%d bytes received...\n", result);
    } else if (result == 0) {
      printf("Connection closed!\n");
      return false;
    } else {
      printf("Receive failed!\n");
      return false;
    }
  }

  return true;
}

bool TCPServer::Recv(const list< pair<Byte*, int> >& blocks, long long length) {
  for (list< pair<Byte*, int> >::const_iterator it = blocks.cbegin(); it != blocks.cend(); ++it) {
    int buffer_size = it->second;
    while (buffer_size > 0) {
      int result = recv(client_socket_, it->first, buffer_size, 0);
      if (result > 0) {
        buffer_size -= result;
        printf("%d bytes received...\n", result);
      } else if (result == 0) {
        printf("Connection closed!\n");
        return false;
      } else {
        printf("Receive failed!\n");
        return false;
      }
    }
  }

  return true;
}