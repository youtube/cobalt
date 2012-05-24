// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/local_sync_test_server.h"

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "net/test/test_server.h"

namespace net {

LocalSyncTestServer::LocalSyncTestServer()
    : LocalTestServer(net::TestServer::TYPE_SYNC,
                      net::TestServer::kLocalhost,
                      FilePath()),
      xmpp_port_(0) {}

LocalSyncTestServer::LocalSyncTestServer(uint16 port, uint16 xmpp_port)
    : LocalTestServer(net::TestServer::TYPE_SYNC,
                      net::TestServer::kLocalhost,
                      FilePath()),
      xmpp_port_(xmpp_port) {
  SetPort(port);
}

LocalSyncTestServer::~LocalSyncTestServer() {}

bool LocalSyncTestServer::AddCommandLineArguments(
    CommandLine* command_line) const {
  LocalTestServer::AddCommandLineArguments(command_line);
  if (xmpp_port_ != 0) {
    std::string xmpp_port_str = base::IntToString(xmpp_port_);
    command_line->AppendArg("--xmpp-port=" + xmpp_port_str);
  }
  return true;
}

}  // namespace net
