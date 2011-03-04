// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/proxy/proxy_service.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/values.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_log.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/proxy/init_proxy_resolver.h"
#include "net/proxy/multi_threaded_proxy_resolver.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_script_fetcher.h"
#if defined(OS_WIN)
#include "net/proxy/proxy_config_service_win.h"
#include "net/proxy/proxy_resolver_winhttp.h"
#elif defined(OS_MACOSX)
#include "net/proxy/proxy_config_service_mac.h"
#include "net/proxy/proxy_resolver_mac.h"
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "net/proxy/proxy_config_service_linux.h"
#endif
#include "net/proxy/proxy_resolver.h"
#include "net/proxy/proxy_resolver_js_bindings.h"
#include "net/proxy/proxy_resolver_v8.h"
#include "net/proxy/sync_host_resolver_bridge.h"
#include "net/url_request/url_request_context.h"

using base::TimeDelta;
using base::TimeTicks;

namespace net {

namespace {

const size_t kMaxNumNetLogEntries = 100;
const size_t kDefaultNumPacThreads = 4;

// When the IP address changes we don't immediately re-run proxy auto-config.
// Instead, we  wait for |kNumMillisToStallAfterNetworkChanges| before
// attempting to re-valuate proxy auto-config.
//
// During this time window, any resolve requests sent to the ProxyService will
// be queued. Once we have waited the required amount of them, the proxy
// auto-config step will be run, and the queued requests resumed.
//
// The reason we play this game is that our signal for detecting network
// changes (NetworkChangeNotifier) may fire *before* the system's networking
// dependencies are fully configured. This is a problem since it means if
// we were to run proxy auto-config right away, it could fail due to spurious
// DNS failures. (see http://crbug.com/50779 for more details.)
//
// By adding the wait window, we give things a chance to get properly set up.
// Now by the time we run the proxy-autoconfig there is a lower chance of
// getting transient DNS / connect failures.
//
// Admitedly this is a hack. Ideally we would have NetworkChangeNotifier
// deliver a reliable signal indicating that the network has changed AND is
// ready for action... But until then, we can reduce the likelihood of users
// getting wedged because of proxy detection failures on network switch.
//
// The obvious downside to this strategy is it introduces an additional
// latency when switching networks. This delay shouldn't be too disruptive
// assuming network switches are infrequent and user initiated. However if
// NetworkChangeNotifier delivers network changes more frequently this could
// cause jankiness. (NetworkChangeNotifier broadcasts a change event when ANY
// interface goes up/down. So in theory if the non-primary interface were
// hopping on and off wireless networks our constant delayed reconfiguration
// could add noticeable jank.)
//
// The specific hard-coded wait time below is arbitrary.
// Basically I ran some experiments switching between wireless networks on
// a Linux Ubuntu (Lucid) laptop, and experimentally found this timeout fixes
// things. It is entirely possible that the value is insuficient for other
// setups.
const int64 kNumMillisToStallAfterNetworkChanges = 2000;

// Config getter that always returns direct settings.
class ProxyConfigServiceDirect : public ProxyConfigService {
 public:
  // ProxyConfigService implementation:
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
  virtual bool GetLatestProxyConfig(ProxyConfig* config) {
    *config = ProxyConfig::CreateDirect();
    return true;
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
                             RequestHandle* request,
                             const BoundNetLog& net_log) {
    return ERR_NOT_IMPLEMENTED;
  }

  virtual void CancelRequest(RequestHandle request) {
    NOTREACHED();
  }

  virtual void CancelSetPacScript() {
    NOTREACHED();
  }

  virtual int SetPacScript(
      const scoped_refptr<ProxyResolverScriptData>& /*script_data*/,
      CompletionCallback* /*callback*/) {
    return ERR_NOT_IMPLEMENTED;
  }
};

// ProxyResolver that simulates a PAC script which returns
// |pac_string| for every single URL.
class ProxyResolverFromPacString : public ProxyResolver {
 public:
  ProxyResolverFromPacString(const std::string& pac_string)
      : ProxyResolver(false /*expects_pac_bytes*/),
        pac_string_(pac_string) {}

