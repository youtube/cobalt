// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_RESPONSE_INFO_H_
#define NET_HTTP_HTTP_RESPONSE_INFO_H_

#include "base/time.h"
#include "net/base/auth.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/base/ssl_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_vary_data.h"

class Pickle;

namespace net {

class HttpResponseInfo {
 public:
  HttpResponseInfo();
  ~HttpResponseInfo();
  // Default copy-constructor and assignment operator are OK!

  // The following is only defined if the request_time member is set.
  // If this response was resurrected from cache, then this bool is set, and
  // request_time may corresponds to a time "far" in the past.  Note that
  // stale content (perhaps un-cacheable) may be fetched from cache subject to
  // the load flags specified on the request info.  For example, this is done
  // when a user presses the back button to re-render pages, or at startup, when
  // reloading previously visited pages (without going over the network).
  bool was_cached;

  // True if the request was fetched over a SPDY channel.
  bool was_fetched_via_spdy;

  // The time at which the request was made that resulted in this response.
  // For cached responses, this is the last time the cache entry was validated.
  base::Time request_time;

  // The time at which the response headers were received.  For cached
  // this is the last time the cache entry was validated.
  base::Time response_time;

  // If the response headers indicate a 401 or 407 failure, then this structure
  // will contain additional information about the authentication challenge.
  scoped_refptr<AuthChallengeInfo> auth_challenge;

  // The SSL client certificate request info.
  // TODO(wtc): does this really belong in HttpResponseInfo?  I put it here
  // because it is similar to |auth_challenge|, but unlike HTTP authentication
  // challenge, client certificate request is not part of an HTTP response.
  scoped_refptr<SSLCertRequestInfo> cert_request_info;

  // The SSL connection info (if HTTPS).
  SSLInfo ssl_info;

  // The parsed response headers and status line.
  scoped_refptr<HttpResponseHeaders> headers;

  // The "Vary" header data for this response.
  HttpVaryData vary_data;

  // Initializes from the representation stored in the given pickle.
  bool InitFromPickle(const Pickle& pickle, bool* response_truncated);

  // Call this method to persist the response info.
  void Persist(Pickle* pickle,
               bool skip_transient_headers,
               bool response_truncated) const;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_RESPONSE_INFO_H_
