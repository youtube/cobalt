// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ProxyScriptFetcher is an async interface for fetching a proxy auto config
// script. It is specific to fetching a PAC script; enforces timeout, max-size,
// status code.

#ifndef NET_PROXY_PROXY_SCRIPT_FETCHER_H_
#define NET_PROXY_PROXY_SCRIPT_FETCHER_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "net/base/completion_callback.h"

class GURL;
class URLRequestContext;

namespace net {

class ProxyScriptFetcher {
 public:
  // Destruction should cancel any outstanding requests.
  virtual ~ProxyScriptFetcher() {}

  // Downloads the given PAC URL, and invokes |callback| on completion.
  // On success |callback| is executed with a result code of OK, |*utf16_text|
  // is filled with the response. On failure, the result text is
  // an empty string, and the result code is a network error. Some special
  // network errors that may occur are:
  //
  //    ERR_TIMED_OUT         -- the fetch took too long to complete.
  //    ERR_FILE_TOO_BIG      -- the response's body was too large.
  //    ERR_PAC_STATUS_NOT_OK -- non-200 HTTP status code.
  //    ERR_NOT_IMPLEMENTED   -- the response required authentication.
  //
  // If the request is cancelled (either using the "Cancel()" method or by
  // deleting |this|), then no callback is invoked.
  //
  // Only one fetch is allowed to be outstanding at a time.
  virtual int Fetch(const GURL& url, string16* utf16_text,
                    CompletionCallback* callback) = 0;

  // Aborts the in-progress fetch (if any).
  virtual void Cancel() = 0;

  // Returns the request context that this fetcher uses to issue downloads,
  // or NULL.
  virtual URLRequestContext* GetRequestContext() { return NULL; }

  // Create a ProxyScriptFetcher that uses |url_request_context|.
  static ProxyScriptFetcher* Create(URLRequestContext* url_request_context);

  // --------------------------------------------------------------------------
  // Testing helpers (only available to unit-tests).
  // --------------------------------------------------------------------------
 private:
  FRIEND_TEST_ALL_PREFIXES(ProxyScriptFetcherTest, Hang);
  FRIEND_TEST_ALL_PREFIXES(ProxyScriptFetcherTest, TooLarge);

  // Sets the maximum duration for a fetch to |timeout_ms|. Returns the previous
  // bound.
  static int SetTimeoutConstraintForUnittest(int timeout_ms);

  // Sets the maximum response size for a fetch to |size_bytes|. Returns the
  // previous bound.
  static size_t SetSizeConstraintForUnittest(size_t size_bytes);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_SCRIPT_FETCHER_H_
