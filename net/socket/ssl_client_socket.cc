// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket.h"

#include "base/string_util.h"

namespace net {

SSLClientSocket::SSLClientSocket()
    : was_npn_negotiated_(false),
      was_spdy_negotiated_(false),
      protocol_negotiated_(kProtoUnknown),
      domain_bound_cert_type_(CLIENT_CERT_INVALID_TYPE) {
}

// static
NextProto SSLClientSocket::NextProtoFromString(
    const std::string& proto_string) {
  if (proto_string == "http1.1" || proto_string == "http/1.1") {
    return kProtoHTTP11;
  } else if (proto_string == "spdy/1") {
    return kProtoSPDY1;
  } else if (proto_string == "spdy/2") {
    return kProtoSPDY2;
  } else if (proto_string == "spdy/3") {
    return kProtoSPDY3;
  } else {
    return kProtoUnknown;
  }
}

// static
const char* SSLClientSocket::NextProtoToString(NextProto next_proto) {
  switch (next_proto) {
    case kProtoHTTP11:
      return "http/1.1";
    case kProtoSPDY1:
      return "spdy/1";
    case kProtoSPDY2:
      return "spdy/2";
    case kProtoSPDY3:
      return "spdy/3";
    default:
      break;
  }
  return "unknown";
}

// static
const char* SSLClientSocket::NextProtoStatusToString(
    const SSLClientSocket::NextProtoStatus status) {
  switch (status) {
    case kNextProtoUnsupported:
      return "unsupported";
    case kNextProtoNegotiated:
      return "negotiated";
    case kNextProtoNoOverlap:
      return "no-overlap";
  }
  return NULL;
}

// static
std::string SSLClientSocket::ServerProtosToString(
    const std::string& server_protos) {
  const char* protos = server_protos.c_str();
  size_t protos_len = server_protos.length();
  std::vector<std::string> server_protos_with_commas;
  for (size_t i = 0; i < protos_len; ) {
    const size_t len = protos[i];
    std::string proto_str(&protos[i + 1], len);
    server_protos_with_commas.push_back(proto_str);
    i += len + 1;
  }
  return JoinString(server_protos_with_commas, ',');
}

NextProto SSLClientSocket::GetNegotiatedProtocol() const {
  return protocol_negotiated_;
}

bool SSLClientSocket::IgnoreCertError(int error, int load_flags) {
  if (error == OK || load_flags & LOAD_IGNORE_ALL_CERT_ERRORS)
    return true;

  if (error == ERR_CERT_COMMON_NAME_INVALID &&
      (load_flags & LOAD_IGNORE_CERT_COMMON_NAME_INVALID))
    return true;

  if (error == ERR_CERT_DATE_INVALID &&
      (load_flags & LOAD_IGNORE_CERT_DATE_INVALID))
    return true;

  if (error == ERR_CERT_AUTHORITY_INVALID &&
      (load_flags & LOAD_IGNORE_CERT_AUTHORITY_INVALID))
    return true;

  return false;
}

bool SSLClientSocket::was_npn_negotiated() const {
  return was_npn_negotiated_;
}

bool SSLClientSocket::set_was_npn_negotiated(bool negotiated) {
  return was_npn_negotiated_ = negotiated;
}

bool SSLClientSocket::was_spdy_negotiated() const {
  return was_spdy_negotiated_;
}

bool SSLClientSocket::set_was_spdy_negotiated(bool negotiated) {
  return was_spdy_negotiated_ = negotiated;
}

void SSLClientSocket::set_protocol_negotiated(NextProto protocol_negotiated) {
  protocol_negotiated_ = protocol_negotiated;
}

bool SSLClientSocket::WasDomainBoundCertSent() const {
  return domain_bound_cert_type_ != CLIENT_CERT_INVALID_TYPE;
}

SSLClientCertType SSLClientSocket::domain_bound_cert_type() const {
  return domain_bound_cert_type_;
}

SSLClientCertType SSLClientSocket::set_domain_bound_cert_type(
    SSLClientCertType type) {
  return domain_bound_cert_type_ = type;
}

}  // namespace net