  virtual int GetProxyForURL(const GURL& url,
                             ProxyInfo* results,
                             CompletionCallback* callback,
                             RequestHandle* request,
                             const BoundNetLog& net_log) {
    results->UsePacString(pac_string_);
    return OK;
  }

  virtual void CancelRequest(RequestHandle request) {
    NOTREACHED();
  }

  virtual void CancelSetPacScript() {
    NOTREACHED();
  }

  virtual int SetPacScript(
      const scoped_refptr<ProxyResolverScriptData>& pac_script,
      CompletionCallback* callback) {
    return OK;
  }

 private:
  const std::string pac_string_;
};

// This factory creates V8ProxyResolvers with appropriate javascript bindings.
class ProxyResolverFactoryForV8 : public ProxyResolverFactory {
 public:
  // |async_host_resolver|, |io_loop| and |net_log| must remain
  // valid for the duration of our lifetime.
  // |async_host_resolver| will only be operated on |io_loop|.
  ProxyResolverFactoryForV8(HostResolver* async_host_resolver,
                            MessageLoop* io_loop,
                            NetLog* net_log)
      : ProxyResolverFactory(true /*expects_pac_bytes*/),
        async_host_resolver_(async_host_resolver),
        io_loop_(io_loop),
        net_log_(net_log) {
  }

  virtual ProxyResolver* CreateProxyResolver() {
    // Create a synchronous host resolver wrapper that operates
    // |async_host_resolver_| on |io_loop_|.
    SyncHostResolverBridge* sync_host_resolver =
        new SyncHostResolverBridge(async_host_resolver_, io_loop_);

    ProxyResolverJSBindings* js_bindings =
        ProxyResolverJSBindings::CreateDefault(sync_host_resolver, net_log_);

    // ProxyResolverV8 takes ownership of |js_bindings|.
    return new ProxyResolverV8(js_bindings);
  }

 private:
  HostResolver* const async_host_resolver_;
  MessageLoop* io_loop_;
  NetLog* net_log_;
};

// Creates ProxyResolvers using a platform-specific implementation.
class ProxyResolverFactoryForSystem : public ProxyResolverFactory {
 public:
  ProxyResolverFactoryForSystem()
      : ProxyResolverFactory(false /*expects_pac_bytes*/) {}

  virtual ProxyResolver* CreateProxyResolver() {
    DCHECK(IsSupported());
#if defined(OS_WIN)
    return new ProxyResolverWinHttp();
#elif defined(OS_MACOSX)
    return new ProxyResolverMac();
#else
    NOTREACHED();
    return NULL;
#endif
  }

  static bool IsSupported() {
#if defined(OS_WIN) || defined(OS_MACOSX)
    return true;
#else
    return false;
#endif
  }
};

// NetLog parameter to describe a proxy configuration change.
class ProxyConfigChangedNetLogParam : public NetLog::EventParameters {
 public:
  ProxyConfigChangedNetLogParam(const ProxyConfig& old_config,
                                const ProxyConfig& new_config)
      : old_config_(old_config),
        new_config_(new_config) {
  }

  virtual Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    // The "old_config" is optional -- the first notification will not have
    // any "previous" configuration.
    if (old_config_.is_valid())
      dict->Set("old_config", old_config_.ToValue());
    dict->Set("new_config", new_config_.ToValue());
    return dict;
  }

 private:
  const ProxyConfig old_config_;
  const ProxyConfig new_config_;
  DISALLOW_COPY_AND_ASSIGN(ProxyConfigChangedNetLogParam);
};

}  // namespace

// ProxyService::PacRequest ---------------------------------------------------

