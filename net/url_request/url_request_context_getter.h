// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_URL_REQUEST_CONTEXT_GETTER_H_
#define CHROME_COMMON_NET_URL_REQUEST_CONTEXT_GETTER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/message_loop_helpers.h"
#include "net/base/net_export.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class CookieStore;
class URLRequestContext;

struct URLRequestContextGetterTraits;

// Interface for retrieving an net::URLRequestContext.
class NET_EXPORT URLRequestContextGetter
    : public base::RefCountedThreadSafe<URLRequestContextGetter,
                                        URLRequestContextGetterTraits> {
 public:
  virtual URLRequestContext* GetURLRequestContext() = 0;

  // Returns a SingleThreadTaskRunner corresponding to the thread on
  // which the network IO happens (the thread on which the returned
  // net::URLRequestContext may be used).
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<URLRequestContextGetter,
                                          URLRequestContextGetterTraits>;
  friend class base::DeleteHelper<URLRequestContextGetter>;
  friend struct URLRequestContextGetterTraits;

  URLRequestContextGetter();
  virtual ~URLRequestContextGetter();

 private:
  // OnDestruct is meant to ensure deletion on the thread on which the request
  // IO happens.
  void OnDestruct() const;
};

struct URLRequestContextGetterTraits {
  static void Destruct(const URLRequestContextGetter* context_getter) {
    context_getter->OnDestruct();
  }
};

}  // namespace net

#endif  // CHROME_COMMON_NET_URL_REQUEST_CONTEXT_GETTER_H_
