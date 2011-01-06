// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_OCSP_NSS_OCSP_H_
#define NET_OCSP_NSS_OCSP_H_
#pragma once

namespace net {

class URLRequestContext;

// Sets the MessageLoop for OCSP to the current message loop.
// This should be called before EnsureOCSPInit() if you want to
// control the message loop for OCSP.
void SetMessageLoopForOCSP();

// Initializes OCSP handlers for NSS.  This must be called before any
// certificate verification functions.  This function is thread-safe, and OCSP
// handlers will only ever be initialized once.  ShutdownOCSP() must be called
// on shutdown.
void EnsureOCSPInit();

// This should be called once on shutdown to stop issuing URLRequests for OCSP.
void ShutdownOCSP();

// Set URLRequestContext for OCSP handlers.
void SetURLRequestContextForOCSP(URLRequestContext* request_context);
URLRequestContext* GetURLRequestContextForOCSP();

}  // namespace net

#endif  // NET_OCSP_NSS_OCSP_H_
