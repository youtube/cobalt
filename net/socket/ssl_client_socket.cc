// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket.h"

#include "base/string_util.h"

namespace net {

SSLClientSocket::SSLClientSocket()
    : was_npn_negotiated_(false),
      was_spdy_negotiated_(false) {
}

SSLClientSocket::NextProto SSLClientSocket::NextProtoFromString(
    const std::string& proto_string) {
  if (proto_string == "http1.1" || proto_string == "http/1.1") {
    return kProtoHTTP11;
  } else if (proto_string == "spdy/1") {
    return kProtoSPDY1;
  } else if (proto_string == "spdy/2") {
    return kProtoSPDY2;
  } else {
    return kProtoUnknown;
  }
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

}  // namespace net
