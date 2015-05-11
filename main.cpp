#include "ftpserver.h"
#include "tcpserver.h"

#include <cassert>
#include <string>

using namespace miniserver;
using namespace std;

namespace {

FTPServer* g_ftpserver = 0;

}

int Main(int argc, char** argv) {

  /*************************/
  /* Launch the FTP Server */
  /*************************/
  g_ftpserver = new FTPServer(DEFAULT_CTRL_PORT);
  g_ftpserver->init();
  g_ftpserver->run();
  delete g_ftpserver;

  return 0;
}