class ProxyService::PacRequest
    : public base::RefCounted<ProxyService::PacRequest> {
 public:
  PacRequest(ProxyService* service,
             const GURL& url,
             ProxyInfo* results,
             CompletionCallback* user_callback,
             const BoundNetLog& net_log)
      : service_(service),
        user_callback_(user_callback),
        ALLOW_THIS_IN_INITIALIZER_LIST(io_callback_(
            this, &PacRequest::QueryComplete)),
        results_(results),
        url_(url),
        resolve_job_(NULL),
        config_id_(ProxyConfig::INVALID_ID),
        net_log_(net_log) {
    DCHECK(user_callback);
  }

  // Starts the resolve proxy request.
  int Start() {
    DCHECK(!was_cancelled());
    DCHECK(!is_started());

    DCHECK(service_->config_.is_valid());

    config_id_ = service_->config_.id();

    return resolver()->GetProxyForURL(
        url_, results_, &io_callback_, &resolve_job_, net_log_);
  }

  bool is_started() const {
    // Note that !! casts to bool. (VS gives a warning otherwise).
    return !!resolve_job_;
  }

  void StartAndCompleteCheckingForSynchronous() {
    int rv = service_->TryToCompleteSynchronously(url_, results_);
    if (rv == ERR_IO_PENDING)
      rv = Start();
    if (rv != ERR_IO_PENDING)
      QueryComplete(rv);
  }

  void CancelResolveJob() {
    DCHECK(is_started());
    // The request may already be running in the resolver.
    resolver()->CancelRequest(resolve_job_);
    resolve_job_ = NULL;
    DCHECK(!is_started());
  }

  void Cancel() {
    net_log_.AddEvent(NetLog::TYPE_CANCELLED, NULL);

    if (is_started())
      CancelResolveJob();

    // Mark as cancelled, to prevent accessing this again later.
    service_ = NULL;
    user_callback_ = NULL;
    results_ = NULL;

    net_log_.EndEvent(NetLog::TYPE_PROXY_SERVICE, NULL);
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

    return service_->DidFinishResolvingProxy(results_, result_code, net_log_);
  }

  BoundNetLog* net_log() { return &net_log_; }

 private:
  friend class base::RefCounted<ProxyService::PacRequest>;

  ~PacRequest() {}

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

  // Note that we don't hold a reference to the ProxyService. Outstanding
  // requests are cancelled during ~ProxyService, so this is guaranteed
  // to be valid throughout our lifetime.
  ProxyService* service_;
  CompletionCallback* user_callback_;
  CompletionCallbackImpl<PacRequest> io_callback_;
  ProxyInfo* results_;
  GURL url_;
  ProxyResolver::RequestHandle resolve_job_;
  ProxyConfig::ID config_id_;  // The config id when the resolve was started.
  BoundNetLog net_log_;
};

// ProxyService ---------------------------------------------------------------

ProxyService::ProxyService(ProxyConfigService* config_service,
                           ProxyResolver* resolver,
                           NetLog* net_log)
    : resolver_(resolver),
      next_config_id_(1),
      ALLOW_THIS_IN_INITIALIZER_LIST(init_proxy_resolver_callback_(
          this, &ProxyService::OnInitProxyResolverComplete)),
      current_state_(STATE_NONE) ,
      net_log_(net_log),
      stall_proxy_auto_config_delay_(
          base::TimeDelta::FromMilliseconds(
              kNumMillisToStallAfterNetworkChanges)) {
  NetworkChangeNotifier::AddIPAddressObserver(this);
  ResetConfigService(config_service);
}

// static
ProxyService* ProxyService::CreateUsingV8ProxyResolver(
    ProxyConfigService* proxy_config_service,
    size_t num_pac_threads,
    ProxyScriptFetcher* proxy_script_fetcher,
    HostResolver* host_resolver,
    NetLog* net_log) {
  DCHECK(proxy_config_service);
  DCHECK(proxy_script_fetcher);
  DCHECK(host_resolver);

  if (num_pac_threads == 0)
    num_pac_threads = kDefaultNumPacThreads;

  ProxyResolverFactory* sync_resolver_factory =
      new ProxyResolverFactoryForV8(
          host_resolver,
          MessageLoop::current(),
          net_log);

  ProxyResolver* proxy_resolver =
      new MultiThreadedProxyResolver(sync_resolver_factory, num_pac_threads);

  ProxyService* proxy_service =
      new ProxyService(proxy_config_service, proxy_resolver, net_log);

  // Configure PAC script downloads to be issued using |proxy_script_fetcher|.
  proxy_service->SetProxyScriptFetcher(proxy_script_fetcher);

  return proxy_service;
}

