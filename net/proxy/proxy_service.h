// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_SERVICE_H_
#define NET_PROXY_PROXY_SERVICE_H_

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/waitable_event.h"
#include "net/base/completion_callback.h"
#include "net/proxy/proxy_server.h"
#include "net/proxy/proxy_info.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class GURL;
class MessageLoop;
class URLRequestContext;

namespace net {

class InitProxyResolver;
class LoadLog;
class ProxyConfigService;
class ProxyResolver;
class ProxyScriptFetcher;

// This class can be used to resolve the proxy server to use when loading a
// HTTP(S) URL.  It uses the given ProxyResolver to handle the actual proxy
// resolution.  See ProxyResolverV8 for example.
class ProxyService : public base::RefCountedThreadSafe<ProxyService> {
 public:
  // The instance takes ownership of |config_service| and |resolver|.
  ProxyService(ProxyConfigService* config_service, ProxyResolver* resolver);

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
  // Profiling information for the request is saved to |load_log| if non-NULL.
  int ResolveProxy(const GURL& url,
                   ProxyInfo* results,
                   CompletionCallback* callback,
                   PacRequest** pac_request,
                   LoadLog* load_log);

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
  // Profiling information for the request is saved to |load_log| if non-NULL.
  int ReconsiderProxyAfterError(const GURL& url,
                                ProxyInfo* results,
                                CompletionCallback* callback,
                                PacRequest** pac_request,
                                LoadLog* load_log);

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

  // Returns the log for the most recent WPAD + PAC initialization.
  // (This shows how much time was spent downloading and parsing the
  // PAC scripts for the current configuration).
  LoadLog* init_proxy_resolver_log() const {
    return init_proxy_resolver_log_;
  }

  // Returns true if we have called UpdateConfig() at least once.
  bool config_has_been_initialized() const {
    return config_.id() != ProxyConfig::INVALID_ID;
  }

  // Returns the last configuration fetched from ProxyConfigService.
  const ProxyConfig& config() {
    return config_;
  }

  // Creates a proxy service that polls |proxy_config_service| to notice when
  // the proxy settings change. We take ownership of |proxy_config_service|.
  // Iff |use_v8_resolver| is true, then the V8 implementation is
  // used.
  // |url_request_context| is only used when use_v8_resolver is true:
  // it specifies the URL request context that will be used if a PAC
  // script needs to be fetched.
  // |io_loop| points to the IO thread's message loop. It is only used
  // when pc is NULL.
  // ##########################################################################
  // # See the warnings in net/proxy/proxy_resolver_v8.h describing the
  // # multi-threading model. In order for this to be safe to use, *ALL* the
  // # other V8's running in the process must use v8::Locker.
  // ##########################################################################
  static ProxyService* Create(
      ProxyConfigService* proxy_config_service,
      bool use_v8_resolver,
      URLRequestContext* url_request_context,
      MessageLoop* io_loop);

  // Convenience method that creates a proxy service using the
  // specified fixed settings. |pc| must not be NULL.
  static ProxyService* CreateFixed(const ProxyConfig& pc);

  // Creates a proxy service that always fails to fetch the proxy configuration,
  // so it falls back to direct connect.
  static ProxyService* CreateNull();

  // Creates a config service appropriate for this platform that fetches the
  // system proxy settings.
  static ProxyConfigService* CreateSystemProxyConfigService(
      MessageLoop* io_loop, MessageLoop* file_loop);

 private:
  friend class base::RefCountedThreadSafe<ProxyService>;
  FRIEND_TEST(ProxyServiceTest, IsLocalName);
  FRIEND_TEST(ProxyServiceTest, UpdateConfigAfterFailedAutodetect);
  FRIEND_TEST(ProxyServiceTest, UpdateConfigFromPACToDirect);
  friend class PacRequest;

  // TODO(eroman): change this to a std::set. Note that this requires updating
  // some tests in proxy_service_unittest.cc such as:
  //   ProxyServiceTest.InitialPACScriptDownload
  // which expects requests to finish in the order they were added.
  typedef std::vector<scoped_refptr<PacRequest> > PendingRequests;

  ~ProxyService();

  // Creates a proxy resolver appropriate for this platform that doesn't rely
  // on V8.
  static ProxyResolver* CreateNonV8ProxyResolver();

