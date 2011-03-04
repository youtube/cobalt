// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_SERVICE_H_
#define NET_PROXY_PROXY_SERVICE_H_
#pragma once

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "net/base/completion_callback.h"
#include "net/base/network_change_notifier.h"
#include "net/base/net_log.h"
#include "net/proxy/proxy_config_service.h"
#include "net/proxy/proxy_server.h"
#include "net/proxy/proxy_info.h"

class GURL;
class MessageLoop;

namespace net {

class HostResolver;
class InitProxyResolver;
class ProxyResolver;
class ProxyScriptFetcher;
class URLRequestContext;

// This class can be used to resolve the proxy server to use when loading a
// HTTP(S) URL.  It uses the given ProxyResolver to handle the actual proxy
// resolution.  See ProxyResolverV8 for example.
class ProxyService : public base::RefCountedThreadSafe<ProxyService>,
                     public NetworkChangeNotifier::IPAddressObserver,
                     public ProxyConfigService::Observer {
 public:
  // The instance takes ownership of |config_service| and |resolver|.
  // |net_log| is a possibly NULL destination to send log events to. It must
  // remain alive for the lifetime of this ProxyService.
  ProxyService(ProxyConfigService* config_service,
               ProxyResolver* resolver,
               NetLog* net_log);

  // Used internally to handle PAC queries.
  // TODO(eroman): consider naming this simply "Request".
  class PacRequest;

  // Returns ERR_IO_PENDING if the proxy information could not be provided
  // synchronously, to indicate that the result will be available when the
  // callback is run.  The callback is run on the thread that calls
  // ResolveProxy.
  //
  // The caller is responsible for ensuring that |results| and |callback|
  // remain valid until the callback is run or until |pac_request| is cancelled
  // via CancelPacRequest.  |pac_request| is only valid while the completion
  // callback is still pending. NULL can be passed for |pac_request| if
  // the caller will not need to cancel the request.
  //
  // We use the three possible proxy access types in the following order,
  // doing fallback if one doesn't work.  See "init_proxy_resolver.h"
  // for the specifics.
  //   1.  WPAD auto-detection
  //   2.  PAC URL
  //   3.  named proxy
  //
  // Profiling information for the request is saved to |net_log| if non-NULL.
  int ResolveProxy(const GURL& url,
                   ProxyInfo* results,
                   CompletionCallback* callback,
                   PacRequest** pac_request,
                   const BoundNetLog& net_log);

  // This method is called after a failure to connect or resolve a host name.
  // It gives the proxy service an opportunity to reconsider the proxy to use.
  // The |results| parameter contains the results returned by an earlier call
  // to ResolveProxy.  The semantics of this call are otherwise similar to
  // ResolveProxy.
  //
  // NULL can be passed for |pac_request| if the caller will not need to
  // cancel the request.
  //
  // Returns ERR_FAILED if there is not another proxy config to try.
  //
  // Profiling information for the request is saved to |net_log| if non-NULL.
  int ReconsiderProxyAfterError(const GURL& url,
                                ProxyInfo* results,
                                CompletionCallback* callback,
                                PacRequest** pac_request,
                                const BoundNetLog& net_log);

  // Call this method with a non-null |pac_request| to cancel the PAC request.
  void CancelPacRequest(PacRequest* pac_request);

  // Sets the ProxyScriptFetcher dependency. This is needed if the ProxyResolver
  // is of type ProxyResolverWithoutFetch. ProxyService takes ownership of
  // |proxy_script_fetcher|.
  void SetProxyScriptFetcher(ProxyScriptFetcher* proxy_script_fetcher);
  ProxyScriptFetcher* GetProxyScriptFetcher() const;

  // Tells this ProxyService to start using a new ProxyConfigService to
  // retrieve its ProxyConfig from. The new ProxyConfigService will immediately
  // be queried for new config info which will be used for all subsequent
  // ResolveProxy calls. ProxyService takes ownership of
  // |new_proxy_config_service|.
  void ResetConfigService(ProxyConfigService* new_proxy_config_service);

  // Tells the resolver to purge any memory it does not need.
  void PurgeMemory();


  // Returns the last configuration fetched from ProxyConfigService.
  const ProxyConfig& fetched_config() {
    return fetched_config_;
  }

  // Returns the current configuration being used by ProxyConfigService.
  const ProxyConfig& config() {
    return config_;
  }

  // Returns the map of proxies which have been marked as "bad".
  const ProxyRetryInfoMap& proxy_retry_info() const {
    return proxy_retry_info_;
  }

  // Clears the list of bad proxy servers that has been cached.
  void ClearBadProxiesCache() {
    proxy_retry_info_.clear();
  }

  // Forces refetching the proxy configuration, and applying it.
  // This re-does everything from fetching the system configuration,
  // to downloading and testing the PAC files.
  void ForceReloadProxyConfig();

  // Creates a proxy service that polls |proxy_config_service| to notice when
  // the proxy settings change. We take ownership of |proxy_config_service|.
  //
  // |num_pac_threads| specifies the maximum number of threads to use for
  // executing PAC scripts. Threads are created lazily on demand.
  // If |0| is specified, then a default number of threads will be selected.
  //
  // Having more threads avoids stalling proxy resolve requests when the
  // PAC script takes a while to run. This is particularly a problem when PAC
  // scripts do synchronous DNS resolutions, since that can take on the order
  // of seconds.
  //
  // However, the disadvantages of using more than 1 thread are:
  //   (a) can cause compatibility issues for scripts that rely on side effects
  //       between runs (such scripts should not be common though).
  //   (b) increases the memory used by proxy resolving, as each thread will
  //       duplicate its own script context.

  // |proxy_script_fetcher| specifies the dependency to use for downloading
  // any PAC scripts. The resulting ProxyService will take ownership of it.
  //
  // |host_resolver| points to the host resolving dependency the PAC script
  // should use for any DNS queries. It must remain valid throughout the
  // lifetime of the ProxyService.
  //
  // ##########################################################################
  // # See the warnings in net/proxy/proxy_resolver_v8.h describing the
  // # multi-threading model. In order for this to be safe to use, *ALL* the
  // # other V8's running in the process must use v8::Locker.
  // ##########################################################################
  static ProxyService* CreateUsingV8ProxyResolver(
      ProxyConfigService* proxy_config_service,
      size_t num_pac_threads,
      ProxyScriptFetcher* proxy_script_fetcher,
      HostResolver* host_resolver,
      NetLog* net_log);

  // Same as CreateUsingV8ProxyResolver, except it uses system libraries
  // for evaluating the PAC script if available, otherwise skips
  // proxy autoconfig.
  static ProxyService* CreateUsingSystemProxyResolver(
      ProxyConfigService* proxy_config_service,
      size_t num_pac_threads,
      NetLog* net_log);

  // Creates a ProxyService without support for proxy autoconfig.
  static ProxyService* CreateWithoutProxyResolver(
      ProxyConfigService* proxy_config_service,
      NetLog* net_log);

  // Convenience methods that creates a proxy service using the
  // specified fixed settings.
  static ProxyService* CreateFixed(const ProxyConfig& pc);
  static ProxyService* CreateFixed(const std::string& proxy);

  // Creates a proxy service that uses a DIRECT connection for all requests.
  static ProxyService* CreateDirect();
  // |net_log|'s lifetime must exceed ProxyService.
  static ProxyService* CreateDirectWithNetLog(NetLog* net_log);

  // This method is used by tests to create a ProxyService that returns a
  // hardcoded proxy fallback list (|pac_string|) for every URL.
  //
  // |pac_string| is a list of proxy servers, in the format that a PAC script
  // would return it. For example, "PROXY foobar:99; SOCKS fml:2; DIRECT"
  static ProxyService* CreateFixedFromPacResult(const std::string& pac_string);

  // Creates a config service appropriate for this platform that fetches the
  // system proxy settings.
  static ProxyConfigService* CreateSystemProxyConfigService(
      MessageLoop* io_loop, MessageLoop* file_loop);

#if UNIT_TEST
  void set_stall_proxy_auto_config_delay(base::TimeDelta delay) {
    stall_proxy_auto_config_delay_ = delay;
  }
#endif

 private:
  friend class base::RefCountedThreadSafe<ProxyService>;
  FRIEND_TEST_ALL_PREFIXES(ProxyServiceTest, UpdateConfigAfterFailedAutodetect);
  FRIEND_TEST_ALL_PREFIXES(ProxyServiceTest, UpdateConfigFromPACToDirect);
  friend class PacRequest;

  // TODO(eroman): change this to a std::set. Note that this requires updating
  // some tests in proxy_service_unittest.cc such as:
  //   ProxyServiceTest.InitialPACScriptDownload
  // which expects requests to finish in the order they were added.
  typedef std::vector<scoped_refptr<PacRequest> > PendingRequests;

  enum State {
    STATE_NONE,
    STATE_WAITING_FOR_PROXY_CONFIG,
    STATE_WAITING_FOR_INIT_PROXY_RESOLVER,
    STATE_READY,
  };

  virtual ~ProxyService();

  // Resets all the variables associated with the current proxy configuration,
  // and rewinds the current state to |STATE_NONE|. Returns the previous value
  // of |current_state_|.  If |reset_fetched_config| is true then
  // |fetched_config_| will also be reset, otherwise it will be left as-is.
  // Resetting it means that we will have to re-fetch the configuration from
  // the ProxyConfigService later.
  State ResetProxyConfig(bool reset_fetched_config);

  // Retrieves the current proxy configuration from the ProxyConfigService, and
  // starts initializing for it.
  void ApplyProxyConfigIfAvailable();

  // Callback for when the proxy resolver has been initialized with a
  // PAC script.
  void OnInitProxyResolverComplete(int result);

  // Returns ERR_IO_PENDING if the request cannot be completed synchronously.
  // Otherwise it fills |result| with the proxy information for |url|.
  // Completing synchronously means we don't need to query ProxyResolver.
  int TryToCompleteSynchronously(const GURL& url, ProxyInfo* result);

  // Cancels all of the requests sent to the ProxyResolver. These will be
  // restarted when calling ResumeAllPendingRequests().
  void SuspendAllPendingRequests();

  // Advances the current state to |STATE_READY|, and resumes any pending
  // requests which had been stalled waiting for initialization to complete.
  void SetReady();

  // Returns true if |pending_requests_| contains |req|.
  bool ContainsPendingRequest(PacRequest* req);

  // Removes |req| from the list of pending requests.
  void RemovePendingRequest(PacRequest* req);

  // Called when proxy resolution has completed (either synchronously or
  // asynchronously). Handles logging the result, and cleaning out
  // bad entries from the results list.
  int DidFinishResolvingProxy(ProxyInfo* result,
                              int result_code,
                              const BoundNetLog& net_log);

  // Start initialization using |fetched_config_|.
  void InitializeUsingLastFetchedConfig();

  // NetworkChangeNotifier::IPAddressObserver
  // When this is called, we re-fetch PAC scripts and re-run WPAD.
  virtual void OnIPAddressChanged();

  // ProxyConfigService::Observer
  virtual void OnProxyConfigChanged(const ProxyConfig& config);

  scoped_ptr<ProxyConfigService> config_service_;
  scoped_ptr<ProxyResolver> resolver_;

  // We store the proxy configuration that was last fetched from the
  // ProxyConfigService, as well as the resulting "effective" configuration.
  // The effective configuration is what we condense the original fetched
  // settings to after testing the various automatic settings (auto-detect
  // and custom PAC url).
  ProxyConfig fetched_config_;
  ProxyConfig config_;

  // Increasing ID to give to the next ProxyConfig that we set.
  int next_config_id_;

  // The time when the proxy configuration was last read from the system.
  base::TimeTicks config_last_update_time_;

  // Map of the known bad proxies and the information about the retry time.
  ProxyRetryInfoMap proxy_retry_info_;

  // Set of pending/inprogress requests.
  PendingRequests pending_requests_;

  // The fetcher to use when downloading PAC scripts for the ProxyResolver.
  // This dependency can be NULL if our ProxyResolver has no need for
  // external PAC script fetching.
  scoped_ptr<ProxyScriptFetcher> proxy_script_fetcher_;

  // Callback for when |init_proxy_resolver_| is done.
  CompletionCallbackImpl<ProxyService> init_proxy_resolver_callback_;

  // Helper to download the PAC script (wpad + custom) and apply fallback rules.
  //
  // Note that the declaration is important here: |proxy_script_fetcher_| and
  // |proxy_resolver_| must outlive |init_proxy_resolver_|.
  scoped_ptr<InitProxyResolver> init_proxy_resolver_;

  State current_state_;

  // This is the log where any events generated by |init_proxy_resolver_| are
  // sent to.
  NetLog* net_log_;

  // The earliest time at which we should run any proxy auto-config. (Used to
  // stall re-configuration following an IP address change).
  base::TimeTicks stall_proxy_autoconfig_until_;

  // The amount of time to stall requests following IP address changes.
  base::TimeDelta stall_proxy_auto_config_delay_;

  DISALLOW_COPY_AND_ASSIGN(ProxyService);
};

// Wrapper for invoking methods on a ProxyService synchronously.
class SyncProxyServiceHelper
    : public base::RefCountedThreadSafe<SyncProxyServiceHelper> {
 public:
  SyncProxyServiceHelper(MessageLoop* io_message_loop,
                         ProxyService* proxy_service);

  int ResolveProxy(const GURL& url,
                   ProxyInfo* proxy_info,
                   const BoundNetLog& net_log);
  int ReconsiderProxyAfterError(const GURL& url,
                                ProxyInfo* proxy_info,
                                const BoundNetLog& net_log);

 private:
  friend class base::RefCountedThreadSafe<SyncProxyServiceHelper>;

  virtual ~SyncProxyServiceHelper();

  void StartAsyncResolve(const GURL& url, const BoundNetLog& net_log);
  void StartAsyncReconsider(const GURL& url, const BoundNetLog& net_log);

  void OnCompletion(int result);

  MessageLoop* io_message_loop_;
  ProxyService* proxy_service_;

  base::WaitableEvent event_;
  CompletionCallbackImpl<SyncProxyServiceHelper> callback_;
  ProxyInfo proxy_info_;
  int result_;
};

}  // namespace net

#endif  // NET_PROXY_PROXY_SERVICE_H_
