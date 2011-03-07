// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_response_info.h"

#include "base/logging.h"
#include "base/pickle.h"
#include "base/time.h"
#include "net/base/auth.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/x509_certificate.h"
#include "net/http/http_response_headers.h"

using base::Time;

namespace net {

// These values can be bit-wise combined to form the flags field of the
// serialized HttpResponseInfo.
enum {
  // The version of the response info used when persisting response info.
  RESPONSE_INFO_VERSION = 1,

  // We reserve up to 8 bits for the version number.
  RESPONSE_INFO_VERSION_MASK = 0xFF,

  // This bit is set if the response info has a cert at the end.
  RESPONSE_INFO_HAS_CERT = 1 << 8,

  // This bit is set if the response info has a security-bits field (security
  // strength, in bits, of the SSL connection) at the end.
  RESPONSE_INFO_HAS_SECURITY_BITS = 1 << 9,

  // This bit is set if the response info has a cert status at the end.
  RESPONSE_INFO_HAS_CERT_STATUS = 1 << 10,

  // This bit is set if the response info has vary header data.
  RESPONSE_INFO_HAS_VARY_DATA = 1 << 11,

  // This bit is set if the request was cancelled before completion.
  RESPONSE_INFO_TRUNCATED = 1 << 12,

  // This bit is set if the response was received via SPDY.
  RESPONSE_INFO_WAS_SPDY = 1 << 13,

  // This bit is set if the request has NPN negotiated.
  RESPONSE_INFO_WAS_NPN = 1 << 14,

  // This bit is set if the request was fetched via an explicit proxy.
  RESPONSE_INFO_WAS_PROXY = 1 << 15,

  // This bit is set if response could use alternate protocol. However, browser
  // will ingore the alternate protocol if spdy is not enabled.
  RESPONSE_INFO_WAS_ALTERNATE_PROTOCOL_AVAILABLE = 1 << 16,

