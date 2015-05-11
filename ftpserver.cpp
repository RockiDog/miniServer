#include "ftpserver.h"
#include "tcpserver.h"

#include <Shlwapi.h>
#include <Windows.h>

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <list>
#include <string>
#include <utility>
#include <vector>

using namespace miniserver;
using namespace std;

FTPServer::FTPServer(const int ctrl_port) : ctrl_port_(ctrl_port) {
  int len = GetCurrentDirectory(0, 0);
  char* dir = new char[len];
  if (GetCurrentDirectory(len, dir) != 0) {
    root_path_ = dir;
    current_path_ = dir;
  } else {
    printf("GetCurrentDirectory failed(%d)\n", GetLastError());
  }

  delete [] dir;
}

FTPServer::~FTPServer() {}

bool FTPServer::init() {
  if (ctrl_server_.Listen(ctrl_port_) == false) {
    printf("Failed to listen to the control port!\n");
    return false;
  } else {
    printf("Listening to the control port %d...\n", ctrl_port_);
    return true;
  }
}

void FTPServer::run() {
  printf("FTP Server running...\n");
  if (ctrl_server_.Accept() == false) {
    printf("I don't know what happened but the server just broken down...\n");
    return;
  }
  printf("One client connected\n");

  while (true) {
    /**********************************/
    /* Receive the client request */
    /**********************************/
    long long length;
    if (ctrl_server_.Recv((Byte*)(&length), sizeof(length)) == true) {

      /*********************/
      /* Parse the request */
      /*********************/
      Byte* buffer = new char[length + 1];
      buffer[length] = '\0';
      vector<string> command = split(buffer, ':');
      delete [] buffer;

      if (ctrl_server_.Recv(buffer, length) == true) {

        /***********************************************/
        /* LIST to list the files under the given path */
        /***********************************************/
        if (command[0] == "LIST") {
          if (command.size() < 2) {
            string error_message = "?Too few parameters!";
            if (ctrl_server_.Send(error_message.c_str(), error_message.size()) == false)
              break;
          } else {
            string file_list = lsfile(command[1]);
            if (ctrl_server_.Send(file_list.c_str(), file_list.size()) == false)
              break;
          }

        /**************************/
        /* CD to change directory */
        /**************************/
        } else if (command[0] == "CD") {
          if (command.size() == 1) {
            if (chdir("") == false)
              break;
          } else {
            if (chdir(command[1]) == false)
              break;
          }
          if (ctrl_server_.Send(current_path_.c_str(), current_path_.size()) == false)
            break;

        /***********************/
        /* PUT to upload files */
        /***********************/
        } else if (command[0] == "PUT") {
          if (command.size() < 2) {
            string error_message = "?Too few parameters!";
            if (ctrl_server_.Send(error_message.c_str(), error_message.size()) == false)
              break;
          } else {
            if (put(command[1]) == false)
              break;
          }

        /*************************/
        /* GET to download files */
        /*************************/
        } else if (command[0] == "GET") {
          if (command.size() < 2) {
            string error_message = "?Too few parameters!";
            if (ctrl_server_.Send(error_message.c_str(), error_message.size()) == false)
              break;
          } else {
            if (get(command[1]) == false)
              break;
          }

        /*****************/
        /* Wrong command */
        /*****************/
        } else {
          string error_message = "?No such command!";
          ctrl_server_.Send(error_message.c_str(), error_message.size());
        }
      } else {
        break;
      }
    } else {
      break;
    }
  }
  printf("Service shut down!\n");
}

