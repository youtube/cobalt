// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_ERROR_OBSERVER_H_
#define NET_PROXY_PROXY_RESOLVER_ERROR_OBSERVER_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"

namespace net {

// Interface for observing JavaScript error messages from PAC scripts. The
// default implementation of the ProxyResolverJSBindings takes a class
// implementing this interface and forwards all JavaScript errors related to
// PAC scripts.
class ProxyResolverErrorObserver {
 public:
  ProxyResolverErrorObserver() {}
  virtual ~ProxyResolverErrorObserver() {}

  // Handler for when an error is encountered. |line_number| may be -1
  // if a line number is not applicable to this error. |error| is a message
  // describing the error.
  virtual void OnPACScriptError(int line_number, const string16& error) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProxyResolverErrorObserver);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_RESOLVER_ERROR_OBSERVER_H_
