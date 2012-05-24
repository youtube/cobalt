// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_LOCAL_TEST_SERVER_H_
#define NET_TEST_LOCAL_TEST_SERVER_H_
#pragma once

#include <string>

#include "base/file_util.h"
#include "base/process.h"
#include "net/test/base_test_server.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

class CommandLine;

namespace net {

// The LocalTestServer runs an external Python-based test server in the
// same machine in which the LocalTestServer runs.
class LocalTestServer : public BaseTestServer {
 public:
  // Initialize a TestServer listening on a specific host (IP or hostname).
  // |document_root| must be a relative path under the root tree.
  LocalTestServer(Type type,
                  const std::string& host,
                  const FilePath& document_root);

  // Initialize a HTTPS TestServer with a specific set of HTTPSOptions.
  // |document_root| must be a relative path under the root tree.
  LocalTestServer(const HTTPSOptions& https_options,
                  const FilePath& document_root);

  virtual ~LocalTestServer();

  bool Start() WARN_UNUSED_RESULT;

  // Stop the server started by Start().
  bool Stop();

  // Modify PYTHONPATH to contain libraries we need.
  static bool SetPythonPath() WARN_UNUSED_RESULT;

  // Returns true if successfully stored the FilePath for the directory of the
  // testserver python script in |*directory|.
  static bool GetTestServerDirectory(FilePath* directory) WARN_UNUSED_RESULT;

  // Adds the command line arguments for the Python test server to
  // |command_line|. Returns true on success.
  virtual bool AddCommandLineArguments(CommandLine* command_line) const;

 private:
  bool Init(const FilePath& document_root);

  // Launches the Python test server. Returns true on success.
  bool LaunchPython(const FilePath& testserver_path) WARN_UNUSED_RESULT;

  // Waits for the server to start. Returns true on success.
  bool WaitToStart() WARN_UNUSED_RESULT;

  // Handle of the Python process running the test server.
  base::ProcessHandle process_handle_;

#if defined(OS_WIN)
  // JobObject used to clean up orphaned child processes.
  base::win::ScopedHandle job_handle_;

  // The pipe file handle we read from.
  base::win::ScopedHandle child_read_fd_;

  // The pipe file handle the child and we write to.
  base::win::ScopedHandle child_write_fd_;
#endif

#if defined(OS_POSIX)
  // The file descriptor the child writes to when it starts.
  int child_fd_;
  file_util::ScopedFD child_fd_closer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(LocalTestServer);
};

}  // namespace net

#endif  // NET_TEST_LOCAL_TEST_SERVER_H_
