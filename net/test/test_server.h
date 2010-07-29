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
#include "base/process_util.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"

#if defined(OS_WIN)
#include "base/scoped_handle_win.h"
#endif

#if defined(USE_NSS)
#include "net/base/x509_certificate.h"
#endif

namespace net {

const int kHTTPDefaultPort = 1337;
const int kFTPDefaultPort = 1338;

const char kDefaultHostName[] = "localhost";

// This object bounds the lifetime of an external python-based HTTP/HTTPS/FTP
// server that can provide various responses useful for testing.
class TestServerLauncher {
 public:
  TestServerLauncher();
  virtual ~TestServerLauncher();

  enum Protocol {
    ProtoHTTP, ProtoFTP
  };

  // Load the test root cert, if it hasn't been loaded yet.
  bool LoadTestRootCert() WARN_UNUSED_RESULT;

  // Start src/net/tools/testserver/testserver.py and
  // ask it to serve the given protocol.
  // If protocol is HTTP, and cert_path is not empty, serves HTTPS.
  // file_root_url specifies the root url on the server that documents will be
  // served out of. This is /files/ by default.
  // Returns true on success, false if files not found or root cert
  // not trusted.
  bool Start(net::TestServerLauncher::Protocol protocol,
             const std::string& host_name, int port,
             const FilePath& document_root,
             const FilePath& cert_path,
             const std::wstring& file_root_url) WARN_UNUSED_RESULT;

  // Stop the server started by Start().
  bool Stop();

  // If you access the server's Kill url, it will exit by itself
  // without a call to Stop().
  // WaitToFinish is handy in that case.
  // It returns true if the server exited cleanly.
  bool WaitToFinish(int milliseconds) WARN_UNUSED_RESULT;

  // Paths to a good, an expired, and an invalid server certificate
  // (use as arguments to Start()).
  FilePath GetOKCertPath();
  FilePath GetExpiredCertPath();

  FilePath GetDocumentRootPath() { return document_root_dir_; }

  // Issuer name of the root cert that should be trusted for the test to work.
  static const wchar_t kCertIssuerName[];

  // Hostname to use for test server
  static const char kHostName[];

  // Different hostname to use for test server (that still resolves to same IP)
  static const char kMismatchedHostName[];

  // Port to use for test server
  static const int kOKHTTPSPort;

  // Port to use for bad test server
  static const int kBadHTTPSPort;

 private:
  // Wait a while for the server to start, return whether
  // we were able to make a connection to it.
  bool WaitToStart(const std::string& host_name, int port) WARN_UNUSED_RESULT;

  // Append to PYTHONPATH so Python can find pyftpdlib and tlslite.
  void SetPythonPath();

  // Path to our test root certificate.
  FilePath GetRootCertPath();

  // Returns false if our test root certificate is not trusted.
  bool CheckCATrusted() WARN_UNUSED_RESULT;

  // Initilize the certificate path.
  void InitCertPath();

  FilePath document_root_dir_;

  FilePath cert_dir_;

  base::ProcessHandle process_handle_;

#if defined(OS_WIN)
  // JobObject used to clean up orphaned child processes.
  ScopedHandle job_handle_;
#endif

#if defined(USE_NSS)
  scoped_refptr<X509Certificate> cert_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TestServerLauncher);
};

#if defined(OS_WIN)
// Launch test server as a job so that it is not orphaned if the test case is
// abnormally terminated.
bool LaunchTestServerAsJob(const std::wstring& cmdline,
                           bool start_hidden,
                           base::ProcessHandle* process_handle,
                           ScopedHandle* job_handle);
#endif

// This object bounds the lifetime of an external python-based HTTP/FTP server
// that can provide various responses useful for testing.
class BaseTestServer : public base::RefCounted<BaseTestServer> {
 protected:
  BaseTestServer() {}

 public:
  bool WaitToFinish(int milliseconds) {
    return launcher_.WaitToFinish(milliseconds);
  }

  bool Stop() {
    return launcher_.Stop();
  }

  GURL TestServerPage(const std::string& base_address,
      const std::string& path) {
    return GURL(base_address + path);
  }

  GURL TestServerPage(const std::string& path) {
    // TODO(phajdan.jr): Check for problems with IPv6.
    return GURL(scheme_ + "://" + host_name_ + ":" + port_str_ + "/" + path);
  }

  GURL TestServerPage(const std::string& path,
                      const std::string& user,
                      const std::string& password) {
    // TODO(phajdan.jr): Check for problems with IPv6.

    if (password.empty())
      return GURL(scheme_ + "://" + user + "@" +
                  host_name_ + ":" + port_str_ + "/" + path);

    return GURL(scheme_ + "://" + user + ":" + password +
                "@" + host_name_ + ":" + port_str_ + "/" + path);
  }

  FilePath GetDataDirectory() {
    return launcher_.GetDocumentRootPath();
  }

 protected:
  friend class base::RefCounted<BaseTestServer>;
  virtual ~BaseTestServer() { }

