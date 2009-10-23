// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "net/proxy/proxy_resolver_js_bindings.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/waitable_event.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

namespace net {
namespace {

// Wrapper around HostResolver to give a sync API while running the resolve
// in async mode on |host_resolver_loop|. If |host_resolver_loop| is NULL,
// runs sync on the current thread (this mode is just used by testing).
class SyncHostResolverBridge
    : public base::RefCountedThreadSafe<SyncHostResolverBridge> {
 public:
  SyncHostResolverBridge(HostResolver* host_resolver,
                         MessageLoop* host_resolver_loop)
      : host_resolver_(host_resolver),
        host_resolver_loop_(host_resolver_loop),
        event_(false, false),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            callback_(this, &SyncHostResolverBridge::OnResolveCompletion)) {
  }

  // Run the resolve on host_resolver_loop, and wait for result.
  int Resolve(const std::string& hostname,
              AddressFamily address_family,
              net::AddressList* addresses) {
    // Port number doesn't matter.
    HostResolver::RequestInfo info(hostname, 80);
    info.set_address_family(address_family);

    // Hack for tests -- run synchronously on current thread.
    if (!host_resolver_loop_)
      return host_resolver_->Resolve(info, addresses, NULL, NULL, NULL);

    // Otherwise start an async resolve on the resolver's thread.
    host_resolver_loop_->PostTask(FROM_HERE, NewRunnableMethod(this,
        &SyncHostResolverBridge::StartResolve, info, addresses));

    // Wait for the resolve to complete in the resolver's thread.
    event_.Wait();
    return err_;
  }

 private:
  // Called on host_resolver_loop_.
  void StartResolve(const HostResolver::RequestInfo& info,
                    net::AddressList* addresses) {
    DCHECK_EQ(host_resolver_loop_, MessageLoop::current());
    int error = host_resolver_->Resolve(
        info, addresses, &callback_, NULL, NULL);
    if (error != ERR_IO_PENDING)
      OnResolveCompletion(error);  // Completed synchronously.
  }

  // Called on host_resolver_loop_.
  void OnResolveCompletion(int result) {
    DCHECK_EQ(host_resolver_loop_, MessageLoop::current());
    err_ = result;
    event_.Signal();
  }

  scoped_refptr<HostResolver> host_resolver_;
  MessageLoop* host_resolver_loop_;

  // Event to notify completion of resolve request.
  base::WaitableEvent event_;

  // Callback for when the resolve completes on host_resolver_loop_.
  net::CompletionCallbackImpl<SyncHostResolverBridge> callback_;

  // The result from the result request (set by in host_resolver_loop_).
  int err_;
};

// ProxyResolverJSBindings implementation.
class DefaultJSBindings : public ProxyResolverJSBindings {
 public:
  DefaultJSBindings(HostResolver* host_resolver,
                    MessageLoop* host_resolver_loop)
      : host_resolver_(new SyncHostResolverBridge(
          host_resolver, host_resolver_loop)) {}

  // Handler for "alert(message)".
  virtual void Alert(const std::string& message) {
    LOG(INFO) << "PAC-alert: " << message;
  }

  // Handler for "myIpAddress()". Returns empty string on failure.
  virtual std::string MyIpAddress() {
    // DnsResolve("") returns "", so no need to check for failure.
    return DnsResolve(GetHostName());
  }

  // Handler for "dnsResolve(host)". Returns empty string on failure.
  virtual std::string DnsResolve(const std::string& host) {
    // TODO(eroman): Should this return our IP address, or fail, or
    // simply be unspecified (works differently on windows and mac os x).
    if (host.empty())
      return std::string();

    // Do a sync resolve of the hostname.
    // Disable IPv6 results. We do this because Internet Explorer does it --
    // consequently a lot of existing PAC scripts assume they will only get
    // IPv4 results, and will misbehave if they get an IPv6 result.
    // See http://crbug.com/24641 for more details.
    net::AddressList address_list;
    int result = host_resolver_->Resolve(host,
                                         ADDRESS_FAMILY_IPV4,
                                         &address_list);

    if (result != OK)
      return std::string();  // Failed.

    if (!address_list.head())
      return std::string();

    // There may be multiple results; we will just use the first one.
    // This returns empty string on failure.
    return net::NetAddressToString(address_list.head());
  }

  // Handler for when an error is encountered. |line_number| may be -1.
  virtual void OnError(int line_number, const std::string& message) {
    if (line_number == -1)
      LOG(INFO) << "PAC-error: " << message;
    else
      LOG(INFO) << "PAC-error: " << "line: " << line_number << ": " << message;
  }

 private:
  scoped_refptr<SyncHostResolverBridge> host_resolver_;
};

}  // namespace

// static
ProxyResolverJSBindings* ProxyResolverJSBindings::CreateDefault(
    HostResolver* host_resolver, MessageLoop* host_resolver_loop) {
  return new DefaultJSBindings(host_resolver, host_resolver_loop);
}

}  // namespace net
