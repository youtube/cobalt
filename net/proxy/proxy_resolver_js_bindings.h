// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_JS_BINDINGS_H
#define NET_PROXY_PROXY_JS_BINDINGS_H

#include <string>

class MessageLoop;

namespace net {

class HostResolver;

// Interface for the javascript bindings.
class ProxyResolverJSBindings {
 public:
  virtual ~ProxyResolverJSBindings() {}

  // Handler for "alert(message)"
  virtual void Alert(const std::string& message) = 0;

  // Handler for "myIpAddress()". Returns empty string on failure.
  virtual std::string MyIpAddress() = 0;

  // Handler for "myIpAddressEx()". Returns empty string on failure.
  //
  // This is a Microsoft extension to PAC for IPv6, see:
  // http://blogs.msdn.com/wndp/articles/IPV6_PAC_Extensions_v0_9.aspx
  virtual std::string MyIpAddressEx() = 0;

  // Handler for "dnsResolve(host)". Returns empty string on failure.
  virtual std::string DnsResolve(const std::string& host) = 0;

  // Handler for "dnsResolveEx(host)". Returns empty string on failure.
  //
  // This is a Microsoft extension to PAC for IPv6, see:
  // http://blogs.msdn.com/wndp/articles/IPV6_PAC_Extensions_v0_9.aspx
  virtual std::string DnsResolveEx(const std::string& host) = 0;

  // Handler for when an error is encountered. |line_number| may be -1
  // if a line number is not applicable to this error.
  virtual void OnError(int line_number, const std::string& error) = 0;

  // Creates a default javascript bindings implementation that will:
  //   - Send script error messages to LOG(INFO)
  //   - Send script alert()s to LOG(INFO)
  //   - Use the provided host resolver to service dnsResolve().
  //
  // |host_resolver| will be used in async mode on |host_resolver_loop|. If
  // |host_resolver_loop| is NULL, then |host_resolver| will be used in sync
  // mode on the PAC thread.
  static ProxyResolverJSBindings* CreateDefault(
      HostResolver* host_resolver, MessageLoop* host_resolver_loop);
};

}  // namespace net

#endif  // NET_PROXY_PROXY_JS_BINDINGS_H