bool FTPServer::put(const string& server_path) {

  /*****************************************************/
  /* Parse the path & Make sure the directory is valid */
  /*****************************************************/
  vector<string> path = split(server_path, '\\');
  const string& filename = path.back();
  string put_dir = current_path_ + server_path.substr(0, server_path.length() - filename.length());
  if (PathFileExists(put_dir.c_str()) == false) {
    string error_message = "?No such directory!";
    ctrl_server_.Send(error_message.c_str(), error_message.size());
    return false;
  }
  ofstream ofs(server_path);
  if (!ofs) {
    printf("Can not create file!\n");
    return false;
  }

  /*******************************/
  /* Generate a random data port */
  /*******************************/
  const int data_port = rand() % (65536 - 1024) + 1024;
  string data_port_message = to_string(rand() % (65536 - 1024) + 1024);
  if (ctrl_server_.Send(data_port_message.c_str(), data_port_message.size()) == false)
    return false;

  /***************************/
  /* Listen on the data port */
  /***************************/
  if (data_server_.Listen(data_port) == false) {
    printf("Failed to listen to the data port!\n");
    return false;
  } else {
    printf("Listening to the data port %d...\n", ctrl_port_);
    return true;
  }

  /**************************************************/
  /* Accept the data socket & Receive the file data */
  /**************************************************/
  if (data_server_.Accept() == false) {
    printf("I don't know what happened but the server just broken down...\n");
    return false;
  }
  printf("One data client connected\n");

  /*********************/
  /* Transfer the file */
  /*********************/
  streamsize filesize;
  if (data_server_.Recv((Byte*)(&filesize), sizeof(filesize)) == true) {

    /*********************/
    /* Create block list */
    /*********************/
    list< pair<Byte*, int> > blocks;
    for (int i = 0; i < filesize / MAX_INT; ++i) {
      Byte* block = new Byte[MAX_INT];
      blocks.push_back(make_pair(block, MAX_INT));
    }
    if (filesize % MAX_INT != 0) {
      Byte* block = new Byte[filesize % MAX_INT];
      blocks.push_back(make_pair(block, filesize % MAX_INT));
    }

    /********************/
    /* Receive the file */
    /********************/
    if (data_server_.Recv(blocks, filesize) == true) {

      /* Send back the acknowledge information */
      if (ctrl_server_.Send((Byte*)(&filesize), sizeof(filesize)) == false) {
        for (list< pair<Byte*, int> >::const_iterator it = blocks.cbegin(); it != blocks.cend(); ++it)
          delete[] it->first;
        ofs.close();
        data_server_.Close();
        return false;
      }

      /**************************/
      /* Write back to the disk */
      /**************************/
      for (list< pair<Byte*, int> >::const_iterator it = blocks.cbegin(); it != blocks.cend(); ++it) {
        ofs.write(it->first, it->second);
        delete [] it->first;
      }
      ofs.close();
      data_server_.Close();
      return true;
    } else {
      for (list< pair<Byte*, int> >::const_iterator it = blocks.cbegin(); it != blocks.cend(); ++it)
        delete [] it->first;
      ofs.close();
      data_server_.Close();
      return false;
    }
  }

  ofs.close();
  data_server_.Close();
  return false;
}

