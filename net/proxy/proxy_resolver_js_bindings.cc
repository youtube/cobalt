// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_js_bindings.h"

#include "base/logging.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/base/sys_addrinfo.h"

namespace net {
namespace {

// ProxyResolverJSBindings implementation.
class DefaultJSBindings : public ProxyResolverJSBindings {
 public:
  explicit DefaultJSBindings(HostResolver* host_resolver)
      : host_resolver_(host_resolver) {}

  // Handler for "alert(message)".
  virtual void Alert(const std::string& message) {
    LOG(INFO) << "PAC-alert: " << message;
  }

  // Handler for "myIpAddress()". Returns empty string on failure.
  // TODO(eroman): Perhaps enumerate the interfaces directly, using
  // getifaddrs().
  virtual std::string MyIpAddress() {
    // DnsResolve("") returns "", so no need to check for failure.
    return DnsResolve(GetHostName());
  }

  // Handler for "myIpAddressEx()". Returns empty string on failure.
  virtual std::string MyIpAddressEx() {
    return DnsResolveEx(GetHostName());
  }

  // Handler for "dnsResolve(host)". Returns empty string on failure.
  virtual std::string DnsResolve(const std::string& host) {
    // Do a sync resolve of the hostname.
    // Disable IPv6 results. We do this because the PAC specification isn't
    // really IPv6 friendly, and Internet Explorer also restricts to IPv4.
    // Consequently a lot of existing PAC scripts assume they will only get
    // IPv4 results, and will misbehave if they get an IPv6 result.
    // See http://crbug.com/24641 for more details.
    HostResolver::RequestInfo info(host, 80);  // Port doesn't matter.
    info.set_address_family(ADDRESS_FAMILY_IPV4);
    net::AddressList address_list;
    int result = host_resolver_->Resolve(info, &address_list, NULL, NULL,
                                         BoundNetLog());

    if (result != OK)
      return std::string();  // Failed.

    if (!address_list.head())
      return std::string();

    // There may be multiple results; we will just use the first one.
    // This returns empty string on failure.
    return net::NetAddressToString(address_list.head());
  }

  // Handler for "dnsResolveEx(host)". Returns empty string on failure.
  virtual std::string DnsResolveEx(const std::string& host) {
    // Do a sync resolve of the hostname.
    HostResolver::RequestInfo info(host, 80);  // Port doesn't matter.
    net::AddressList address_list;
    int result = host_resolver_->Resolve(info, &address_list, NULL, NULL,
                                         BoundNetLog());

    if (result != OK)
      return std::string();  // Failed.

    // Stringify all of the addresses in the address list, separated
    // by semicolons.
    std::string address_list_str;
    const struct addrinfo* current_address = address_list.head();
    while (current_address) {
      if (!address_list_str.empty())
        address_list_str += ";";
      address_list_str += net::NetAddressToString(current_address);
      current_address = current_address->ai_next;
    }

    return address_list_str;
  }

  // Handler for when an error is encountered. |line_number| may be -1.
  virtual void OnError(int line_number, const std::string& message) {
    if (line_number == -1)
      LOG(INFO) << "PAC-error: " << message;
    else
      LOG(INFO) << "PAC-error: " << "line: " << line_number << ": " << message;
  }

 private:
  scoped_refptr<HostResolver> host_resolver_;
};

}  // namespace

// static
ProxyResolverJSBindings* ProxyResolverJSBindings::CreateDefault(
    HostResolver* host_resolver) {
  return new DefaultJSBindings(host_resolver);
}

}  // namespace net
