// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_REMOTE_TEST_SERVER_H_
#define NET_TEST_REMOTE_TEST_SERVER_H_
#pragma once

#include <string>

#include "net/test/base_test_server.h"

namespace net {

class SpawnerCommunicator;

// The RemoteTestServer runs an external Python-based test server in another
// machine that is different from the machine in which RemoteTestServer runs.
class RemoteTestServer : public BaseTestServer {
 public:
  // Initialize a TestServer listening on a specific host (IP or hostname).
  // |document_root| must be a relative path under the root tree.
  RemoteTestServer(Type type,
                   const std::string& host,
                   const FilePath& document_root);

  // Initialize a HTTPS TestServer with a specific set of HTTPSOptions.
  // |document_root| must be a relative path under the root tree.
  RemoteTestServer(const HTTPSOptions& https_options,
                   const FilePath& document_root);

  virtual ~RemoteTestServer();

  // Starts the Python test server on the host, instead of on the device.
  bool Start() WARN_UNUSED_RESULT;

  // Stops the Python test server that is running on the host machine.
  bool Stop();

 private:
  bool Init(const FilePath& document_root);

  // The local port used to communicate with the TestServer spawner. This is
  // used to control the startup and shutdown of the Python TestServer running
  // on the remote machine. On Android, this port will be redirected to the
  // same port on the host machine.
  int spawner_server_port_;

  // Helper to start and stop instances of the Python test server that runs on
  // the host machine.
  scoped_ptr<SpawnerCommunicator> spawner_communicator_;

  DISALLOW_COPY_AND_ASSIGN(RemoteTestServer);
};

}  // namespace net

#endif  // NET_TEST_REMOTE_TEST_SERVER_H_

