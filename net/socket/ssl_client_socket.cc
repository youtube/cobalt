// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket.h"

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