  // Identifies the proxy configuration.
  ProxyConfig::ID config_id() const { return config_.id(); }

  // Checks to see if the proxy configuration changed, and then updates config_
  // to reference the new configuration.
  void UpdateConfig(LoadLog* load_log);

  // Assign |config| as the current configuration.
  void SetConfig(const ProxyConfig& config);

  // Starts downloading and testing the various PAC choices.
  // Calls OnInitProxyResolverComplete() when completed.
  void StartInitProxyResolver();

  // Tries to update the configuration if it hasn't been checked in a while.
  void UpdateConfigIfOld(LoadLog* load_log);

  // Returns true if the proxy resolver is being initialized for PAC
  // (downloading PAC script(s) + testing).
  // Resolve requests will be frozen until the initialization has completed.
  bool IsInitializingProxyResolver() const {
    return init_proxy_resolver_.get() != NULL;
  }

  // Callback for when the proxy resolver has been initialized with a
  // PAC script.
  void OnInitProxyResolverComplete(int result);

  // Returns ERR_IO_PENDING if the request cannot be completed synchronously.
  // Otherwise it fills |result| with the proxy information for |url|.
  // Completing synchronously means we don't need to query ProxyResolver.
  int TryToCompleteSynchronously(const GURL& url, ProxyInfo* result);

  // Set |result| with the proxy to use for |url|, based on |rules|.
  void ApplyProxyRules(const GURL& url,
                       const ProxyConfig::ProxyRules& rules,
                       ProxyInfo* result);

  // Cancel all of the requests sent to the ProxyResolver. These will be
  // restarted when calling ResumeAllPendingRequests().
  void SuspendAllPendingRequests();

  // Sends all the unstarted pending requests off to the resolver.
  void ResumeAllPendingRequests();

  // Returns true if |pending_requests_| contains |req|.
  bool ContainsPendingRequest(PacRequest* req);

  // Removes |req| from the list of pending requests.
  void RemovePendingRequest(PacRequest* req);

  // Called when proxy resolution has completed (either synchronously or
  // asynchronously). Handles logging the result, and cleaning out
  // bad entries from the results list.
  int DidFinishResolvingProxy(ProxyInfo* result,
                              int result_code,
                              LoadLog* load_log);

  // Returns true if the URL passed in should not go through the proxy server.
  // 1. If the proxy settings say to bypass local names, and |IsLocalName(url)|.
  // 2. The URL matches one of the entities in the proxy bypass list.
  bool ShouldBypassProxyForURL(const GURL& url);

  // Returns true if |url| is to an intranet site (using non-FQDN as the
  // heuristic).
  static bool IsLocalName(const GURL& url);

  scoped_ptr<ProxyConfigService> config_service_;
  scoped_ptr<ProxyResolver> resolver_;

  // We store the proxy config and a counter (ID) that is incremented each time
  // the config changes.
  ProxyConfig config_;

  // Increasing ID to give to the next ProxyConfig that we set.
  int next_config_id_;

  // Indicates whether the ProxyResolver should be sent requests.
  bool should_use_proxy_resolver_;

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

  // Log from the *last* time |init_proxy_resolver_.Init()| was called, or NULL.
  scoped_refptr<LoadLog> init_proxy_resolver_log_;

  DISALLOW_COPY_AND_ASSIGN(ProxyService);
};

// Wrapper for invoking methods on a ProxyService synchronously.
class SyncProxyServiceHelper
    : public base::RefCountedThreadSafe<SyncProxyServiceHelper> {
 public:
  SyncProxyServiceHelper(MessageLoop* io_message_loop,
                         ProxyService* proxy_service);

  int ResolveProxy(const GURL& url, ProxyInfo* proxy_info, LoadLog* load_log);
  int ReconsiderProxyAfterError(const GURL& url,
                                ProxyInfo* proxy_info, LoadLog* load_log);

 private:
  friend class base::RefCountedThreadSafe<SyncProxyServiceHelper>;

  ~SyncProxyServiceHelper() {}

  void StartAsyncResolve(const GURL& url, LoadLog* load_log);
  void StartAsyncReconsider(const GURL& url, LoadLog* load_log);

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