bool FTPServer::get(const string& server_path) {

  /*********************************************/
  /* Make sure the file exists & Read the file */
  /*********************************************/
  ifstream ifs(server_path);
  if (!ifs) {
    printf("Can not create file!\n");
    return false;
  }
  streamsize filesize = ifs.tellg();
  list< pair<Byte*, int> > blocks;
  for (int i = 0; i < filesize / MAX_INT; ++i) {
    Byte* block = new Byte[MAX_INT];
    ifs.read(block, MAX_INT);
    blocks.push_back(make_pair(block, MAX_INT));
  }
  if (filesize % MAX_INT != 0) {
    Byte* block = new Byte[filesize % MAX_INT];
    ifs.read(block, filesize % MAX_INT);
    blocks.push_back(make_pair(block, filesize % MAX_INT));
  }

  /*******************************/
  /* Generate a random data port */
  /*******************************/
  const int data_port = rand() % (65536 - 1024) + 1024;
  string data_port_message = to_string(rand() % (65536 - 1024) + 1024);
  if (ctrl_server_.Send(data_port_message.c_str(), data_port_message.size()) == false)
    return false;

  /***************************/
  /* Listen on the data port */
  /***************************/
  if (data_server_.Listen(data_port) == false) {
    printf("Failed to listen to the data port!\n");
    return false;
  } else {
    printf("Listening to the data port %d...\n", ctrl_port_);
    return true;
  }

  /**************************************************/
  /* Accept the data socket & Receive the file data */
  /**************************************************/
  if (data_server_.Accept() == false) {
    printf("I don't know what happened but the server just broken down...\n");
    return false;
  }
  printf("One data client connected\n");

  /*********************/
  /* Transfer the file */
  /*********************/
  bool result = data_server_.Send(blocks, filesize);
  for (list< pair<Byte*, int> >::const_iterator it = blocks.cbegin(); it != blocks.cend(); ++it)
    delete [] it->first;
  ifs.close();
  data_server_.Close();
  if (result == true)
    return ctrl_server_.Send((Byte*)(&filesize), sizeof(filesize));
  else
    return false;
}

bool FTPServer::chdir(const string& dir) {
  const string* d = 0;
  if (dir.length() == 0)
    d = &root_path_;
  else
    d = &dir;

  if (SetCurrentDirectory(d->c_str()) == false) {
    printf("GetCurrentDirectory failed(%d)\n", GetLastError());
    return false;
  }
  current_path_ = *d;
  return true;
}

string FTPServer::lsfile(const string& dir) {
  WIN32_FIND_DATA w32_find_data;
  HANDLE h_find = FindFirstFile(dir.c_str(), &w32_find_data);
  if (h_find == INVALID_HANDLE_VALUE) {
    return "?Wrong directory!";
  } else {
    string file_list = dir + "\n";
    do {
      if (w32_find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        char* info = new char[MAX_FILE_INFO_LENGHT];
        sprintf_s(info, MAX_FILE_INFO_LENGHT, " <DIR>  %12.10s\n", w32_find_data.cFileName);
        file_list += info;
        delete [] info;
      } else {
        char* info = new char[MAX_FILE_INFO_LENGHT];
        LARGE_INTEGER filesize;
        filesize.LowPart = w32_find_data.nFileSizeLow;
        filesize.HighPart = w32_find_data.nFileSizeHigh;
        sprintf_s(info, MAX_FILE_INFO_LENGHT, " <FILE> %12.10s %12.10ld bytes\n", w32_find_data.cFileName, filesize.QuadPart);
        file_list += info;
        delete [] info;
      }
    } while (FindNextFile(h_find, &w32_find_data) != 0);
    return file_list;
  }
}

vector<string> FTPServer::split(const string& str, char separator) {
  string s = str;
  if (s.back() == separator)
    s.pop_back();
  char* sub_string = new char[str.length()];
  int sub_len = 0;
  vector<string> strings;

  for (string::const_iterator& it = str.cbegin(); it != str.cend(); ++it) {
    if (*it == separator) {
      sub_string[sub_len] = '\0';
      strings.push_back(sub_string);
      sub_len = 0;
    } else if (it == str.cend() - 1) {
      sub_string[sub_len++] = *it;
      sub_string[sub_len] = '\0';
      strings.push_back(sub_string);
    } else {
      sub_string[sub_len++] = *it;
    }
  }

  delete [] sub_string;
  return strings;
}

string FTPServer::to_string(int v) {
  int bit_width = 1;
  int cardinality = 10;
  while (v / cardinality != 0) {
    cardinality *= 10;
    ++bit_width;
  }
  char* tmp = new char[bit_width + 1];
  memset(tmp, 0, bit_width + 1);
  for (int i = bit_width - 1; i >= 0; --i) {
    tmp[i] = v % 10;
    v /= 10;
  }
  string str(tmp);
  delete tmp;
  return str;
}