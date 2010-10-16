// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_js_bindings.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/values.h"
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

// Event parameters for a PAC error message (line number + message).
class ErrorNetlogParams : public NetLog::EventParameters {
 public:
  ErrorNetlogParams(int line_number,
                    const string16& message)
      : line_number_(line_number),
        message_(message) {
  }

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("line_number", line_number_);
    dict->SetString("message", message_);
    return dict;
  }

 private:
  const int line_number_;
  const string16 message_;

  DISALLOW_COPY_AND_ASSIGN(ErrorNetlogParams);
};

// Event parameters for a PAC alert().
class AlertNetlogParams : public NetLog::EventParameters {
 public:
  explicit AlertNetlogParams(const string16& message) : message_(message) {
  }

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetString("message", message_);
    return dict;
  }

 private:
  const string16 message_;

  DISALLOW_COPY_AND_ASSIGN(AlertNetlogParams);
};

// ProxyResolverJSBindings implementation.
class DefaultJSBindings : public ProxyResolverJSBindings {
 public:
  DefaultJSBindings(HostResolver* host_resolver, NetLog* net_log)
      : host_resolver_(host_resolver),
        net_log_(net_log) {
  }

  // Handler for "alert(message)".
  virtual void Alert(const string16& message) {
    VLOG(1) << "PAC-alert: " << message;

    // Send to the NetLog.
    LogEventToCurrentRequestAndGlobally(NetLog::TYPE_PAC_JAVASCRIPT_ALERT,
                                        new AlertNetlogParams(message));
  }

  // Handler for "myIpAddress()".
  // TODO(eroman): Perhaps enumerate the interfaces directly, using
  // getifaddrs().
  virtual bool MyIpAddress(std::string* first_ip_address) {
    LogEventToCurrentRequest(NetLog::PHASE_BEGIN,
                             NetLog::TYPE_PAC_JAVASCRIPT_MY_IP_ADDRESS,
                             NULL);

    bool ok = MyIpAddressImpl(first_ip_address);

    LogEventToCurrentRequest(NetLog::PHASE_END,
                             NetLog::TYPE_PAC_JAVASCRIPT_MY_IP_ADDRESS,
                             NULL);
    return ok;
  }

  // Handler for "myIpAddressEx()".
  virtual bool MyIpAddressEx(std::string* ip_address_list) {
    LogEventToCurrentRequest(NetLog::PHASE_BEGIN,
                             NetLog::TYPE_PAC_JAVASCRIPT_MY_IP_ADDRESS_EX,
                             NULL);

    bool ok = MyIpAddressExImpl(ip_address_list);

    LogEventToCurrentRequest(NetLog::PHASE_END,
                             NetLog::TYPE_PAC_JAVASCRIPT_MY_IP_ADDRESS_EX,
                             NULL);
    return ok;
  }

  // Handler for "dnsResolve(host)".
  virtual bool DnsResolve(const std::string& host,
                          std::string* first_ip_address) {
    LogEventToCurrentRequest(NetLog::PHASE_BEGIN,
                             NetLog::TYPE_PAC_JAVASCRIPT_DNS_RESOLVE,
                             NULL);

    bool ok = DnsResolveImpl(host, first_ip_address);

    LogEventToCurrentRequest(NetLog::PHASE_END,
                             NetLog::TYPE_PAC_JAVASCRIPT_DNS_RESOLVE,
                             NULL);
    return ok;
  }

  // Handler for "dnsResolveEx(host)".
  virtual bool DnsResolveEx(const std::string& host,
                            std::string* ip_address_list) {
    LogEventToCurrentRequest(NetLog::PHASE_BEGIN,
                             NetLog::TYPE_PAC_JAVASCRIPT_DNS_RESOLVE_EX,
                             NULL);

    bool ok = DnsResolveExImpl(host, ip_address_list);

    LogEventToCurrentRequest(NetLog::PHASE_END,
                             NetLog::TYPE_PAC_JAVASCRIPT_DNS_RESOLVE_EX,
                             NULL);
    return ok;
  }

  // Handler for when an error is encountered. |line_number| may be -1.
  virtual void OnError(int line_number, const string16& message) {
    // Send to the chrome log.
    if (line_number == -1)
      VLOG(1) << "PAC-error: " << message;
    else
      VLOG(1) << "PAC-error: " << "line: " << line_number << ": " << message;

    // Send the error to the NetLog.
    LogEventToCurrentRequestAndGlobally(
        NetLog::TYPE_PAC_JAVASCRIPT_ERROR,
        new ErrorNetlogParams(line_number, message));
  }

  virtual void Shutdown() {
    host_resolver_->Shutdown();
  }

 private:
  bool MyIpAddressImpl(std::string* first_ip_address) {
    std::string my_hostname = GetHostName();
    if (my_hostname.empty())
      return false;
    return DnsResolveImpl(my_hostname, first_ip_address);
  }

  bool MyIpAddressExImpl(std::string* ip_address_list) {
    std::string my_hostname = GetHostName();
    if (my_hostname.empty())
      return false;
    return DnsResolveExImpl(my_hostname, ip_address_list);
  }

  bool DnsResolveImpl(const std::string& host,
                      std::string* first_ip_address) {
    // Do a sync resolve of the hostname (port doesn't matter).
    // Disable IPv6 results. We do this because the PAC specification isn't
    // really IPv6 friendly, and Internet Explorer also restricts to IPv4.
    // Consequently a lot of existing PAC scripts assume they will only get
    // IPv4 results, and will misbehave if they get an IPv6 result.
    // See http://crbug.com/24641 for more details.
    HostResolver::RequestInfo info(HostPortPair(host, 80));
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

  bool DnsResolveExImpl(const std::string& host,
                        std::string* ip_address_list) {
    // Do a sync resolve of the hostname (port doesn't matter).
    HostResolver::RequestInfo info(HostPortPair(host, 80));
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

  void LogEventToCurrentRequest(
      NetLog::EventPhase phase,
      NetLog::EventType type,
      scoped_refptr<NetLog::EventParameters> params) {
    if (current_request_context() && current_request_context()->net_log)
      current_request_context()->net_log->AddEntry(type, phase, params);
  }

  void LogEventToCurrentRequestAndGlobally(
      NetLog::EventType type,
      scoped_refptr<NetLog::EventParameters> params) {
    LogEventToCurrentRequest(NetLog::PHASE_NONE, type, params);

    // Emit to the global NetLog event stream.
    if (net_log_) {
      net_log_->AddEntry(
          type,
          base::TimeTicks::Now(),
          NetLog::Source(),
          NetLog::PHASE_NONE,
          params);
    }
  }

  HostResolver* const host_resolver_;
  NetLog* net_log_;
};

}  // namespace

// static
ProxyResolverJSBindings* ProxyResolverJSBindings::CreateDefault(
    HostResolver* host_resolver, NetLog* net_log) {
  return new DefaultJSBindings(host_resolver, net_log);
}

}  // namespace net
