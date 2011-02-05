// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is only inclued in ssl_client_socket_nss.cc and
// ssl_server_socket_nss.cc to share common functions of NSS.

#ifndef NET_SOCKET_NSS_SSL_UTIL_H_
#define NET_SOCKET_NSS_SSL_UTIL_H_

#include <prerror.h>

namespace net {

class BoundNetLog;

// Initalize NSS SSL library.
void EnsureNSSSSLInit();

// Log a failed NSS funcion call.
void LogFailedNSSFunction(const BoundNetLog& net_log,
                          const char* function,
                          const char* param);

// Map network error code to NSS error code.
PRErrorCode MapErrorToNSS(int result);

// Map NSS error code to network error code.
int MapNSSError(PRErrorCode err);

// Map NSS error code from the first SSL handshake to network error code.
int MapNSSHandshakeError(PRErrorCode err);

}  // namespace net

#endif  // NET_SOCKET_NSS_SSL_UTIL_H_
