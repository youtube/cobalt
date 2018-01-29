// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_service_v8.h"

#include "base/logging.h"
#include "net/proxy/multi_threaded_proxy_resolver.h"
#include "net/proxy/network_delegate_error_observer.h"
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_resolver_js_bindings.h"
#include "net/proxy/proxy_resolver_v8.h"
#include "net/proxy/proxy_service.h"
#include "net/proxy/sync_host_resolver_bridge.h"

namespace net {
namespace {

// This factory creates V8ProxyResolvers with appropriate javascript bindings.
class ProxyResolverFactoryForV8 : public ProxyResolverFactory {
 public:
  // |async_host_resolver|, |io_loop| and |net_log| must remain
  // valid for the duration of our lifetime.
  // |async_host_resolver| will only be operated on |io_loop|.
  // TODO(willchan): remove io_loop and replace it with origin_loop.
  ProxyResolverFactoryForV8(HostResolver* async_host_resolver,
                            MessageLoop* io_loop,
                            base::MessageLoopProxy* origin_loop,
                            NetLog* net_log,
                            NetworkDelegate* network_delegate)
      : ProxyResolverFactory(true /*expects_pac_bytes*/),
        async_host_resolver_(async_host_resolver),
        io_loop_(io_loop),
        origin_loop_(origin_loop),
        net_log_(net_log),
        network_delegate_(network_delegate) {
  }

  virtual ProxyResolver* CreateProxyResolver() override {
    // Create a synchronous host resolver wrapper that operates
    // |async_host_resolver_| on |io_loop_|.
    SyncHostResolverBridge* sync_host_resolver =
        new SyncHostResolverBridge(async_host_resolver_, io_loop_);

    NetworkDelegateErrorObserver* error_observer =
        new NetworkDelegateErrorObserver(
            network_delegate_, origin_loop_.get());

    // ProxyResolverJSBindings takes ownership of |error_observer| and
    // |sync_host_resolver|.
    ProxyResolverJSBindings* js_bindings =
        ProxyResolverJSBindings::CreateDefault(
            sync_host_resolver, net_log_, error_observer);

    // ProxyResolverV8 takes ownership of |js_bindings|.
    return new ProxyResolverV8(js_bindings);
  }

 private:
  HostResolver* const async_host_resolver_;
  MessageLoop* io_loop_;
  scoped_refptr<base::MessageLoopProxy> origin_loop_;
  NetLog* net_log_;
  NetworkDelegate* network_delegate_;
};

}  // namespace

// static
ProxyService* CreateProxyServiceUsingV8ProxyResolver(
    ProxyConfigService* proxy_config_service,
    size_t num_pac_threads,
    ProxyScriptFetcher* proxy_script_fetcher,
    DhcpProxyScriptFetcher* dhcp_proxy_script_fetcher,
    HostResolver* host_resolver,
    NetLog* net_log,
    NetworkDelegate* network_delegate) {
  DCHECK(proxy_config_service);
  DCHECK(proxy_script_fetcher);
  DCHECK(dhcp_proxy_script_fetcher);
  DCHECK(host_resolver);

  if (num_pac_threads == 0)
    num_pac_threads = ProxyService::kDefaultNumPacThreads;

  ProxyResolverFactory* sync_resolver_factory =
      new ProxyResolverFactoryForV8(
          host_resolver,
          MessageLoop::current(),
          base::MessageLoopProxy::current(),
          net_log,
          network_delegate);

  ProxyResolver* proxy_resolver =
      new MultiThreadedProxyResolver(sync_resolver_factory, num_pac_threads);

  ProxyService* proxy_service =
      new ProxyService(proxy_config_service, proxy_resolver, net_log);

  // Configure fetchers to use for PAC script downloads and auto-detect.
  proxy_service->SetProxyScriptFetchers(proxy_script_fetcher,
                                        dhcp_proxy_script_fetcher);

  return proxy_service;
}

}  // namespace net
