// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_URL_REQUEST_CONTEXT_GETTER_H_
#define CHROME_COMMON_NET_URL_REQUEST_CONTEXT_GETTER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/task.h"

namespace base {
class MessageLoopProxy;
}

namespace net {
class CookieStore;
class URLRequestContext;

struct URLRequestContextGetterTraits;

// Interface for retrieving an net::URLRequestContext.
class URLRequestContextGetter
    : public base::RefCountedThreadSafe<URLRequestContextGetter,
                                        URLRequestContextGetterTraits> {
 public:
  virtual URLRequestContext* GetURLRequestContext() = 0;

  // See http://crbug.com/77835 for why this shouldn't be used. Instead use
  // GetURLRequestContext()->cookie_store();
  virtual CookieStore* DONTUSEME_GetCookieStore();

  // Returns a MessageLoopProxy corresponding to the thread on which the
  // request IO happens (the thread on which the returned net::URLRequestContext
  // may be used).
  virtual scoped_refptr<base::MessageLoopProxy>
      GetIOMessageLoopProxy() const = 0;

 protected:
  friend class DeleteTask<const URLRequestContextGetter>;
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