// static
ProxyService* ProxyService::CreateUsingSystemProxyResolver(
    ProxyConfigService* proxy_config_service,
    size_t num_pac_threads,
    NetLog* net_log) {
  DCHECK(proxy_config_service);

  if (!ProxyResolverFactoryForSystem::IsSupported()) {
    LOG(WARNING) << "PAC support disabled because there is no "
                    "system implementation";
    return CreateWithoutProxyResolver(proxy_config_service, net_log);
  }

  if (num_pac_threads == 0)
    num_pac_threads = kDefaultNumPacThreads;

  ProxyResolver* proxy_resolver = new MultiThreadedProxyResolver(
      new ProxyResolverFactoryForSystem(), num_pac_threads);

  return new ProxyService(proxy_config_service, proxy_resolver, net_log);
}

// static
ProxyService* ProxyService::CreateWithoutProxyResolver(
    ProxyConfigService* proxy_config_service,
    NetLog* net_log) {
  return new ProxyService(proxy_config_service,
                          new ProxyResolverNull(),
                          net_log);
}

// static
ProxyService* ProxyService::CreateFixed(const ProxyConfig& pc) {
  // TODO(eroman): This isn't quite right, won't work if |pc| specifies
  //               a PAC script.
  return CreateUsingSystemProxyResolver(new ProxyConfigServiceFixed(pc),
                                        0, NULL);
}

// static
ProxyService* ProxyService::CreateFixed(const std::string& proxy) {
  net::ProxyConfig proxy_config;
  proxy_config.proxy_rules().ParseFromString(proxy);
  return ProxyService::CreateFixed(proxy_config);
}

// static
ProxyService* ProxyService::CreateDirect() {
  return CreateDirectWithNetLog(NULL);
}

ProxyService* ProxyService::CreateDirectWithNetLog(NetLog* net_log) {
  // Use direct connections.
  return new ProxyService(new ProxyConfigServiceDirect, new ProxyResolverNull,
                          net_log);
}

// static
ProxyService* ProxyService::CreateFixedFromPacResult(
    const std::string& pac_string) {

  // We need the settings to contain an "automatic" setting, otherwise the
  // ProxyResolver dependency we give it will never be used.
  scoped_ptr<ProxyConfigService> proxy_config_service(
      new ProxyConfigServiceFixed(ProxyConfig::CreateAutoDetect()));

  scoped_ptr<ProxyResolver> proxy_resolver(
      new ProxyResolverFromPacString(pac_string));

  return new ProxyService(proxy_config_service.release(),
                          proxy_resolver.release(),
                          NULL);
}

