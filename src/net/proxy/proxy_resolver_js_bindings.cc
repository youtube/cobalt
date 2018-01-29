// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_resolver_js_bindings.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/base/host_cache.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/proxy/proxy_resolver_error_observer.h"
#include "net/proxy/proxy_resolver_request_context.h"
#include "net/proxy/sync_host_resolver.h"

namespace net {

namespace {

// TTL for the per-request DNS cache. Applies to both successful and failed
// DNS resolutions.
const unsigned kCacheEntryTTLSeconds = 5 * 60;

// Returns event parameters for a PAC error message (line number + message).
Value* NetLogErrorCallback(int line_number,
                           const string16* message,
                           NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("line_number", line_number);
  dict->SetString("message", *message);
  return dict;
}

// ProxyResolverJSBindings implementation.
class DefaultJSBindings : public ProxyResolverJSBindings {
 public:
  DefaultJSBindings(SyncHostResolver* host_resolver,
                    NetLog* net_log,
                    ProxyResolverErrorObserver* error_observer)
      : host_resolver_(host_resolver),
        net_log_(net_log),
        error_observer_(error_observer) {
  }

  // Handler for "alert(message)".
  virtual void Alert(const string16& message) override {
    VLOG(1) << "PAC-alert: " << message;

    // Send to the NetLog.
    LogEventToCurrentRequestAndGlobally(
        NetLog::TYPE_PAC_JAVASCRIPT_ALERT,
        NetLog::StringCallback("message", &message));
  }

  // Handler for "myIpAddress()".
  // TODO(eroman): Perhaps enumerate the interfaces directly, using
  // getifaddrs().
  virtual bool MyIpAddress(std::string* first_ip_address) override {
    LogEventToCurrentRequest(NetLog::PHASE_BEGIN,
                             NetLog::TYPE_PAC_JAVASCRIPT_MY_IP_ADDRESS);

    bool ok = MyIpAddressImpl(first_ip_address);

    LogEventToCurrentRequest(NetLog::PHASE_END,
                             NetLog::TYPE_PAC_JAVASCRIPT_MY_IP_ADDRESS);
    return ok;
  }

  // Handler for "myIpAddressEx()".
  virtual bool MyIpAddressEx(std::string* ip_address_list) override {
    LogEventToCurrentRequest(NetLog::PHASE_BEGIN,
                             NetLog::TYPE_PAC_JAVASCRIPT_MY_IP_ADDRESS_EX);

    bool ok = MyIpAddressExImpl(ip_address_list);

    LogEventToCurrentRequest(NetLog::PHASE_END,
                             NetLog::TYPE_PAC_JAVASCRIPT_MY_IP_ADDRESS_EX);
    return ok;
  }

  // Handler for "dnsResolve(host)".
  virtual bool DnsResolve(const std::string& host,
                          std::string* first_ip_address) override {
    LogEventToCurrentRequest(NetLog::PHASE_BEGIN,
                             NetLog::TYPE_PAC_JAVASCRIPT_DNS_RESOLVE);

    bool ok = DnsResolveImpl(host, first_ip_address);

    LogEventToCurrentRequest(NetLog::PHASE_END,
                             NetLog::TYPE_PAC_JAVASCRIPT_DNS_RESOLVE);
    return ok;
  }

  // Handler for "dnsResolveEx(host)".
  virtual bool DnsResolveEx(const std::string& host,
                            std::string* ip_address_list) override {
    LogEventToCurrentRequest(NetLog::PHASE_BEGIN,
                             NetLog::TYPE_PAC_JAVASCRIPT_DNS_RESOLVE_EX);

    bool ok = DnsResolveExImpl(host, ip_address_list);

    LogEventToCurrentRequest(NetLog::PHASE_END,
                             NetLog::TYPE_PAC_JAVASCRIPT_DNS_RESOLVE_EX);
    return ok;
  }

  // Handler for when an error is encountered. |line_number| may be -1.
  virtual void OnError(int line_number, const string16& message) override {
    // Send to the chrome log.
    if (line_number == -1)
      VLOG(1) << "PAC-error: " << message;
    else
      VLOG(1) << "PAC-error: " << "line: " << line_number << ": " << message;

    // Send the error to the NetLog.
    LogEventToCurrentRequestAndGlobally(
        NetLog::TYPE_PAC_JAVASCRIPT_ERROR,
        base::Bind(&NetLogErrorCallback, line_number, &message));

    if (error_observer_.get())
      error_observer_->OnPACScriptError(line_number, message);
  }

  virtual void Shutdown() override {
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
    *first_ip_address = address_list.front().ToStringWithoutPort();
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
    for (AddressList::const_iterator iter = address_list.begin();
         iter != address_list.end(); ++iter) {
      if (!address_list_str.empty())
        address_list_str += ";";
      const std::string address_string = iter->ToStringWithoutPort();
      if (address_string.empty())
        return false;
      address_list_str += address_string;
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

    // Otherwise ask the host resolver.
    const BoundNetLog* net_log = GetNetLogForCurrentRequest();
    int result = host_resolver_->Resolve(info,
                                         address_list,
                                         net_log ? *net_log : BoundNetLog());

    // Save the result back to the per-request DNS cache.
    if (host_cache) {
      host_cache->Set(cache_key, HostCache::Entry(result, *address_list),
                      base::TimeTicks::Now(),
                      base::TimeDelta::FromSeconds(kCacheEntryTTLSeconds));
    }

    return result;
  }

  // May return NULL.
  const BoundNetLog* GetNetLogForCurrentRequest() {
    if (!current_request_context())
      return NULL;
    return current_request_context()->net_log;
  }

  void LogEventToCurrentRequest(
      NetLog::EventPhase phase,
      NetLog::EventType type) {
    const BoundNetLog* net_log = GetNetLogForCurrentRequest();
    if (net_log)
      net_log->AddEntry(type, phase);
  }

  void LogEventToCurrentRequest(
      NetLog::EventPhase phase,
      NetLog::EventType type,
      const NetLog::ParametersCallback& parameters_callback) {
    const BoundNetLog* net_log = GetNetLogForCurrentRequest();
    if (net_log)
      net_log->AddEntry(type, phase, parameters_callback);
  }

  void LogEventToCurrentRequestAndGlobally(
      NetLog::EventType type,
      const NetLog::ParametersCallback& parameters_callback) {
    LogEventToCurrentRequest(NetLog::PHASE_NONE, type, parameters_callback);

    // Emit to the global NetLog event stream.
    if (net_log_)
      net_log_->AddGlobalEntry(type, parameters_callback);
  }

  scoped_ptr<SyncHostResolver> host_resolver_;
  NetLog* net_log_;
  scoped_ptr<ProxyResolverErrorObserver> error_observer_;
  DISALLOW_COPY_AND_ASSIGN(DefaultJSBindings);
};

}  // namespace

// static
ProxyResolverJSBindings* ProxyResolverJSBindings::CreateDefault(
    SyncHostResolver* host_resolver,
    NetLog* net_log,
    ProxyResolverErrorObserver* error_observer) {
  return new DefaultJSBindings(host_resolver, net_log, error_observer);
}

}  // namespace net
