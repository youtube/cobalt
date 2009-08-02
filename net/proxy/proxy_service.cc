// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_service.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_script_fetcher.h"
#if defined(OS_WIN)
#include "net/proxy/proxy_config_service_win.h"
#include "net/proxy/proxy_resolver_winhttp.h"
#elif defined(OS_MACOSX)
#include "net/proxy/proxy_resolver_mac.h"
#elif defined(OS_LINUX)
#include "net/proxy/proxy_config_service_linux.h"
#endif
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_resolver_js_bindings.h"
#include "net/proxy/proxy_resolver_v8.h"
#include "net/proxy/single_threaded_proxy_resolver.h"
#include "net/url_request/url_request_context.h"

using base::TimeDelta;
using base::TimeTicks;

namespace net {

// Config getter that fails every time.
class ProxyConfigServiceNull : public ProxyConfigService {
 public:
  // ProxyConfigService implementation:
  virtual int GetProxyConfig(ProxyConfig* config) {
    return ERR_NOT_IMPLEMENTED;
  }
};

// Proxy resolver that fails every time.
class ProxyResolverNull : public ProxyResolver {
 public:
  ProxyResolverNull() : ProxyResolver(false /*expects_pac_bytes*/) {}

  // ProxyResolver implementation:
  virtual int GetProxyForURL(const GURL& url,
                             ProxyInfo* results,
                             CompletionCallback* callback,
                             RequestHandle* request) {
    return ERR_NOT_IMPLEMENTED;
  }

  virtual void CancelRequest(RequestHandle request) {
    NOTREACHED();
  }