int ProxyService::ResolveProxy(const GURL& raw_url,
                               ProxyInfo* result,
                               CompletionCallback* callback,
                               PacRequest** pac_request,
                               const BoundNetLog& net_log) {
  DCHECK(callback);

  net_log.BeginEvent(NetLog::TYPE_PROXY_SERVICE, NULL);

  config_service_->OnLazyPoll();
  if (current_state_ == STATE_NONE)
    ApplyProxyConfigIfAvailable();

  // Strip away any reference fragments and the username/password, as they
  // are not relevant to proxy resolution.
  GURL url = SimplifyUrlForRequest(raw_url);

  // Check if the request can be completed right away. (This is the case when
  // using a direct connection for example).
  int rv = TryToCompleteSynchronously(url, result);
  if (rv != ERR_IO_PENDING)
    return DidFinishResolvingProxy(result, rv, net_log);

  scoped_refptr<PacRequest> req(
      new PacRequest(this, url, result, callback, net_log));

  if (current_state_ == STATE_READY) {
    // Start the resolve request.
    rv = req->Start();
    if (rv != ERR_IO_PENDING)
      return req->QueryDidComplete(rv);
  } else {
    req->net_log()->BeginEvent(NetLog::TYPE_PROXY_SERVICE_WAITING_FOR_INIT_PAC,
                               NULL);
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
  DCHECK_NE(STATE_NONE, current_state_);

  if (current_state_ != STATE_READY)
    return ERR_IO_PENDING;  // Still initializing.

  DCHECK_NE(config_.id(), ProxyConfig::INVALID_ID);

  if (config_.HasAutomaticSettings())
    return ERR_IO_PENDING;  // Must submit the request to the proxy resolver.

  // Use the manual proxy settings.
  config_.proxy_rules().Apply(url, result);
  result->config_id_ = config_.id();
  return OK;
}

ProxyService::~ProxyService() {
  NetworkChangeNotifier::RemoveIPAddressObserver(this);
  config_service_->RemoveObserver(this);

  // Cancel any inprogress requests.
  for (PendingRequests::iterator it = pending_requests_.begin();
       it != pending_requests_.end();
       ++it) {
    (*it)->Cancel();
  }
}

void ProxyService::SuspendAllPendingRequests() {
  for (PendingRequests::iterator it = pending_requests_.begin();
       it != pending_requests_.end();
       ++it) {
    PacRequest* req = it->get();
    if (req->is_started()) {
      req->CancelResolveJob();

      req->net_log()->BeginEvent(
          NetLog::TYPE_PROXY_SERVICE_WAITING_FOR_INIT_PAC, NULL);
    }
  }
}

void ProxyService::SetReady() {
  DCHECK(!init_proxy_resolver_.get());
  current_state_ = STATE_READY;

  // Make a copy in case |this| is deleted during the synchronous completion
  // of one of the requests. If |this| is deleted then all of the PacRequest
  // instances will be Cancel()-ed.
  PendingRequests pending_copy = pending_requests_;

  for (PendingRequests::iterator it = pending_copy.begin();
       it != pending_copy.end();
       ++it) {
    PacRequest* req = it->get();
    if (!req->is_started() && !req->was_cancelled()) {
      req->net_log()->EndEvent(NetLog::TYPE_PROXY_SERVICE_WAITING_FOR_INIT_PAC,
                               NULL);

      // Note that we re-check for synchronous completion, in case we are
      // no longer using a ProxyResolver (can happen if we fell-back to manual).
      req->StartAndCompleteCheckingForSynchronous();
    }
  }
}

void ProxyService::ApplyProxyConfigIfAvailable() {
  DCHECK_EQ(STATE_NONE, current_state_);

  config_service_->OnLazyPoll();

  // If we have already fetched the configuration, start applying it.
  if (fetched_config_.is_valid()) {
    InitializeUsingLastFetchedConfig();
    return;
  }

  // Otherwise we need to first fetch the configuration.
  current_state_ = STATE_WAITING_FOR_PROXY_CONFIG;

  // Retrieve the current proxy configuration from the ProxyConfigService.
  // If a configuration is not available yet, we will get called back later
  // by our ProxyConfigService::Observer once it changes.
  ProxyConfig config;
  bool has_config = config_service_->GetLatestProxyConfig(&config);
  if (has_config)
    OnProxyConfigChanged(config);
}

void ProxyService::OnInitProxyResolverComplete(int result) {
  DCHECK_EQ(STATE_WAITING_FOR_INIT_PROXY_RESOLVER, current_state_);
  DCHECK(init_proxy_resolver_.get());
  DCHECK(fetched_config_.HasAutomaticSettings());
  init_proxy_resolver_.reset();

  if (result != OK) {
    VLOG(1) << "Failed configuring with PAC script, falling-back to manual "
               "proxy servers.";
    config_ = fetched_config_;
    config_.ClearAutomaticSettings();
  }

  config_.set_id(fetched_config_.id());

  // Resume any requests which we had to defer until the PAC script was
  // downloaded.
  SetReady();
}

int ProxyService::ReconsiderProxyAfterError(const GURL& url,
                                            ProxyInfo* result,
                                            CompletionCallback* callback,
                                            PacRequest** pac_request,
                                            const BoundNetLog& net_log) {
  // Check to see if we have a new config since ResolveProxy was called.  We
  // want to re-run ResolveProxy in two cases: 1) we have a new config, or 2) a
  // direct connection failed and we never tried the current config.

  bool re_resolve = result->config_id_ != config_.id();

  if (re_resolve) {
    // If we have a new config or the config was never tried, we delete the
    // list of bad proxies and we try again.
    proxy_retry_info_.clear();
    return ResolveProxy(url, result, callback, pac_request, net_log);
  }

  // We don't have new proxy settings to try, try to fallback to the next proxy
  // in the list.
  bool did_fallback = result->Fallback(&proxy_retry_info_);

  // Return synchronous failure if there is nothing left to fall-back to.
  // TODO(eroman): This is a yucky API, clean it up.
  return did_fallback ? OK : ERR_FAILED;
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

int ProxyService::DidFinishResolvingProxy(ProxyInfo* result,
                                          int result_code,
                                          const BoundNetLog& net_log) {
  // Log the result of the proxy resolution.
  if (result_code == OK) {
    // When logging all events is enabled, dump the proxy list.
    if (net_log.IsLoggingAllEvents()) {
      net_log.AddEvent(
          NetLog::TYPE_PROXY_SERVICE_RESOLVED_PROXY_LIST,
          make_scoped_refptr(new NetLogStringParameter(
              "pac_string", result->ToPacString())));
    }
    result->DeprioritizeBadProxies(proxy_retry_info_);
  } else {
    net_log.AddEvent(
        NetLog::TYPE_PROXY_SERVICE_RESOLVED_PROXY_LIST,
        make_scoped_refptr(new NetLogIntegerParameter(
            "net_error", result_code)));

    // Fall-back to direct when the proxy resolver fails. This corresponds
    // with a javascript runtime error in the PAC script.
    //
    // This implicit fall-back to direct matches Firefox 3.5 and
    // Internet Explorer 8. For more information, see:
    //
    // http://www.chromium.org/developers/design-documents/proxy-settings-fallback
    result->UseDirect();
    result_code = OK;
  }

  net_log.EndEvent(NetLog::TYPE_PROXY_SERVICE, NULL);
  return result_code;
}

void ProxyService::SetProxyScriptFetcher(
    ProxyScriptFetcher* proxy_script_fetcher) {
  State previous_state = ResetProxyConfig(false);
  proxy_script_fetcher_.reset(proxy_script_fetcher);
  if (previous_state != STATE_NONE)
    ApplyProxyConfigIfAvailable();
}

ProxyScriptFetcher* ProxyService::GetProxyScriptFetcher() const {
  return proxy_script_fetcher_.get();
}

ProxyService::State ProxyService::ResetProxyConfig(bool reset_fetched_config) {
  State previous_state = current_state_;

  proxy_retry_info_.clear();
  init_proxy_resolver_.reset();
  SuspendAllPendingRequests();
  config_ = ProxyConfig();
  if (reset_fetched_config)
    fetched_config_ = ProxyConfig();
  current_state_ = STATE_NONE;

  return previous_state;
}

void ProxyService::ResetConfigService(
    ProxyConfigService* new_proxy_config_service) {
  State previous_state = ResetProxyConfig(true);

  // Release the old configuration service.
  if (config_service_.get())
    config_service_->RemoveObserver(this);

  // Set the new configuration service.
  config_service_.reset(new_proxy_config_service);
  config_service_->AddObserver(this);

  if (previous_state != STATE_NONE)
    ApplyProxyConfigIfAvailable();
}

void ProxyService::PurgeMemory() {
  if (resolver_.get())
    resolver_->PurgeMemory();
}

void ProxyService::ForceReloadProxyConfig() {
  ResetProxyConfig(false);
  ApplyProxyConfigIfAvailable();
}

// static
ProxyConfigService* ProxyService::CreateSystemProxyConfigService(
    MessageLoop* io_loop, MessageLoop* file_loop) {
#if defined(OS_WIN)
  return new ProxyConfigServiceWin();
#elif defined(OS_MACOSX)
  return new ProxyConfigServiceMac(io_loop);
#elif defined(OS_CHROMEOS)
  NOTREACHED() << "ProxyConfigService for ChromeOS should be created in "
               << "chrome_url_request_context.cc::CreateProxyConfigService.";
  return NULL;
#elif defined(OS_LINUX)
  ProxyConfigServiceLinux* linux_config_service
      = new ProxyConfigServiceLinux();

  // Assume we got called from the UI loop, which runs the default
  // glib main loop, so the current thread is where we should be
  // running gconf calls from.
  MessageLoop* glib_default_loop = MessageLoopForUI::current();

  // The file loop should be a MessageLoopForIO on Linux.
  DCHECK_EQ(MessageLoop::TYPE_IO, file_loop->type());

  // Synchronously fetch the current proxy config (since we are
  // running on glib_default_loop). Additionally register for
  // notifications (delivered in either |glib_default_loop| or
  // |file_loop|) to keep us updated when the proxy config changes.
  linux_config_service->SetupAndFetchInitialConfig(glib_default_loop, io_loop,
      static_cast<MessageLoopForIO*>(file_loop));

  return linux_config_service;
#else
  LOG(WARNING) << "Failed to choose a system proxy settings fetcher "
                  "for this platform.";
  return new ProxyConfigServiceNull();
#endif
}

void ProxyService::OnProxyConfigChanged(const ProxyConfig& config) {
  // Emit the proxy settings change to the NetLog stream.
  if (net_log_) {
    scoped_refptr<NetLog::EventParameters> params(
        new ProxyConfigChangedNetLogParam(fetched_config_, config));
    net_log_->AddEntry(net::NetLog::TYPE_PROXY_CONFIG_CHANGED,
                       base::TimeTicks::Now(),
                       NetLog::Source(),
                       NetLog::PHASE_NONE,
                       params);
  }

  // Set the new configuration as the most recently fetched one.
  fetched_config_ = config;
  fetched_config_.set_id(1);  // Needed for a later DCHECK of is_valid().

  InitializeUsingLastFetchedConfig();
}

void ProxyService::InitializeUsingLastFetchedConfig() {
  ResetProxyConfig(false);

  DCHECK(fetched_config_.is_valid());

  // Increment the ID to reflect that the config has changed.
  fetched_config_.set_id(next_config_id_++);

  if (!fetched_config_.HasAutomaticSettings()) {
    config_ = fetched_config_;
    SetReady();
    return;
  }

  // Start downloading + testing the PAC scripts for this new configuration.
  current_state_ = STATE_WAITING_FOR_INIT_PROXY_RESOLVER;

  init_proxy_resolver_.reset(
      new InitProxyResolver(resolver_.get(), proxy_script_fetcher_.get(),
                            net_log_));

  // If we changed networks recently, we should delay running proxy auto-config.
  base::TimeDelta wait_delay =
      stall_proxy_autoconfig_until_ - base::TimeTicks::Now();

  int rv = init_proxy_resolver_->Init(
      fetched_config_, wait_delay, &config_, &init_proxy_resolver_callback_);

  if (rv != ERR_IO_PENDING)
    OnInitProxyResolverComplete(rv);
}

void ProxyService::OnIPAddressChanged() {
  // See the comment block by |kNumMillisToStallAfterNetworkChanges| for info.
  stall_proxy_autoconfig_until_ =
      base::TimeTicks::Now() + stall_proxy_auto_config_delay_;

  State previous_state = ResetProxyConfig(false);
  if (previous_state != STATE_NONE)
    ApplyProxyConfigIfAvailable();
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
                                         ProxyInfo* proxy_info,
                                         const BoundNetLog& net_log) {
  DCHECK(io_message_loop_ != MessageLoop::current());

  io_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SyncProxyServiceHelper::StartAsyncResolve, url, net_log));

  event_.Wait();

  if (result_ == net::OK) {
    *proxy_info = proxy_info_;
  }
  return result_;
}

