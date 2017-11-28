// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_LOCAL_SYNC_TEST_SERVER_H_
#define NET_TEST_LOCAL_SYNC_TEST_SERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/test/local_test_server.h"

namespace net {

// Runs a Python-based sync test server on the same machine in which the
// LocalSyncTestServer runs.
class LocalSyncTestServer : public LocalTestServer {
 public:
  // Initialize a sync server that listens on localhost using ephemeral ports
  // for sync and p2p notifications.
  LocalSyncTestServer();

  // Initialize a sync server that listens on |port| for sync updates and
  // |xmpp_port| for p2p notifications.
  LocalSyncTestServer(uint16 port, uint16 xmpp_port);

  virtual ~LocalSyncTestServer();

  // Calls LocalTestServer::AddCommandLineArguments and then appends the
  // --xmpp-port flag to |command_line| if required. Returns true on success.
  virtual bool AddCommandLineArguments(
      CommandLine* command_line) const override;

 private:
  // Port on which the Sync XMPP server listens.
  uint16 xmpp_port_;

  DISALLOW_COPY_AND_ASSIGN(LocalSyncTestServer);
};

}  // namespace net

#endif  // NET_TEST_LOCAL_SYNC_TEST_SERVER_H_