  // TODO(darin): Add other bits to indicate alternate request methods.
  // For now, we don't support storing those.
};

HttpResponseInfo::HttpResponseInfo()
    : was_cached(false),
      was_fetched_via_spdy(false),
      was_npn_negotiated(false),
      was_alternate_protocol_available(false),
      was_fetched_via_proxy(false) {
}

HttpResponseInfo::HttpResponseInfo(const HttpResponseInfo& rhs)
    : was_cached(rhs.was_cached),
      was_fetched_via_spdy(rhs.was_fetched_via_spdy),
      was_npn_negotiated(rhs.was_npn_negotiated),
      was_alternate_protocol_available(rhs.was_alternate_protocol_available),
      was_fetched_via_proxy(rhs.was_fetched_via_proxy),
      socket_address(rhs.socket_address),
      request_time(rhs.request_time),
      response_time(rhs.response_time),
      auth_challenge(rhs.auth_challenge),
      cert_request_info(rhs.cert_request_info),
      ssl_info(rhs.ssl_info),
      headers(rhs.headers),
      vary_data(rhs.vary_data),
      metadata(rhs.metadata) {
}

HttpResponseInfo::~HttpResponseInfo() {
}

HttpResponseInfo& HttpResponseInfo::operator=(const HttpResponseInfo& rhs) {
  was_cached = rhs.was_cached;
  was_fetched_via_spdy = rhs.was_fetched_via_spdy;
  was_npn_negotiated = rhs.was_npn_negotiated;
  was_alternate_protocol_available = rhs.was_alternate_protocol_available;
  was_fetched_via_proxy = rhs.was_fetched_via_proxy;
  socket_address = rhs.socket_address;
  request_time = rhs.request_time;
  response_time = rhs.response_time;
  auth_challenge = rhs.auth_challenge;
  cert_request_info = rhs.cert_request_info;
  ssl_info = rhs.ssl_info;
  headers = rhs.headers;
  vary_data = rhs.vary_data;
  metadata = rhs.metadata;
  return *this;
}

bool HttpResponseInfo::InitFromPickle(const Pickle& pickle,
                                      bool* response_truncated) {
  void* iter = NULL;

  // read flags and verify version
  int flags;
  if (!pickle.ReadInt(&iter, &flags))
    return false;
  int version = flags & RESPONSE_INFO_VERSION_MASK;
  if (version != RESPONSE_INFO_VERSION) {
    DLOG(ERROR) << "unexpected response info version: " << version;
    return false;
  }

  // read request-time
  int64 time_val;
  if (!pickle.ReadInt64(&iter, &time_val))
    return false;
  request_time = Time::FromInternalValue(time_val);
  was_cached = true;  // Set status to show cache resurrection.

  // read response-time
  if (!pickle.ReadInt64(&iter, &time_val))
    return false;
  response_time = Time::FromInternalValue(time_val);

  // read response-headers
  headers = new HttpResponseHeaders(pickle, &iter);
  DCHECK_NE(headers->response_code(), -1);

  // read ssl-info
  if (flags & RESPONSE_INFO_HAS_CERT) {
    ssl_info.cert =
        X509Certificate::CreateFromPickle(pickle, &iter);
  }
  if (flags & RESPONSE_INFO_HAS_CERT_STATUS) {
    int cert_status;
    if (!pickle.ReadInt(&iter, &cert_status))
      return false;
    ssl_info.cert_status = cert_status;
  }
  if (flags & RESPONSE_INFO_HAS_SECURITY_BITS) {
    int security_bits;
    if (!pickle.ReadInt(&iter, &security_bits))
      return false;
    ssl_info.security_bits = security_bits;
  }

  // read vary-data
  if (flags & RESPONSE_INFO_HAS_VARY_DATA) {
    if (!vary_data.InitFromPickle(pickle, &iter))
      return false;
  }

  // Read socket_address.  This was not always present in the response info,
  // so we don't fail if it can't be read.  If additional fields are added in
  // a future version, then they must only be read if this operation succeeds.
  std::string socket_address_host;
  if (pickle.ReadString(&iter, &socket_address_host)) {
    // If the host was written, we always expect the port to follow.
    uint16 socket_address_port;
    if (!pickle.ReadUInt16(&iter, &socket_address_port))
      return false;
    socket_address = HostPortPair(socket_address_host, socket_address_port);
  }

  was_fetched_via_spdy = (flags & RESPONSE_INFO_WAS_SPDY) != 0;

  was_npn_negotiated = (flags & RESPONSE_INFO_WAS_NPN) != 0;

  was_alternate_protocol_available =
      (flags & RESPONSE_INFO_WAS_ALTERNATE_PROTOCOL_AVAILABLE) != 0;

  was_fetched_via_proxy = (flags & RESPONSE_INFO_WAS_PROXY) != 0;

  *response_truncated = (flags & RESPONSE_INFO_TRUNCATED) ? true : false;

  return true;
}

void HttpResponseInfo::Persist(Pickle* pickle,
                               bool skip_transient_headers,
                               bool response_truncated) const {
  int flags = RESPONSE_INFO_VERSION;
  if (ssl_info.is_valid()) {
    flags |= RESPONSE_INFO_HAS_CERT;
    flags |= RESPONSE_INFO_HAS_CERT_STATUS;
    if (ssl_info.security_bits != -1)
      flags |= RESPONSE_INFO_HAS_SECURITY_BITS;
    // TODO(wtc): we should persist ssl_info.connection_status.
  }
  if (vary_data.is_valid())
    flags |= RESPONSE_INFO_HAS_VARY_DATA;
  if (response_truncated)
    flags |= RESPONSE_INFO_TRUNCATED;
  if (was_fetched_via_spdy)
    flags |= RESPONSE_INFO_WAS_SPDY;
  if (was_npn_negotiated)
    flags |= RESPONSE_INFO_WAS_NPN;
  if (was_alternate_protocol_available)
    flags |= RESPONSE_INFO_WAS_ALTERNATE_PROTOCOL_AVAILABLE;
  if (was_fetched_via_proxy)
    flags |= RESPONSE_INFO_WAS_PROXY;

  pickle->WriteInt(flags);
  pickle->WriteInt64(request_time.ToInternalValue());
  pickle->WriteInt64(response_time.ToInternalValue());

  net::HttpResponseHeaders::PersistOptions persist_options =
      net::HttpResponseHeaders::PERSIST_RAW;

  if (skip_transient_headers) {
    persist_options =
        net::HttpResponseHeaders::PERSIST_SANS_COOKIES |
        net::HttpResponseHeaders::PERSIST_SANS_CHALLENGES |
        net::HttpResponseHeaders::PERSIST_SANS_HOP_BY_HOP |
        net::HttpResponseHeaders::PERSIST_SANS_NON_CACHEABLE |
        net::HttpResponseHeaders::PERSIST_SANS_RANGES;
  }

  headers->Persist(pickle, persist_options);

  if (ssl_info.is_valid()) {
    ssl_info.cert->Persist(pickle);
    pickle->WriteInt(ssl_info.cert_status);
    if (ssl_info.security_bits != -1)
      pickle->WriteInt(ssl_info.security_bits);
  }

  if (vary_data.is_valid())
    vary_data.Persist(pickle);

  pickle->WriteString(socket_address.host());
  pickle->WriteUInt16(socket_address.port());
}

}  // namespace net
