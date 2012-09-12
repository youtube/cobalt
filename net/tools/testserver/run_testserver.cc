// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "net/test/local_sync_test_server.h"
#include "net/test/python_utils.h"
#include "net/test/test_server.h"

static void PrintUsage() {
  printf("run_testserver --doc-root=relpath [--http|--https|--ftp|--sync]\n"
         "               [--https-cert=ok|mismatched-name|expired]\n"
         "               [--port=<port>] [--xmpp-port=<xmpp_port>]\n");
  printf("(NOTE: relpath should be relative to the 'src' directory.\n");
  printf("       --port and --xmpp-port only work with the --sync flag.)\n");
}

// Launches the chromiumsync_test script, testing the --sync functionality.
static bool RunSyncTest() {
 if (!net::TestServer::SetPythonPath()) {
    LOG(ERROR) << "Error trying to set python path. Exiting.";
    return false;
  }

  FilePath sync_test_path;
  if (!net::TestServer::GetTestServerDirectory(&sync_test_path)) {
    LOG(ERROR) << "Error trying to get python test server path.";
    return false;
  }

  sync_test_path =
      sync_test_path.Append(FILE_PATH_LITERAL("chromiumsync_test.py"));

  CommandLine python_command(CommandLine::NO_PROGRAM);
  if (!GetPythonCommand(&python_command)) {
    LOG(ERROR) << "Could not get python runtime command.";
    return false;
  }

  python_command.AppendArgPath(sync_test_path);
  if (!base::LaunchProcess(python_command, base::LaunchOptions(), NULL)) {
    LOG(ERROR) << "Failed to launch test script.";
    return false;
  }
  return true;
}

// Gets a port value from the switch with name |switch_name| and writes it to
// |port|. Returns true if successful and false otherwise.
static bool GetPortFromSwitch(const std::string& switch_name, uint16* port) {
  DCHECK(port != NULL) << "|port| is NULL";
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  int port_int = 0;
  if (command_line->HasSwitch(switch_name)) {
    std::string port_str = command_line->GetSwitchValueASCII(switch_name);
    if (!base::StringToInt(port_str, &port_int)) {
      LOG(WARNING) << "Could not extract port from switch " << switch_name;
      return false;
    }
  }
  *port = static_cast<uint16>(port_int);
  return true;
}

int main(int argc, const char* argv[]) {
  base::AtExitManager at_exit_manager;
  MessageLoopForIO message_loop;

  // Process command line
  CommandLine::Init(argc, argv);
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (!logging::InitLogging(
          FILE_PATH_LITERAL("testserver.log"),
          logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
          logging::LOCK_LOG_FILE,
          logging::APPEND_TO_OLD_LOG_FILE,
          logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS)) {
    printf("Error: could not initialize logging. Exiting.\n");
    return -1;
  }

  TestTimeouts::Initialize();

  if (command_line->GetSwitches().empty() ||
      command_line->HasSwitch("help") ||
      ((command_line->HasSwitch("port") ||
        command_line->HasSwitch("xmpp-port")) &&
       !command_line->HasSwitch("sync"))) {
    PrintUsage();
    return -1;
  }

  net::TestServer::Type server_type(net::TestServer::TYPE_HTTP);
  if (command_line->HasSwitch("https")) {
    server_type = net::TestServer::TYPE_HTTPS;
  } else if (command_line->HasSwitch("ftp")) {
    server_type = net::TestServer::TYPE_FTP;
  } else if (command_line->HasSwitch("sync")) {
    server_type = net::TestServer::TYPE_SYNC;
  } else if (command_line->HasSwitch("sync-test")) {
    return RunSyncTest() ? 0 : -1;
  }

  net::TestServer::SSLOptions ssl_options;
  if (command_line->HasSwitch("https-cert")) {
    server_type = net::TestServer::TYPE_HTTPS;
    std::string cert_option = command_line->GetSwitchValueASCII("https-cert");
    if (cert_option == "ok") {
      ssl_options.server_certificate = net::TestServer::SSLOptions::CERT_OK;
    } else if (cert_option == "mismatched-name") {
      ssl_options.server_certificate =
          net::TestServer::SSLOptions::CERT_MISMATCHED_NAME;
    } else if (cert_option == "expired") {
      ssl_options.server_certificate =
          net::TestServer::SSLOptions::CERT_EXPIRED;
    } else {
      printf("Error: --https-cert has invalid value %s\n", cert_option.c_str());
      PrintUsage();
      return -1;
    }
  }

  FilePath doc_root = command_line->GetSwitchValuePath("doc-root");
  if ((server_type != net::TestServer::TYPE_SYNC) && doc_root.empty()) {
    printf("Error: --doc-root must be specified\n");
    PrintUsage();
    return -1;
  }

  scoped_ptr<net::TestServer> test_server;
  switch (server_type) {
    case net::TestServer::TYPE_HTTPS: {
      test_server.reset(new net::TestServer(server_type,
                                            ssl_options,
                                            doc_root));
      break;
    }
    case net::TestServer::TYPE_SYNC: {
      uint16 port = 0;
      uint16 xmpp_port = 0;
      if (!GetPortFromSwitch("port", &port) ||
          !GetPortFromSwitch("xmpp-port", &xmpp_port)) {
        printf("Error: Could not extract --port and/or --xmpp-port.\n");
        return -1;
      }
      test_server.reset(new net::LocalSyncTestServer(port, xmpp_port));
      break;
    }
    default: {
      test_server.reset(new net::TestServer(server_type,
                                            net::TestServer::kLocalhost,
                                            doc_root));
      break;
    }
  }

  if (!test_server->Start()) {
    printf("Error: failed to start test server. Exiting.\n");
    return -1;
  }

  if (!file_util::DirectoryExists(test_server->document_root())) {
    printf("Error: invalid doc root: \"%s\" does not exist!\n",
        UTF16ToUTF8(test_server->document_root().LossyDisplayName()).c_str());
    return -1;
  }

  printf("testserver running at %s (type ctrl+c to exit)\n",
         test_server->host_port_pair().ToString().c_str());

  message_loop.Run();
  return 0;
}
