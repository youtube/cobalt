// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CONNECTION_STATUS_FLAGS_H_
#define NET_BASE_SSL_CONNECTION_STATUS_FLAGS_H_

namespace net {

// Status flags for SSLInfo::connection_status.
enum {
  // The lower 16 bits are reserved for the TLS ciphersuite id.
  SSL_CONNECTION_CIPHERSUITE_SHIFT = 0,
  SSL_CONNECTION_CIPHERSUITE_MASK = 0xffff,

  // The next two bits are reserved for the compression used.
  SSL_CONNECTION_COMPRESSION_SHIFT = 16,
  SSL_CONNECTION_COMPRESSION_MASK = 3,

  // We fell back to SSLv3 for this connection.
  SSL_CONNECTION_SSL3_FALLBACK = 1 << 18,
  // The server doesn't support the renegotiation_info extension.
  SSL_CONNECTION_NO_RENEGOTIATION_EXTENSION = 1 << 19,

  // 1 << 31 (the sign bit) is reserved so that the SSL connection status will
  // never be negative.
};

}  // namespace net

#endif  // NET_BASE_SSL_CONNECTION_STATUS_FLAGS_H_