int SyncProxyServiceHelper::ReconsiderProxyAfterError(
    const GURL& url, ProxyInfo* proxy_info, const BoundNetLog& net_log) {
  DCHECK(io_message_loop_ != MessageLoop::current());

  io_message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
      this, &SyncProxyServiceHelper::StartAsyncReconsider, url, net_log));

  event_.Wait();

  if (result_ == net::OK) {
    *proxy_info = proxy_info_;
  }
  return result_;
}

SyncProxyServiceHelper::~SyncProxyServiceHelper() {}

void SyncProxyServiceHelper::StartAsyncResolve(const GURL& url,
                                               const BoundNetLog& net_log) {
  result_ = proxy_service_->ResolveProxy(
      url, &proxy_info_, &callback_, NULL, net_log);
  if (result_ != net::ERR_IO_PENDING) {
    OnCompletion(result_);
  }
}

void SyncProxyServiceHelper::StartAsyncReconsider(const GURL& url,
                                                  const BoundNetLog& net_log) {
  result_ = proxy_service_->ReconsiderProxyAfterError(
      url, &proxy_info_, &callback_, NULL, net_log);
  if (result_ != net::ERR_IO_PENDING) {
    OnCompletion(result_);
  }
}

void SyncProxyServiceHelper::OnCompletion(int rv) {
  result_ = rv;
  event_.Signal();
}

}  // namespace net
