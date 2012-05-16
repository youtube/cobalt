// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/base_test_server.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_root_certs.h"

namespace net {

namespace {

std::string GetHostname(BaseTestServer::Type type,
                        const BaseTestServer::HTTPSOptions& options) {
  if (type == BaseTestServer::TYPE_HTTPS &&
      options.server_certificate ==
          BaseTestServer::HTTPSOptions::CERT_MISMATCHED_NAME) {
    // Return a different hostname string that resolves to the same hostname.
    return "localhost";
  }

  // Use the 127.0.0.1 as default.
  return BaseTestServer::kLocalhost;
}

void GetCiphersList(int cipher, base::ListValue* values) {
  if (cipher & BaseTestServer::HTTPSOptions::BULK_CIPHER_RC4)
    values->Append(base::Value::CreateStringValue("rc4"));
  if (cipher & BaseTestServer::HTTPSOptions::BULK_CIPHER_AES128)
    values->Append(base::Value::CreateStringValue("aes128"));
  if (cipher & BaseTestServer::HTTPSOptions::BULK_CIPHER_AES256)
    values->Append(base::Value::CreateStringValue("aes256"));
  if (cipher & BaseTestServer::HTTPSOptions::BULK_CIPHER_3DES)
    values->Append(base::Value::CreateStringValue("3des"));
}

}  // namespace

BaseTestServer::HTTPSOptions::HTTPSOptions()
    : server_certificate(CERT_OK),
      ocsp_status(OCSP_OK),
      request_client_certificate(false),
      bulk_ciphers(HTTPSOptions::BULK_CIPHER_ANY),
      record_resume(false),
      tls_intolerant(false) {}

BaseTestServer::HTTPSOptions::HTTPSOptions(
    BaseTestServer::HTTPSOptions::ServerCertificate cert)
    : server_certificate(cert),
      request_client_certificate(false),
      bulk_ciphers(HTTPSOptions::BULK_CIPHER_ANY),
      record_resume(false),
      tls_intolerant(false) {}

BaseTestServer::HTTPSOptions::~HTTPSOptions() {}

FilePath BaseTestServer::HTTPSOptions::GetCertificateFile() const {
  switch (server_certificate) {
    case CERT_OK:
    case CERT_MISMATCHED_NAME:
      return FilePath(FILE_PATH_LITERAL("ok_cert.pem"));
    case CERT_EXPIRED:
      return FilePath(FILE_PATH_LITERAL("expired_cert.pem"));
    case CERT_CHAIN_WRONG_ROOT:
      // This chain uses its own dedicated test root certificate to avoid
      // side-effects that may affect testing.
      return FilePath(FILE_PATH_LITERAL("redundant-server-chain.pem"));
    case CERT_AUTO:
      return FilePath();
    default:
      NOTREACHED();
  }
  return FilePath();
}

std::string BaseTestServer::HTTPSOptions::GetOCSPArgument() const {
  if (server_certificate != CERT_AUTO)
    return "";

  switch (ocsp_status) {
    case OCSP_OK:
      return "ok";
    case OCSP_REVOKED:
      return "revoked";
    case OCSP_INVALID:
      return "invalid";
    default:
      NOTREACHED();
      return "";
  }
}

const char BaseTestServer::kLocalhost[] = "127.0.0.1";
const char BaseTestServer::kGDataAuthToken[] = "testtoken";

BaseTestServer::BaseTestServer(Type type, const std::string& host)
    : type_(type),
      started_(false),
      log_to_console_(false) {
  Init(host);
}

BaseTestServer::BaseTestServer(const HTTPSOptions& https_options)
    : https_options_(https_options),
      type_(TYPE_HTTPS),
      started_(false),
      log_to_console_(false) {
  Init(GetHostname(TYPE_HTTPS, https_options));
}

BaseTestServer::~BaseTestServer() {}

const HostPortPair& BaseTestServer::host_port_pair() const {
  DCHECK(started_);
  return host_port_pair_;
}

const DictionaryValue& BaseTestServer::server_data() const {
  DCHECK(started_);
  DCHECK(server_data_.get());
  return *server_data_;
}

std::string BaseTestServer::GetScheme() const {
  switch (type_) {
    case TYPE_FTP:
      return "ftp";
    case TYPE_GDATA:
    case TYPE_HTTP:
    case TYPE_SYNC:
      return "http";
    case TYPE_HTTPS:
      return "https";
    case TYPE_TCP_ECHO:
    case TYPE_UDP_ECHO:
    default:
      NOTREACHED();
  }
  return std::string();
}

bool BaseTestServer::GetAddressList(AddressList* address_list) const {
  DCHECK(address_list);

  scoped_ptr<HostResolver> resolver(
      CreateSystemHostResolver(HostResolver::kDefaultParallelism,
                               HostResolver::kDefaultRetryAttempts,
                               NULL));
  HostResolver::RequestInfo info(host_port_pair_);
  TestCompletionCallback callback;
  int rv = resolver->Resolve(info, address_list, callback.callback(), NULL,
                             BoundNetLog());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  if (rv != net::OK) {
    LOG(ERROR) << "Failed to resolve hostname: " << host_port_pair_.host();
    return false;
  }
  return true;
}

uint16 BaseTestServer::GetPort() {
  return host_port_pair_.port();
}

void BaseTestServer::SetPort(uint16 port) {
  host_port_pair_.set_port(port);
}

GURL BaseTestServer::GetURL(const std::string& path) const {
  return GURL(GetScheme() + "://" + host_port_pair_.ToString() + "/" + path);
}

GURL BaseTestServer::GetURLWithUser(const std::string& path,
                                const std::string& user) const {
  return GURL(GetScheme() + "://" + user + "@" + host_port_pair_.ToString() +
              "/" + path);
}

GURL BaseTestServer::GetURLWithUserAndPassword(const std::string& path,
                                           const std::string& user,
                                           const std::string& password) const {
  return GURL(GetScheme() + "://" + user + ":" + password + "@" +
              host_port_pair_.ToString() + "/" + path);
}

// static
bool BaseTestServer::GetFilePathWithReplacements(
    const std::string& original_file_path,
    const std::vector<StringPair>& text_to_replace,
    std::string* replacement_path) {
  std::string new_file_path = original_file_path;
  bool first_query_parameter = true;
  const std::vector<StringPair>::const_iterator end = text_to_replace.end();
  for (std::vector<StringPair>::const_iterator it = text_to_replace.begin();
       it != end;
       ++it) {
    const std::string& old_text = it->first;
    const std::string& new_text = it->second;
    std::string base64_old;
    std::string base64_new;
    if (!base::Base64Encode(old_text, &base64_old))
      return false;
    if (!base::Base64Encode(new_text, &base64_new))
      return false;
    if (first_query_parameter) {
      new_file_path += "?";
      first_query_parameter = false;
    } else {
      new_file_path += "&";
    }
    new_file_path += "replace_text=";
    new_file_path += base64_old;
    new_file_path += ":";
    new_file_path += base64_new;
  }

  *replacement_path = new_file_path;
  return true;
}

void BaseTestServer::Init(const std::string& host) {
  host_port_pair_ = HostPortPair(host, 0);

  // TODO(battre) Remove this after figuring out why the TestServer is flaky.
  // http://crbug.com/96594
  log_to_console_ = true;
}

void BaseTestServer::SetResourcePath(const FilePath& document_root,
                                     const FilePath& certificates_dir) {
  // This method shouldn't get called twice.
  DCHECK(certificates_dir_.empty());
  document_root_ = document_root;
  certificates_dir_ = certificates_dir;
  DCHECK(!certificates_dir_.empty());
}

bool BaseTestServer::ParseServerData(const std::string& server_data) {
  VLOG(1) << "Server data: " << server_data;
  base::JSONReader json_reader;
  scoped_ptr<Value> value(json_reader.ReadToValue(server_data));
  if (!value.get() || !value->IsType(Value::TYPE_DICTIONARY)) {
    LOG(ERROR) << "Could not parse server data: "
               << json_reader.GetErrorMessage();
    return false;
  }

  server_data_.reset(static_cast<DictionaryValue*>(value.release()));
  int port = 0;
  if (!server_data_->GetInteger("port", &port)) {
    LOG(ERROR) << "Could not find port value";
    return false;
  }
  if ((port <= 0) || (port > kuint16max)) {
    LOG(ERROR) << "Invalid port value: " << port;
    return false;
  }
  host_port_pair_.set_port(port);

  return true;
}

bool BaseTestServer::LoadTestRootCert() const {
  TestRootCerts* root_certs = TestRootCerts::GetInstance();
  if (!root_certs)
    return false;

  // Should always use absolute path to load the root certificate.
  FilePath root_certificate_path = certificates_dir_;
  if (!certificates_dir_.IsAbsolute()) {
    FilePath src_dir;
    if (!PathService::Get(base::DIR_SOURCE_ROOT, &src_dir))
      return false;
    root_certificate_path = src_dir.Append(certificates_dir_);
  }

  return root_certs->AddFromFile(
      root_certificate_path.AppendASCII("root_ca_cert.crt"));
}

bool BaseTestServer::SetupWhenServerStarted() {
  DCHECK(host_port_pair_.port());

  if (type_ == TYPE_HTTPS && !LoadTestRootCert())
      return false;

  started_ = true;
  allowed_port_.reset(new ScopedPortException(host_port_pair_.port()));
  return true;
}

void BaseTestServer::CleanUpWhenStoppingServer() {
  TestRootCerts* root_certs = TestRootCerts::GetInstance();
  root_certs->Clear();

  host_port_pair_.set_port(0);
  allowed_port_.reset();
  started_ = false;
}

// Generates a dictionary of arguments to pass to the Python test server via
// the test server spawner, in the form of
// { argument-name: argument-value, ... }
// Returns false if an invalid configuration is specified.
bool BaseTestServer::GenerateArguments(base::DictionaryValue* arguments) const {
  DCHECK(arguments);

  arguments->SetString("host", host_port_pair_.host());
  arguments->SetInteger("port", host_port_pair_.port());
  arguments->SetString("data-dir", document_root_.value());

  if (VLOG_IS_ON(1) || log_to_console_)
    arguments->Set("log-to-console", base::Value::CreateNullValue());

  if (type_ == TYPE_HTTPS) {
    arguments->Set("https", base::Value::CreateNullValue());

    // Check the certificate arguments of the HTTPS server.
    FilePath certificate_path(certificates_dir_);
    FilePath certificate_file(https_options_.GetCertificateFile());
    if (!certificate_file.value().empty()) {
      certificate_path = certificate_path.Append(certificate_file);
      if (certificate_path.IsAbsolute() &&
          !file_util::PathExists(certificate_path)) {
        LOG(ERROR) << "Certificate path " << certificate_path.value()
                   << " doesn't exist. Can't launch https server.";
        return false;
      }
      arguments->SetString("cert-and-key-file", certificate_path.value());
    }

    std::string ocsp_arg = https_options_.GetOCSPArgument();
    if (!ocsp_arg.empty())
      arguments->SetString("ocsp", ocsp_arg);

    // Check the client certificate related arguments.
    if (https_options_.request_client_certificate)
      arguments->Set("ssl-client-auth", base::Value::CreateNullValue());
    scoped_ptr<base::ListValue> ssl_client_certs(new base::ListValue());

    std::vector<FilePath>::const_iterator it;
    for (it = https_options_.client_authorities.begin();
         it != https_options_.client_authorities.end(); ++it) {
      if (it->IsAbsolute() && !file_util::PathExists(*it)) {
        LOG(ERROR) << "Client authority path " << it->value()
                   << " doesn't exist. Can't launch https server.";
        return false;
      }
      ssl_client_certs->Append(base::Value::CreateStringValue(it->value()));
    }

    if (ssl_client_certs->GetSize())
      arguments->Set("ssl-client-ca", ssl_client_certs.release());

    // Check bulk cipher argument.
    scoped_ptr<base::ListValue> bulk_cipher_values(new base::ListValue());
    GetCiphersList(https_options_.bulk_ciphers, bulk_cipher_values.get());
    if (bulk_cipher_values->GetSize())
      arguments->Set("ssl-bulk-cipher", bulk_cipher_values.release());
    if (https_options_.record_resume)
      arguments->Set("https-record-resume", base::Value::CreateNullValue());
    if (https_options_.tls_intolerant)
      arguments->Set("tls-intolerant", base::Value::CreateNullValue());
  }
  return true;
}

}  // namespace net
