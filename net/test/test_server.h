// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_TEST_SERVER_H_
#define NET_TEST_TEST_SERVER_H_
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "build/build_config.h"

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/process_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_util.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

class CommandLine;
class DictionaryValue;
class GURL;

namespace net {

class AddressList;

// This object bounds the lifetime of an external python-based HTTP/FTP server
// that can provide various responses useful for testing.
class TestServer {
 public:
  typedef std::pair<std::string, std::string> StringPair;

  enum Type {
    TYPE_FTP,
    TYPE_HTTP,
    TYPE_HTTPS,
    TYPE_SYNC,
  };

  // Container for various options to control how the HTTPS server is
  // initialized.
  struct HTTPSOptions {
    enum ServerCertificate {
      CERT_OK,
      CERT_MISMATCHED_NAME,
      CERT_EXPIRED,
    };

    // Bitmask of bulk encryption algorithms that the test server supports
    // and that can be selectively enabled or disabled.
    enum BulkCipher {
      // Special value used to indicate that any algorithm the server supports
      // is acceptable. Preferred over explicitly OR-ing all ciphers.
      BULK_CIPHER_ANY    = 0,

      BULK_CIPHER_RC4    = (1 << 0),
      BULK_CIPHER_AES128 = (1 << 1),
      BULK_CIPHER_AES256 = (1 << 2),

      // NOTE: 3DES support in the Python test server has external
      // dependencies and not be available on all machines. Clients may not
      // be able to connect if only 3DES is specified.
      BULK_CIPHER_3DES   = (1 << 3),
    };

    // Initialize a new HTTPSOptions using CERT_OK as the certificate.
    HTTPSOptions();

    // Initialize a new HTTPSOptions that will use the specified certificate.
    explicit HTTPSOptions(ServerCertificate cert);
    ~HTTPSOptions();

    // Returns the relative filename of the file that contains the
    // |server_certificate|.
    FilePath GetCertificateFile() const;

    // The certificate to use when serving requests.
    ServerCertificate server_certificate;

    // True if a CertificateRequest should be sent to the client during
    // handshaking.
    bool request_client_certificate;

    // If |request_client_certificate| is true, an optional list of files,
    // each containing a single, PEM-encoded X.509 certificates. The subject
    // from each certificate will be added to the certificate_authorities
    // field of the CertificateRequest.
    std::vector<FilePath> client_authorities;

    // A bitwise-OR of BulkCipher that should be used by the
    // HTTPS server, or BULK_CIPHER_ANY to indicate that all implemented
    // ciphers are acceptable.
    int bulk_ciphers;
  };

  TestServer(Type type, const FilePath& document_root);

  // Initialize a HTTPS TestServer with a specific set of HTTPSOptions.
  TestServer(const HTTPSOptions& https_options,
             const FilePath& document_root);

  ~TestServer();

  bool Start() WARN_UNUSED_RESULT;

  // Stop the server started by Start().
  bool Stop();

  const FilePath& document_root() const { return document_root_; }
  const HostPortPair& host_port_pair() const;
  const DictionaryValue& server_data() const;
  std::string GetScheme() const;
  bool GetAddressList(AddressList* address_list) const WARN_UNUSED_RESULT;

  GURL GetURL(const std::string& path) const;

  GURL GetURLWithUser(const std::string& path,
                      const std::string& user) const;

  GURL GetURLWithUserAndPassword(const std::string& path,
                                 const std::string& user,
                                 const std::string& password) const;

  static bool GetFilePathWithReplacements(
      const std::string& original_path,
      const std::vector<StringPair>& text_to_replace,
      std::string* replacement_path);

 private:
  void Init(const FilePath& document_root);

  // Modify PYTHONPATH to contain libraries we need.
  bool SetPythonPath() WARN_UNUSED_RESULT;

  // Launches the Python test server. Returns true on success.
  bool LaunchPython(const FilePath& testserver_path) WARN_UNUSED_RESULT;

  // Waits for the server to start. Returns true on success.
  bool WaitToStart() WARN_UNUSED_RESULT;

  // Parses the server data read from the test server.  Returns true
  // on success.
  bool ParseServerData(const std::string& server_data) WARN_UNUSED_RESULT;

  // Returns path to the root certificate.
  FilePath GetRootCertificatePath();

  // Load the test root cert, if it hasn't been loaded yet.
  bool LoadTestRootCert() WARN_UNUSED_RESULT;

  // Add the command line arguments for the Python test server to
  // |command_line|. Return true on success.
  bool AddCommandLineArguments(CommandLine* command_line) const;

  // Document root of the test server.
  FilePath document_root_;

  // Directory that contains the SSL certificates.
  FilePath certificates_dir_;

  // Address the test server listens on.
  HostPortPair host_port_pair_;

  // Holds the data sent from the server (e.g., port number).
  scoped_ptr<DictionaryValue> server_data_;

  // Handle of the Python process running the test server.
  base::ProcessHandle process_handle_;

  scoped_ptr<net::ScopedPortException> allowed_port_;

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

  // If |type_| is TYPE_HTTPS, the TLS settings to use for the test server.
  HTTPSOptions https_options_;

  Type type_;

  // Has the server been started?
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(TestServer);
};

}  // namespace net

#endif  // NET_TEST_TEST_SERVER_H_
