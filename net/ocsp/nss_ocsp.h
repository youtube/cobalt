// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_OCSP_NSS_OCSP_H_
#define NET_OCSP_NSS_OCSP_H_

class URLRequestContext;

namespace net {

// Initializes OCSP handlers for NSS.  This must be called before any
// certificate verification functions.  This function is thread-safe, and OCSP
// handlers will only ever be initialized once.
void EnsureOCSPInit();

// Set URLRequestContext for OCSP handlers.
void SetURLRequestContextForOCSP(URLRequestContext* request_context);
URLRequestContext* GetURLRequestContextForOCSP();

}  // namespace net

#endif  // NET_OCSP_NSS_OCSP_H_