 private:
  virtual void SetPacScriptByUrlInternal(const GURL& pac_url) {}
};

// Strip away any reference fragments and the username/password, as they
// are not relevant to proxy resolution.
static GURL SanitizeURLForProxyResolver(const GURL& url) {
  // TODO(eroman): The following duplicates logic from
  // HttpUtil::SpecForRequest. Should probably live in net_util.h
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

// ProxyService::PacRequest ---------------------------------------------------

class ProxyService::PacRequest
    : public base::RefCounted<ProxyService::PacRequest> {
 public:
  PacRequest(ProxyService* service,
             const GURL& url,
             ProxyInfo* results,
             CompletionCallback* user_callback)
      : service_(service),
        user_callback_(user_callback),
        ALLOW_THIS_IN_INITIALIZER_LIST(io_callback_(
            this, &PacRequest::QueryComplete)),
        results_(results),
        url_(url),
        resolve_job_(NULL),
        config_id_(ProxyConfig::INVALID_ID) {
    DCHECK(user_callback);
  }

  // Starts the resolve proxy request.
  int Start() {
    DCHECK(!was_cancelled());
    DCHECK(!is_started());

    config_id_ = service_->config_.id();

    return resolver()->GetProxyForURL(
        url_, results_, &io_callback_, &resolve_job_);
  }

  bool is_started() const {
    // Note that !! casts to bool. (VS gives a warning otherwise).
    return !!resolve_job_;
  }

  void StartAndComplete() {
    int rv = Start();
    if (rv != ERR_IO_PENDING)
      QueryComplete(rv);
  }

  void Cancel() {
    // The request may already be running in the resolver.
    if (is_started()) {
      resolver()->CancelRequest(resolve_job_);
      resolve_job_ = NULL;
    }

    // Mark as cancelled, to prevent accessing this again later.
    service_ = NULL;
    user_callback_ = NULL;
    results_ = NULL;
  }

  // Returns true if Cancel() has been called.
  bool was_cancelled() const { return user_callback_ == NULL; }

  // Helper to call after ProxyResolver completion (both synchronous and
  // asynchronous). Fixes up the result that is to be returned to user.
  int QueryDidComplete(int result_code) {
    DCHECK(!was_cancelled());

    // Make a note in the results which configuration was in use at the
    // time of the resolve.
    results_->config_id_ = config_id_;

    // Reset the state associated with in-progress-resolve.
    resolve_job_ = NULL;
    config_id_ = ProxyConfig::INVALID_ID;

    // Notify the service of the completion.
    service_->DidCompletePacRequest(results_->config_id_, result_code);

    // Clean up the results list.
    if (result_code == OK)
      results_->RemoveBadProxies(service_->proxy_retry_info_);

    return result_code;
  }

 private:
  // Callback for when the ProxyResolver request has completed.
  void QueryComplete(int result_code) {
    result_code = QueryDidComplete(result_code);

    // Remove this completed PacRequest from the service's pending list.
    /// (which will probably cause deletion of |this|).
    CompletionCallback* callback = user_callback_;
    service_->RemovePendingRequest(this);

    callback->Run(result_code);
  }

  ProxyResolver* resolver() const { return service_->resolver_.get(); }

  ProxyService* service_;
  CompletionCallback* user_callback_;
  CompletionCallbackImpl<PacRequest> io_callback_;
  ProxyInfo* results_;
  GURL url_;
  ProxyResolver::RequestHandle resolve_job_;
  ProxyConfig::ID config_id_;  // The config id when the resolve was started.
};

// ProxyService ---------------------------------------------------------------

ProxyService::ProxyService(ProxyConfigService* config_service,
                           ProxyResolver* resolver)
    : config_service_(config_service),
      resolver_(resolver),
      next_config_id_(1),
      config_is_bad_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(proxy_script_fetcher_callback_(
          this, &ProxyService::OnScriptFetchCompletion)),
      fetched_pac_config_id_(ProxyConfig::INVALID_ID),
      fetched_pac_error_(OK),
      in_progress_fetch_config_id_(ProxyConfig::INVALID_ID) {
}

// static
ProxyService* ProxyService::Create(
    const ProxyConfig* pc,
    bool use_v8_resolver,
    URLRequestContext* url_request_context,
    MessageLoop* io_loop) {
  // Choose the system configuration service appropriate for each platform.
  ProxyConfigService* proxy_config_service = pc ?
      new ProxyConfigServiceFixed(*pc) :
      CreateSystemProxyConfigService(io_loop);

  ProxyResolver* proxy_resolver;

  if (use_v8_resolver) {
    // Send javascript errors and alerts to LOG(INFO).
    HostResolver* host_resolver = url_request_context->host_resolver();
    ProxyResolverJSBindings* js_bindings =
        ProxyResolverJSBindings::CreateDefault(host_resolver, io_loop);

    proxy_resolver = new ProxyResolverV8(js_bindings);
  } else {
    proxy_resolver = CreateNonV8ProxyResolver();
  }

  // Wrap the (synchronous) ProxyResolver implementation in a single-threaded
  // runner. This will dispatch requests to a threadpool of size 1.
  proxy_resolver = new SingleThreadedProxyResolver(proxy_resolver);

  ProxyService* proxy_service = new ProxyService(
      proxy_config_service, proxy_resolver);

  if (proxy_resolver->expects_pac_bytes()) {
    // Configure PAC script downloads to be issued using |url_request_context|.
    DCHECK(url_request_context);
    proxy_service->SetProxyScriptFetcher(
        ProxyScriptFetcher::Create(url_request_context));
  }

  return proxy_service;
}

// static
ProxyService* ProxyService::CreateFixed(const ProxyConfig& pc) {
  return Create(&pc, false, NULL, NULL);
}

// static
ProxyService* ProxyService::CreateNull() {
  // Use a configuration fetcher and proxy resolver which always fail.
  return new ProxyService(new ProxyConfigServiceNull, new ProxyResolverNull);
}

int ProxyService::ResolveProxy(const GURL& raw_url, ProxyInfo* result,
                               CompletionCallback* callback,
                               PacRequest** pac_request) {
  DCHECK(callback);

  GURL url = SanitizeURLForProxyResolver(raw_url);

  // Check if the request can be completed right away. This is the case when
  // using a direct connection, or when the config is bad.
  UpdateConfigIfOld();
  int rv = TryToCompleteSynchronously(url, result);
  if (rv != ERR_IO_PENDING)
    return rv;

  scoped_refptr<PacRequest> req = new PacRequest(this, url, result, callback);

  bool resolver_is_ready = PrepareResolverForRequests();

  if (resolver_is_ready) {
    // Start the resolve request.
    rv = req->Start();
    if (rv != ERR_IO_PENDING)
      return req->QueryDidComplete(rv);
  }

  DCHECK_EQ(ERR_IO_PENDING, rv);
  DCHECK(!ContainsPendingRequest(req));
  pending_requests_.push_back(req);

  // Completion will be notifed through |callback|, unless the caller cancels
  // the request using |pac_request|.
  if (pac_request)
    *pac_request = req.get();
  return rv;  // ERR_IO_PENDING
}

int ProxyService::TryToCompleteSynchronously(const GURL& url,
                                             ProxyInfo* result) {
  result->config_id_ = config_.id();

  DCHECK(config_.id() != ProxyConfig::INVALID_ID);

  // Fallback to a "direct" (no proxy) connection if the current configuration
  // is known to be bad.
  if (config_is_bad_) {
    // Reset this flag to false in case the ProxyInfo object is being
    // re-used by the caller.
    result->config_was_tried_ = false;
  } else {
    // Remember that we are trying to use the current proxy configuration.
    result->config_was_tried_ = true;

    if (!config_.proxy_rules.empty()) {
      ApplyProxyRules(url, config_.proxy_rules, result);
      return OK;
    }

    if (config_.pac_url.is_valid() || config_.auto_detect) {
      // If we failed to download the PAC script, return the network error
      // from the failed download. This is only going to happen for the first
      // request after the failed download -- after that |config_is_bad_| will
      // be set to true, so we short-cuircuit sooner.
      if (fetched_pac_error_ != OK && !IsFetchingPacScript()) {
        DidCompletePacRequest(fetched_pac_config_id_, fetched_pac_error_);
        return fetched_pac_error_;
      }
      return ERR_IO_PENDING;
    }
  }

  // otherwise, we have no proxy config
  result->UseDirect();
  return OK;
}

void ProxyService::ApplyProxyRules(const GURL& url,
                                   const ProxyConfig::ProxyRules& proxy_rules,
                                   ProxyInfo* result) {
  DCHECK(!proxy_rules.empty());

  if (ShouldBypassProxyForURL(url)) {
    result->UseDirect();
    return;
  }

  switch (proxy_rules.type) {
    case ProxyConfig::ProxyRules::TYPE_SINGLE_PROXY:
      result->UseProxyServer(proxy_rules.single_proxy);
      break;
    case ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME: {
      const ProxyServer* entry = proxy_rules.MapUrlSchemeToProxy(url.scheme());
      if (entry) {
        result->UseProxyServer(*entry);
      } else {
        // We failed to find a matching proxy server for the current URL
        // scheme. Default to direct.
        result->UseDirect();
      }
      break;
    }
    default:
      result->UseDirect();
      NOTREACHED();
      break;
  }
}

ProxyService::~ProxyService() {
  // Cancel any inprogress requests.
  for (PendingRequests::iterator it = pending_requests_.begin();
       it != pending_requests_.end();
       ++it) {
    (*it)->Cancel();
  }
}

void ProxyService::ResumeAllPendingRequests() {
  // Make a copy in case |this| is deleted during the synchronous completion
  // of one of the requests. If |this| is deleted then all of the PacRequest
  // instances will be Cancel()-ed.
  PendingRequests pending_copy = pending_requests_;

  for (PendingRequests::iterator it = pending_copy.begin();
       it != pending_copy.end();
       ++it) {
    PacRequest* req = it->get();
    if (!req->is_started() && !req->was_cancelled())
      req->StartAndComplete();
  }
}

bool ProxyService::PrepareResolverForRequests() {
  // While the PAC script is being downloaded, block requests.
  if (IsFetchingPacScript())
    return false;

  // Check if a new PAC script needs to be downloaded.
  DCHECK(config_.id() != ProxyConfig::INVALID_ID);
  if (resolver_->expects_pac_bytes() &&
      config_.id() != fetched_pac_config_id_) {
    // For auto-detect we use the well known WPAD url.
    GURL pac_url = config_.auto_detect ?
        GURL("http://wpad/wpad.dat") : config_.pac_url;

    in_progress_fetch_config_id_ = config_.id();

    LOG(INFO) << "Starting fetch of PAC script " << pac_url
              << " for config_id=" << in_progress_fetch_config_id_;

    proxy_script_fetcher_->Fetch(
        pac_url, &in_progress_fetch_bytes_, &proxy_script_fetcher_callback_);
    return false;
  }

  // We are good to go.
  return true;
}

void ProxyService::OnScriptFetchCompletion(int result) {
  DCHECK(IsFetchingPacScript());
  DCHECK(resolver_->expects_pac_bytes());

  LOG(INFO) << "Completed PAC script fetch for config_id="
            << in_progress_fetch_config_id_
            << " with error " << ErrorToString(result)
            << ". Fetched a total of " << in_progress_fetch_bytes_.size()
            << " bytes";

  // Notify the ProxyResolver of the new script data (will be empty string if
  // result != OK).
  resolver_->SetPacScriptByData(in_progress_fetch_bytes_);

  fetched_pac_config_id_ = in_progress_fetch_config_id_;
  fetched_pac_error_ = result;
  in_progress_fetch_config_id_ = ProxyConfig::INVALID_ID;
  in_progress_fetch_bytes_.clear();

  // Resume any requests which we had to defer until the PAC script was
  // downloaded.
  ResumeAllPendingRequests();
}

int ProxyService::ReconsiderProxyAfterError(const GURL& url,
                                            ProxyInfo* result,
                                            CompletionCallback* callback,
                                            PacRequest** pac_request) {
  // Check to see if we have a new config since ResolveProxy was called.  We
  // want to re-run ResolveProxy in two cases: 1) we have a new config, or 2) a
  // direct connection failed and we never tried the current config.

  bool re_resolve = result->config_id_ != config_.id();
  if (!re_resolve) {
    UpdateConfig();
    if (result->config_id_ != config_.id()) {
      // A new configuration!
      re_resolve = true;
    } else if (!result->config_was_tried_) {
      // We never tried the proxy configuration since we thought it was bad,
      // but because we failed to establish a connection, let's try the proxy
      // configuration again to see if it will work now.
      config_is_bad_ = false;
      re_resolve = true;
    }
  }
  if (re_resolve) {
    // If we have a new config or the config was never tried, we delete the
    // list of bad proxies and we try again.
    proxy_retry_info_.clear();
    return ResolveProxy(url, result, callback, pac_request);
  }

  // We don't have new proxy settings to try, fallback to the next proxy
  // in the list.
  bool was_direct = result->is_direct();
  if (!was_direct && result->Fallback(&proxy_retry_info_))
    return OK;

  if (!config_.auto_detect && !config_.proxy_rules.empty()) {
    // If auto detect is on, then we should try a DIRECT connection
    // as the attempt to reach the proxy failed.
    return ERR_FAILED;
  }

  // If we already tried a direct connection, then just give up.
  if (was_direct)
    return ERR_FAILED;

  // Try going direct.
  result->UseDirect();
  return OK;
}

void ProxyService::CancelPacRequest(PacRequest* req) {
  DCHECK(req);
  req->Cancel();
  RemovePendingRequest(req);
}

bool ProxyService::ContainsPendingRequest(PacRequest* req) {
  PendingRequests::iterator it = std::find(
      pending_requests_.begin(), pending_requests_.end(), req);
  return pending_requests_.end() != it;
}

void ProxyService::RemovePendingRequest(PacRequest* req) {
  DCHECK(ContainsPendingRequest(req));
  PendingRequests::iterator it = std::find(
      pending_requests_.begin(), pending_requests_.end(), req);
  pending_requests_.erase(it);
}

void ProxyService::SetProxyScriptFetcher(
    ProxyScriptFetcher* proxy_script_fetcher) {
  proxy_script_fetcher_.reset(proxy_script_fetcher);
}

void ProxyService::ResetConfigService(
    ProxyConfigService* new_proxy_config_service) {
  config_service_.reset(new_proxy_config_service);
  UpdateConfig();
}

void ProxyService::DidCompletePacRequest(int config_id, int result_code) {
  // If we get an error that indicates a bad PAC config, then we should
  // remember that, and not try the PAC config again for a while.

  // Our config may have already changed.
  if (result_code == OK || config_id != config_.id())
    return;

  // Remember that this configuration doesn't work.
  config_is_bad_ = true;
}

// static
ProxyConfigService* ProxyService::CreateSystemProxyConfigService(
    MessageLoop* io_loop) {
#if defined(OS_WIN)
  return new ProxyConfigServiceWin();
#elif defined(OS_MACOSX)
  return new ProxyConfigServiceMac();
#elif defined(OS_LINUX)
  ProxyConfigServiceLinux* linux_config_service
      = new ProxyConfigServiceLinux();

  // Assume we got called from the UI loop, which runs the default
  // glib main loop, so the current thread is where we should be
  // running gconf calls from.
  MessageLoop* glib_default_loop = MessageLoopForUI::current();

  // Synchronously fetch the current proxy config (since we are
  // running on glib_default_loop). Additionally register for gconf
  // notifications (delivered in |glib_default_loop|) to keep us
  // updated on when the proxy config has changed.
  linux_config_service->SetupAndFetchInitialConfig(glib_default_loop, io_loop);

  return linux_config_service;
#else
  LOG(WARNING) << "Failed to choose a system proxy settings fetcher "
                  "for this platform.";
  return new ProxyConfigServiceNull();
#endif
}

// static
ProxyResolver* ProxyService::CreateNonV8ProxyResolver() {
#if defined(OS_WIN)
  return new ProxyResolverWinHttp();
#elif defined(OS_MACOSX)
  return new ProxyResolverMac();
#else
  LOG(WARNING) << "PAC support disabled because there is no fallback "
                  "non-V8 implementation";
  return new ProxyResolverNull();
#endif
}

void ProxyService::UpdateConfig() {
  bool is_first_update = !config_has_been_initialized();

  ProxyConfig latest;
  if (config_service_->GetProxyConfig(&latest) != OK) {
    if (is_first_update) {
      // Default to direct-connection if the first fetch fails.
      LOG(INFO) << "Failed initial proxy configuration fetch.";
      SetConfig(ProxyConfig());
    }
    return;
  }
  config_last_update_time_ = TimeTicks::Now();

  if (!is_first_update && latest.Equals(config_))
    return;

  SetConfig(latest);
}

void ProxyService::SetConfig(const ProxyConfig& config) {
  config_ = config;

  // Increment the ID to reflect that the config has changed.
  config_.set_id(next_config_id_++);

  LOG(INFO) << "New proxy configuration was loaded:\n" << config_;

  // Reset state associated with latest config.
  config_is_bad_ = false;
  proxy_retry_info_.clear();

  // Tell the resolver to use the new PAC URL (for resolvers that fetch the
  // PAC script internally).
  // TODO(eroman): this isn't quite right. See http://crbug.com/9985
  if ((config_.pac_url.is_valid() || config_.auto_detect) &&
      !resolver_->expects_pac_bytes()) {
    const GURL pac_url = config_.auto_detect ? GURL() : config_.pac_url;
    resolver_->SetPacScriptByUrl(pac_url);
  }
}

void ProxyService::UpdateConfigIfOld() {
  // The overhead of calling ProxyConfigService::GetProxyConfig is very low.
  const TimeDelta kProxyConfigMaxAge = TimeDelta::FromSeconds(5);

  // Periodically check for a new config.
  if (!config_has_been_initialized() ||
      (TimeTicks::Now() - config_last_update_time_) > kProxyConfigMaxAge)
    UpdateConfig();
}

bool ProxyService::ShouldBypassProxyForURL(const GURL& url) {
  std::string url_domain = url.scheme();
  if (!url_domain.empty())
    url_domain += "://";

  url_domain += url.host();
  // This isn't superfluous; GURL case canonicalization doesn't hit the embedded
  // percent-encoded characters.
  StringToLowerASCII(&url_domain);

  // TODO(eroman): use GetHostAndPort().
  std::string url_domain_and_port = url_domain + ":"
      + IntToString(url.EffectiveIntPort());

  if (config_.proxy_bypass_local_names && IsLocalName(url))
    return true;

  for(std::vector<std::string>::const_iterator i = config_.proxy_bypass.begin();
      i != config_.proxy_bypass.end(); ++i) {
    std::string bypass_url_domain = *i;

    // The proxy server bypass list can contain entities with http/https
    // If no scheme is specified then it indicates that all schemes are
    // allowed for the current entry. For matching this we just use
    // the protocol scheme of the url passed in.
    size_t scheme_colon = bypass_url_domain.find("://");
    if (scheme_colon == std::string::npos) {
      std::string bypass_url_domain_with_scheme = url.scheme();
      scheme_colon = bypass_url_domain_with_scheme.length();
      bypass_url_domain_with_scheme += "://";
      bypass_url_domain_with_scheme += bypass_url_domain;

      bypass_url_domain = bypass_url_domain_with_scheme;
    }
    std::string* url_compare_reference = &url_domain;
    size_t port_colon = bypass_url_domain.rfind(":");
    if (port_colon > scheme_colon) {
      // If our match pattern includes a colon followed by a digit,
      // and either it's preceded by ']' (IPv6 with port)
      // or has no other colon (IPv4),
      // then match against <domain>:<port>.
      // TODO(sdoyon): straighten this out, in particular the IPv6 brackets,
      // and do the parsing in ProxyConfig when we do the CIDR matching
      // mentioned below.
      std::string::const_iterator domain_begin =
          bypass_url_domain.begin() + scheme_colon + 3;  // after ://
      std::string::const_iterator port_iter =
          bypass_url_domain.begin() + port_colon;
      std::string::const_iterator end = bypass_url_domain.end();
      if ((port_iter + 1) < end && IsAsciiDigit(*(port_iter + 1)) &&
          (*(port_iter - 1) == ']' ||
           std::find(domain_begin, port_iter, ':') == port_iter))
        url_compare_reference = &url_domain_and_port;
    }

    StringToLowerASCII(&bypass_url_domain);

    if (MatchPattern(*url_compare_reference, bypass_url_domain))
      return true;

    // Some systems (the Mac, for example) allow CIDR-style specification of
    // proxy bypass for IP-specified hosts (e.g.  "10.0.0.0/8"; see
    // http://www.tcd.ie/iss/internet/osx_proxy.php for a real-world example).
    // That's kinda cool so we'll provide that for everyone.
    // TODO(avi): implement here. See: http://crbug.com/9835.
    // IP addresses ought to be canonicalized for comparison (whether
    // with CIDR, port, or IP address alone).
  }

  return false;
}

// This matches IE's interpretation of the
// "Bypass proxy server for local addresses" settings checkbox. Fully
// qualified domain names or IP addresses are considered non-local,
// regardless of what they map to.
//
// static
bool ProxyService::IsLocalName(const GURL& url) {
  const std::string& host = url.host();
  if (host == "127.0.0.1" || host == "[::1]")
    return true;
  return host.find('.') == std::string::npos;
}

SyncProxyServiceHelper::SyncProxyServiceHelper(MessageLoop* io_message_loop,
                                               ProxyService* proxy_service)
    : io_message_loop_(io_message_loop),
      proxy_service_(proxy_service),
      event_(false, false),
      ALLOW_THIS_IN_INITIALIZER_LIST(callback_(
          this, &SyncProxyServiceHelper::OnCompletion)) {
  DCHECK(io_message_loop_ != MessageLoop::current());
}

int SyncProxyServiceHelper::ResolveProxy(const GURL& url,
                                         ProxyInfo* proxy_info) {
  DCHECK(io_message_loop_ != MessageLoop::current());

  io_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SyncProxyServiceHelper::StartAsyncResolve, url));

  event_.Wait();

  if (result_ == net::OK) {
    *proxy_info = proxy_info_;
  }
  return result_;
}

int SyncProxyServiceHelper::ReconsiderProxyAfterError(const GURL& url,
                                                      ProxyInfo* proxy_info) {
  DCHECK(io_message_loop_ != MessageLoop::current());

  io_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SyncProxyServiceHelper::StartAsyncReconsider, url));

  event_.Wait();

  if (result_ == net::OK) {
    *proxy_info = proxy_info_;
  }
  return result_;
}

void SyncProxyServiceHelper::StartAsyncResolve(const GURL& url) {
  result_ = proxy_service_->ResolveProxy(url, &proxy_info_, &callback_, NULL);
  if (result_ != net::ERR_IO_PENDING) {
    OnCompletion(result_);
  }
}

void SyncProxyServiceHelper::StartAsyncReconsider(const GURL& url) {
  result_ = proxy_service_->ReconsiderProxyAfterError(
      url, &proxy_info_, &callback_, NULL);
  if (result_ != net::ERR_IO_PENDING) {
    OnCompletion(result_);
  }
}

void SyncProxyServiceHelper::OnCompletion(int rv) {
  result_ = rv;
  event_.Signal();
}

}  // namespace net