  bool Start(net::TestServerLauncher::Protocol protocol,
             const std::string& host_name, int port,
             const FilePath& document_root,
             const FilePath& cert_path,
             const std::wstring& file_root_url) {
    if (!launcher_.Start(protocol,
        host_name, port, document_root, cert_path, file_root_url))
      return false;

    if (protocol == net::TestServerLauncher::ProtoFTP)
      scheme_ = "ftp";
    else
      scheme_ = "http";
    if (!cert_path.empty())
      scheme_.push_back('s');

    host_name_ = host_name;
    port_str_ = IntToString(port);
    return true;
  }

  net::TestServerLauncher launcher_;
  std::string scheme_;
  std::string host_name_;
  std::string port_str_;
};

class HTTPTestServer : public BaseTestServer {
 protected:
  HTTPTestServer() {}

 public:
  // Creates and returns a new HTTPTestServer.
  static scoped_refptr<HTTPTestServer> CreateServer(
      const std::wstring& document_root) {
    return CreateServerWithFileRootURL(document_root, std::wstring());
  }

  static scoped_refptr<HTTPTestServer> CreateServerWithFileRootURL(
      const std::wstring& document_root,
      const std::wstring& file_root_url) {
    scoped_refptr<HTTPTestServer> test_server(new HTTPTestServer());
    FilePath no_cert;
    FilePath docroot = FilePath::FromWStringHack(document_root);
    if (!StartTestServer(test_server.get(), docroot, no_cert, file_root_url))
      return NULL;
    return test_server;
  }

  static bool StartTestServer(HTTPTestServer* server,
                              const FilePath& document_root,
                              const FilePath& cert_path,
                              const std::wstring& file_root_url) {
    return server->Start(net::TestServerLauncher::ProtoHTTP, kDefaultHostName,
                         kHTTPDefaultPort, document_root, cert_path,
                         file_root_url);
  }
};

class HTTPSTestServer : public HTTPTestServer {
 protected:
  HTTPSTestServer() {}

 public:
  // Create a server with a valid certificate
  // TODO(dkegel): HTTPSTestServer should not require an instance to specify
  // stock test certificates
  static scoped_refptr<HTTPSTestServer> CreateGoodServer(
      const std::wstring& document_root) {
    scoped_refptr<HTTPSTestServer> test_server = new HTTPSTestServer();
    FilePath docroot = FilePath::FromWStringHack(document_root);
    FilePath certpath = test_server->launcher_.GetOKCertPath();
    if (!test_server->Start(net::TestServerLauncher::ProtoHTTP,
        net::TestServerLauncher::kHostName,
        net::TestServerLauncher::kOKHTTPSPort,
        docroot, certpath, std::wstring())) {
      return NULL;
    }
    return test_server;
  }

  // Create a server with an up to date certificate for the wrong hostname
  // for this host
  static scoped_refptr<HTTPSTestServer> CreateMismatchedServer(
      const std::wstring& document_root) {
    scoped_refptr<HTTPSTestServer> test_server = new HTTPSTestServer();
    FilePath docroot = FilePath::FromWStringHack(document_root);
    FilePath certpath = test_server->launcher_.GetOKCertPath();
    if (!test_server->Start(net::TestServerLauncher::ProtoHTTP,
        net::TestServerLauncher::kMismatchedHostName,
        net::TestServerLauncher::kOKHTTPSPort,
        docroot, certpath, std::wstring())) {
      return NULL;
    }
    return test_server;
  }

  // Create a server with an expired certificate
  static scoped_refptr<HTTPSTestServer> CreateExpiredServer(
      const std::wstring& document_root) {
    scoped_refptr<HTTPSTestServer> test_server = new HTTPSTestServer();
    FilePath docroot = FilePath::FromWStringHack(document_root);
    FilePath certpath = test_server->launcher_.GetExpiredCertPath();
    if (!test_server->Start(net::TestServerLauncher::ProtoHTTP,
        net::TestServerLauncher::kHostName,
        net::TestServerLauncher::kBadHTTPSPort,
        docroot, certpath, std::wstring())) {
      return NULL;
    }
    return test_server;
  }

  // Create a server with an arbitrary certificate
  static scoped_refptr<HTTPSTestServer> CreateServer(
      const std::string& host_name, int port,
      const std::wstring& document_root,
      const std::wstring& cert_path) {
    scoped_refptr<HTTPSTestServer> test_server = new HTTPSTestServer();
    FilePath docroot = FilePath::FromWStringHack(document_root);
    FilePath certpath = FilePath::FromWStringHack(cert_path);
    if (!test_server->Start(net::TestServerLauncher::ProtoHTTP,
        host_name, port, docroot, certpath, std::wstring())) {
      return NULL;
    }
    return test_server;
  }

 protected:
  std::wstring cert_path_;

 private:
  virtual ~HTTPSTestServer() {}
};

class FTPTestServer : public BaseTestServer {
 public:
  FTPTestServer() {
  }

  static scoped_refptr<FTPTestServer> CreateServer(
      const std::wstring& document_root) {
    scoped_refptr<FTPTestServer> test_server = new FTPTestServer();
    FilePath docroot = FilePath::FromWStringHack(document_root);
    FilePath no_cert;
    if (!test_server->Start(net::TestServerLauncher::ProtoFTP,
        kDefaultHostName, kFTPDefaultPort, docroot, no_cert, std::wstring())) {
      return NULL;
    }
    return test_server;
  }

 private:
  ~FTPTestServer() {}
};


}  // namespace net

#endif  // NET_TEST_TEST_SERVER_H_
