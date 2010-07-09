// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_js_bindings.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/address_list.h"
#include "net/base/host_cache.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/base/sys_addrinfo.h"
#include "net/proxy/proxy_resolver_request_context.h"

namespace net {
namespace {

// ProxyResolverJSBindings implementation.
class DefaultJSBindings : public ProxyResolverJSBindings {
 public:
  explicit DefaultJSBindings(HostResolver* host_resolver)
      : host_resolver_(host_resolver) {}

  // Handler for "alert(message)".
  virtual void Alert(const string16& message) {
    LOG(INFO) << "PAC-alert: " << message;
  }

  // Handler for "myIpAddress()".
  // TODO(eroman): Perhaps enumerate the interfaces directly, using
  // getifaddrs().
  virtual bool MyIpAddress(std::string* first_ip_address) {
    std::string my_hostname = GetHostName();
    if (my_hostname.empty())
      return false;
    return DnsResolve(my_hostname, first_ip_address);
  }

  // Handler for "myIpAddressEx()".
  virtual bool MyIpAddressEx(std::string* ip_address_list) {
    std::string my_hostname = GetHostName();
    if (my_hostname.empty())
      return false;
    return DnsResolveEx(my_hostname, ip_address_list);
  }

  // Handler for "dnsResolve(host)".
  virtual bool DnsResolve(const std::string& host,
                          std::string* first_ip_address) {
    // Do a sync resolve of the hostname.
    // Disable IPv6 results. We do this because the PAC specification isn't
    // really IPv6 friendly, and Internet Explorer also restricts to IPv4.
    // Consequently a lot of existing PAC scripts assume they will only get
    // IPv4 results, and will misbehave if they get an IPv6 result.
    // See http://crbug.com/24641 for more details.
    HostResolver::RequestInfo info(host, 80);  // Port doesn't matter.
    info.set_address_family(ADDRESS_FAMILY_IPV4);
    AddressList address_list;

    int result = DnsResolveHelper(info, &address_list);
    if (result != OK)
      return false;

    // There may be multiple results; we will just use the first one.
    // This returns empty string on failure.
    *first_ip_address = net::NetAddressToString(address_list.head());
    if (first_ip_address->empty())
      return false;

    return true;
  }

  // Handler for "dnsResolveEx(host)".
  virtual bool DnsResolveEx(const std::string& host,
                            std::string* ip_address_list) {
    // Do a sync resolve of the hostname.
    HostResolver::RequestInfo info(host, 80);  // Port doesn't matter.
    AddressList address_list;
    int result = DnsResolveHelper(info, &address_list);

    if (result != OK)
      return false;

    // Stringify all of the addresses in the address list, separated
    // by semicolons.
    std::string address_list_str;
    const struct addrinfo* current_address = address_list.head();
    while (current_address) {
      if (!address_list_str.empty())
        address_list_str += ";";
      const std::string address_string = NetAddressToString(current_address);
      if (address_string.empty())
        return false;
      address_list_str += address_string;
      current_address = current_address->ai_next;
    }

    *ip_address_list = address_list_str;
    return true;
  }

  // Handler for when an error is encountered. |line_number| may be -1.
  virtual void OnError(int line_number, const string16& message) {
    if (line_number == -1)
      LOG(INFO) << "PAC-error: " << message;
    else
      LOG(INFO) << "PAC-error: " << "line: " << line_number << ": " << message;
  }

  virtual void Shutdown() {
    host_resolver_->Shutdown();
  }

 private:
  // Helper to execute a synchronous DNS resolve, using the per-request
  // DNS cache if there is one.
  int DnsResolveHelper(const HostResolver::RequestInfo& info,
                       AddressList* address_list) {
    HostCache::Key cache_key(info.hostname(),
                             info.address_family(),
                             info.host_resolver_flags());

    HostCache* host_cache = current_request_context() ?
        current_request_context()->host_cache : NULL;

    // First try to service this request from the per-request DNS cache.
    // (we cache DNS failures much more aggressively within the context
    // of a FindProxyForURL() request).
    if (host_cache) {
      const HostCache::Entry* entry =
          host_cache->Lookup(cache_key, base::TimeTicks::Now());
      if (entry) {
        if (entry->error == OK)
          *address_list = entry->addrlist;
        return entry->error;
      }
    }

    // Otherwise ask the resolver.
    int result = host_resolver_->Resolve(info, address_list, NULL, NULL,
                                         BoundNetLog());

    // Save the result back to the per-request DNS cache.
    if (host_cache) {
      host_cache->Set(cache_key, result, *address_list,
                      base::TimeTicks::Now());
    }

    return result;
  }

  scoped_refptr<HostResolver> host_resolver_;
};

}  // namespace

// static
ProxyResolverJSBindings* ProxyResolverJSBindings::CreateDefault(
    HostResolver* host_resolver) {
  return new DefaultJSBindings(host_resolver);
}

}  // namespace net
