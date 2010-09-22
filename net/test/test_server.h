// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_TEST_SERVER_H_
#define NET_TEST_TEST_SERVER_H_
#pragma once

#include "build/build_config.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/process_util.h"
#include "net/base/host_port_pair.h"

#if defined(OS_WIN)
#include "base/scoped_handle_win.h"
#endif

#if defined(USE_NSS)
#include "base/ref_counted.h"
#include "net/base/x509_certificate.h"
#endif

class GURL;

namespace net {

class AddressList;

// This object bounds the lifetime of an external python-based HTTP/FTP server
// that can provide various responses useful for testing.
class TestServer {
 public:
  enum Type {
    TYPE_FTP,
    TYPE_HTTP,
    TYPE_HTTPS,
    TYPE_HTTPS_CLIENT_AUTH,
    TYPE_HTTPS_MISMATCHED_HOSTNAME,
    TYPE_HTTPS_EXPIRED_CERTIFICATE,
  };

  TestServer(Type type, const FilePath& document_root);
  ~TestServer();

  bool Start() WARN_UNUSED_RESULT;

  // Stop the server started by Start().
  bool Stop();

  // If you access the server's Kill url, it will exit by itself
  // without a call to Stop().
  // WaitToFinish is handy in that case.
  // It returns true if the server exited cleanly.
  bool WaitToFinish(int milliseconds) WARN_UNUSED_RESULT;

  const FilePath& document_root() const { return document_root_; }
  const HostPortPair& host_port_pair() const { return host_port_pair_; }
  std::string GetScheme() const;
  bool GetAddressList(AddressList* address_list) const WARN_UNUSED_RESULT;

  GURL GetURL(const std::string& path);

  GURL GetURLWithUser(const std::string& path,
                      const std::string& user);

  GURL GetURLWithUserAndPassword(const std::string& path,
                                 const std::string& user,
                                 const std::string& password);

 private:
  // Modify PYTHONPATH to contain libraries we need.
  bool SetPythonPath() WARN_UNUSED_RESULT;

  // Launches the Python test server. Returns true on success.
  bool LaunchPython(const FilePath& testserver_path) WARN_UNUSED_RESULT;

  // Waits for the server to start. Returns true on success.
  bool WaitToStart() WARN_UNUSED_RESULT;

  // Returns path to the root certificate.
  FilePath GetRootCertificatePath();

  // Returns false if our test root certificate is not trusted.
  bool CheckCATrusted() WARN_UNUSED_RESULT;

  // Load the test root cert, if it hasn't been loaded yet.
  bool LoadTestRootCert() WARN_UNUSED_RESULT;

  // Returns path to the SSL certificate we should use, or empty path
  // if not applicable.
  FilePath GetCertificatePath();

  // Document root of the test server.
  FilePath document_root_;

  // Directory that contains the SSL certificates.
  FilePath certificates_dir_;

  // Address the test server listens on.
  HostPortPair host_port_pair_;

  // Handle of the Python process running the test server.
  base::ProcessHandle process_handle_;

#if defined(OS_WIN)
  // JobObject used to clean up orphaned child processes.
  ScopedHandle job_handle_;

  // The file handle the child writes to when it starts.
  ScopedHandle child_fd_;
#endif

#if defined(OS_POSIX)
  // The file descriptor the child writes to when it starts.
  int child_fd_;
  file_util::ScopedFD child_fd_closer_;
#endif

#if defined(USE_NSS)
  scoped_refptr<X509Certificate> cert_;
#endif

  Type type_;

  DISALLOW_COPY_AND_ASSIGN(TestServer);
};

}  // namespace net

#endif  // NET_TEST_TEST_SERVER_H_
