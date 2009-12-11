// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_CLIENT_SOCKET_H_
#define NET_SOCKET_SSL_CLIENT_SOCKET_H_

#include "net/socket/client_socket.h"

namespace net {

class SSLCertRequestInfo;
class SSLInfo;

// A client socket that uses SSL as the transport layer.
//
// NOTE: The SSL handshake occurs within the Connect method after a TCP
// connection is established.  If a SSL error occurs during the handshake,
// Connect will fail.
//
class SSLClientSocket : public ClientSocket {
 public:
  // Next Protocol Negotiation (NPN) allows a TLS client and server to come to
  // an agreement about the application level protocol to speak over a
  // connection.
  enum NextProtoStatus {
    kNextProtoUnsupported = 0,  // The server doesn't support NPN.
    kNextProtoNegotiated = 1,   // We agreed on a protocol.
    kNextProtoNoOverlap = 2,    // No protocols in common. We requested
                                // the first protocol in our list.
  };

  // Next Protocol Negotiation (NPN), if successful, results in agreement on an
  // application-level string that specifies the application level protocol to
  // use over the TLS connection. NextProto enumerates the application level
  // protocols that we recognise.
  enum NextProto {
    kProtoUnknown = 0,
    kProtoHTTP11 = 1,
    kProtoSPDY = 2,
  };

  // Gets the SSL connection information of the socket.
  virtual void GetSSLInfo(SSLInfo* ssl_info) = 0;

  // Gets the SSL CertificateRequest info of the socket after Connect failed
  // with ERR_SSL_CLIENT_AUTH_CERT_NEEDED.
  virtual void GetSSLCertRequestInfo(
      SSLCertRequestInfo* cert_request_info) = 0;

  // Get the application level protocol that we negotiated with the server.
  // *proto is set to the resulting protocol (n.b. that the string may have
  // embedded NULs).
  //   kNextProtoUnsupported: *proto is cleared.
  //   kNextProtoNegotiated:  *proto is set to the negotiated protocol.
  //   kNextProtoNoOverlap:   *proto is set to the first protocol in the
  //                          supported list.
  virtual NextProtoStatus GetNextProto(std::string* proto) = 0;

  static NextProto NextProtoFromString(const std::string& proto_string) {
    if (proto_string == "http1.1") {
      return kProtoHTTP11;
    } else if (proto_string == "spdy") {
      return kProtoSPDY;
    } else {
      return kProtoUnknown;
    }
  }
};

}  // namespace net

#endif  // NET_SOCKET_SSL_CLIENT_SOCKET_H_
