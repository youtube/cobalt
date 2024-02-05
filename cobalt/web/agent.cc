// Copyright 2022 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/web/agent.h"

#include <map>
#include <memory>
#include <utility>

#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/startup_timer.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/script_loader_factory.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/error_report.h"
#include "cobalt/script/execution_state.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/script_runner.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/watchdog/watchdog.h"
#include "cobalt/web/blob.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/url.h"
#include "cobalt/web/window_or_worker_global_scope.h"
#include "cobalt/worker/service_worker.h"
#include "cobalt/worker/service_worker_context.h"
#include "cobalt/worker/service_worker_object.h"
#include "cobalt/worker/service_worker_registration.h"
#include "cobalt/worker/service_worker_registration_object.h"
#include "cobalt/worker/worker_consts.h"

namespace cobalt {
namespace web {
// Private Web Context implementation. Each Agent owns a single instance of
// this class, which performs all the actual work. All functions of this class
// must be called on the message loop of the Agent thread, so they
// execute synchronously with respect to one another.
namespace {

// The watchdog time interval in microseconds allowed between pings before
// triggering violations.
const int64_t kWatchdogTimeInterval = 15000000;
// The watchdog time wait in microseconds to initially wait before triggering
// violations.
const int64_t kWatchdogTimeWait = 15000000;
// The watchdog time interval in milliseconds between pings.
const int64_t kWatchdogTimePing = 5000;

class Impl : public Context {
 public:
  Impl(const std::string& name, const Agent::Options& options);
  virtual ~Impl();

  void AddEnvironmentSettingsChangeObserver(
      EnvironmentSettingsChangeObserver* observer) final;
  void RemoveEnvironmentSettingsChangeObserver(
      EnvironmentSettingsChangeObserver* observer) final;

  // Context
  //
  void set_message_loop(base::MessageLoop* message_loop) {
    message_loop_ = message_loop;
  }
  base::MessageLoop* message_loop() const final { return message_loop_; }
  void ShutDownJavaScriptEngine() final;
  loader::FetcherFactory* fetcher_factory() const final {
    return fetcher_factory_.get();
  }
  loader::ScriptLoaderFactory* script_loader_factory() const final {
    return script_loader_factory_.get();
  }
  script::JavaScriptEngine* javascript_engine() const final {
    return javascript_engine_.get();
  }
  script::GlobalEnvironment* global_environment() const final {
    return global_environment_.get();
  }
  script::ExecutionState* execution_state() const final {
    return execution_state_.get();
  }
  script::ScriptRunner* script_runner() const final {
    return script_runner_.get();
  }
  Blob::Registry* blob_registry() const final { return blob_registry_.get(); }
  web::WebSettings* web_settings() const final { return web_settings_; }
  network::NetworkModule* network_module() const final {
    DCHECK(fetcher_factory_);
    return fetcher_factory_->network_module();
  }
  worker::ServiceWorkerContext* service_worker_context() const final {
    return service_worker_context_;
  }

  const std::string& name() const final { return name_; };
  void SetupEnvironmentSettings(
      EnvironmentSettings* environment_settings) final {
    for (auto& observer : environment_settings_change_observers_) {
      observer.OnEnvironmentSettingsChanged(!!environment_settings);
    }
    environment_settings_.reset(environment_settings);
    if (environment_settings_) {
      environment_settings_->set_context(this);
    }
  }

  void SetupFinished();

  EnvironmentSettings* environment_settings() const final {
    DCHECK(environment_settings_);
    DCHECK_EQ(environment_settings_->context(), this);
    return environment_settings_.get();
  }

  scoped_refptr<worker::ServiceWorkerRegistration>
  LookupServiceWorkerRegistration(
      worker::ServiceWorkerRegistrationObject* registration) final;
  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#service-worker-registration-creation
  scoped_refptr<worker::ServiceWorkerRegistration> GetServiceWorkerRegistration(
      worker::ServiceWorkerRegistrationObject* registration) final;

  void RemoveServiceWorker(worker::ServiceWorkerObject* worker) final;
  scoped_refptr<worker::ServiceWorker> LookupServiceWorker(
      worker::ServiceWorkerObject* worker) final;
  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#get-the-service-worker-object
  scoped_refptr<worker::ServiceWorker> GetServiceWorker(
      worker::ServiceWorkerObject* worker) final;

  WindowOrWorkerGlobalScope* GetWindowOrWorkerGlobalScope() final;

  const UserAgentPlatformInfo* platform_info() const final {
    return platform_info_;
  }
  std::string GetUserAgent() const final {
    return network_module()->GetUserAgent();
  }
  std::string GetPreferredLanguage() const final {
    return network_module()->preferred_language();
  }

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dfn-control
  bool is_controlled_by(worker::ServiceWorkerObject* worker) const final {
    // When a service worker client has a non-null active service worker, it is
    // said to be controlled by that active service worker.
    return active_service_worker() && (active_service_worker() == worker);
  }

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-environment-active-service-worker
  void set_active_service_worker(
      const scoped_refptr<worker::ServiceWorkerObject>& worker) final {
    active_service_worker_ = worker;
  }
  const scoped_refptr<worker::ServiceWorkerObject>& active_service_worker()
      const final {
    return active_service_worker_;
  }
  scoped_refptr<worker::ServiceWorkerObject>& active_service_worker() final {
    return active_service_worker_;
  }


 private:
  // Injects a list of attributes into the Web Context's global object.
  void InjectGlobalObjectAttributes(
      const Agent::Options::InjectedGlobalObjectAttributes& attributes);

  // Thread checker ensures all calls to the Context are made from the same
  // thread that it is created in.
  THREAD_CHECKER(thread_checker_);

  // The message loop for the web context.
  base::MessageLoop* message_loop_ = nullptr;

  // Name of the web instance.
  std::string name_;

  web::WebSettings* const web_settings_;

  // FetcherFactory that is used to create a fetcher according to URL.
  std::unique_ptr<loader::FetcherFactory> fetcher_factory_;

  // Todo: b/225410588 This is not used by WebModule. Should live in a
  // better place.
  // LoaderFactory that is used to acquire references to resources from a URL.
  std::unique_ptr<loader::ScriptLoaderFactory> script_loader_factory_;

  // JavaScript engine for the browser.
  std::unique_ptr<script::JavaScriptEngine> javascript_engine_;

  // JavaScript Global Object for the browser. There should be one per window,
  // but since there is only one window, we can have one per browser.
  scoped_refptr<script::GlobalEnvironment> global_environment_;

  // Used by |Console| to obtain a JavaScript stack trace.
  std::unique_ptr<script::ExecutionState> execution_state_;

  // Interface for the document to execute JavaScript code.
  std::unique_ptr<script::ScriptRunner> script_runner_;

  // Object to register and retrieve Blob objects with a string key.
  std::unique_ptr<Blob::Registry> blob_registry_;

  // Environment Settings object
  std::unique_ptr<EnvironmentSettings> environment_settings_;

  // The service worker registration object map.
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#environment-settings-object-service-worker-registration-object-map
  std::map<worker::ServiceWorkerRegistrationObject*,
           scoped_refptr<worker::ServiceWorkerRegistration>>
      service_worker_registration_object_map_;

  // The service worker object map.
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#environment-settings-object-service-worker-object-map
  std::map<worker::ServiceWorkerObject*, scoped_refptr<worker::ServiceWorker>>
      service_worker_object_map_;

  worker::ServiceWorkerContext* service_worker_context_;
  const web::UserAgentPlatformInfo* platform_info_;

  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-environment-active-service-worker
  // Note: When a service worker is unregistered from the last client, this will
  // hold the last reference until the current page is unloaded.
  scoped_refptr<worker::ServiceWorkerObject> active_service_worker_;

  base::ObserverList<Context::EnvironmentSettingsChangeObserver>::Unchecked
      environment_settings_change_observers_;
};

void LogScriptError(const base::SourceLocation& source_location,
                    const std::string& error_message) {
  std::string file_name =
      base::FilePath(source_location.file_path).BaseName().value();

  std::stringstream ss;
  base::TimeDelta dt = base::StartupTimer::TimeElapsed();

  // Create the error output.
  // Example:
  //   JS:50250:file.js(29,80): ka(...) is not iterable
  //   JS:<time millis><js-file-name>(<line>,<column>):<message>
  ss << "JS:" << dt.InMilliseconds() << ":" << file_name << "("
     << source_location.line_number << "," << source_location.column_number
     << "): " << error_message << "\n";
  SbLogRaw(ss.str().c_str());
}

Impl::Impl(const std::string& name, const Agent::Options& options)
    : name_(name), web_settings_(options.web_settings) {
  TRACE_EVENT0("cobalt::web", "Agent::Impl::Impl()");
  service_worker_context_ = options.service_worker_context;
  platform_info_ = options.platform_info;
  blob_registry_.reset(new Blob::Registry);

  fetcher_factory_.reset(new loader::FetcherFactory(
      options.network_module, options.extra_web_file_dir,
      URL::MakeBlobResolverCallback(blob_registry_.get()),
      options.read_cache_callback));
  DCHECK(fetcher_factory_);

  script_loader_factory_.reset(new loader::ScriptLoaderFactory(
      name.c_str(), fetcher_factory_.get(), options.thread_priority));
  DCHECK(script_loader_factory_);

  javascript_engine_ =
      script::JavaScriptEngine::CreateEngine(options.javascript_engine_options);
  DCHECK(javascript_engine_);

#if defined(COBALT_ENABLE_JAVASCRIPT_ERROR_LOGGING)
  script::JavaScriptEngine::ErrorHandler error_handler =
      base::Bind(&LogScriptError);
  javascript_engine_->RegisterErrorHandler(error_handler);
#endif

  global_environment_ = javascript_engine_->CreateGlobalEnvironment();
  DCHECK(global_environment_);

  global_environment_->AddRoot(blob_registry_.get());

  execution_state_ =
      script::ExecutionState::CreateExecutionState(global_environment_);
  DCHECK(execution_state_);

  script_runner_ =
      script::ScriptRunner::CreateScriptRunner(global_environment_);
  DCHECK(script_runner_);

  // Schedule the injected global attributes to be added later, to ensure they
  // are added after the global object is created.
  if (!options.injected_global_object_attributes.empty()) {
    DCHECK(base::MessageLoop::current());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&Impl::InjectGlobalObjectAttributes, base::Unretained(this),
                   options.injected_global_object_attributes));
  }
}

void Impl::ShutDownJavaScriptEngine() {
  // TODO: Disentangle shutdown of the JS engine with the various tracking and
  // caching in the WebModule.

  set_active_service_worker(nullptr);
  service_worker_object_map_.clear();
  service_worker_registration_object_map_.clear();

  if (global_environment_) {
    global_environment_->SetReportEvalCallback(base::Closure());
    global_environment_->SetReportErrorCallback(
        script::GlobalEnvironment::ReportErrorCallback());
  }

  SetupEnvironmentSettings(nullptr);
  environment_settings_change_observers_.Clear();
  blob_registry_.reset();
  script_runner_.reset();
  execution_state_.reset();

  // Ensure that global_environment_ is null before it's destroyed.
  scoped_refptr<script::GlobalEnvironment> global_environment(
      std::move(global_environment_));
  DCHECK(!global_environment_);
  global_environment = nullptr;

  javascript_engine_.reset();
  fetcher_factory_.reset();
  script_loader_factory_.reset();
}

Impl::~Impl() { ShutDownJavaScriptEngine(); }

void Impl::AddEnvironmentSettingsChangeObserver(
    Context::EnvironmentSettingsChangeObserver* observer) {
  environment_settings_change_observers_.AddObserver(observer);
}

void Impl::RemoveEnvironmentSettingsChangeObserver(
    Context::EnvironmentSettingsChangeObserver* observer) {
  environment_settings_change_observers_.RemoveObserver(observer);
}

void Impl::SetupFinished() {
  auto* global_scope = GetWindowOrWorkerGlobalScope();
#if !defined(COBALT_FORCE_CSP)
  if (global_scope && global_scope->options().csp_options.enforcement_type ==
                          web::kCspEnforcementDisable) {
    // If CSP is disabled, enable eval(). Otherwise, it will be enabled by
    // a CSP directive.
    global_environment_->EnableEval();
  }
#endif

  if (service_worker_context_) {
    service_worker_context_->RegisterWebContext(this);
    service_worker_context_->SetActiveWorker(environment_settings_.get());
  }
}


void Impl::InjectGlobalObjectAttributes(
    const Agent::Options::InjectedGlobalObjectAttributes& attributes) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(global_environment_);

  for (Agent::Options::InjectedGlobalObjectAttributes::const_iterator iter =
           attributes.begin();
       iter != attributes.end(); ++iter) {
    global_environment_->Bind(iter->first,
                              iter->second.Run(environment_settings()));
  }
}

scoped_refptr<worker::ServiceWorkerRegistration>
Impl::LookupServiceWorkerRegistration(
    worker::ServiceWorkerRegistrationObject* registration) {
  scoped_refptr<worker::ServiceWorkerRegistration> worker_registration;
  if (!registration) {
    return worker_registration;
  }
  auto registration_lookup =
      service_worker_registration_object_map_.find(registration);
  if (registration_lookup != service_worker_registration_object_map_.end()) {
    worker_registration = registration_lookup->second;
  }
  return worker_registration;
}

scoped_refptr<worker::ServiceWorkerRegistration>
Impl::GetServiceWorkerRegistration(
    worker::ServiceWorkerRegistrationObject* registration) {
  // Algorithm for 'get the service worker registration object':
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#get-the-service-worker-registration-object
  scoped_refptr<worker::ServiceWorkerRegistration> worker_registration;
  if (!registration) {
    // Return undefined when registration is null.
    return worker_registration;
  }

  // 1. Let objectMap be environment’s service worker registration object map.
  // 2. If objectMap[registration] does not exist, then:
  auto registration_lookup =
      service_worker_registration_object_map_.find(registration);
  if (registration_lookup == service_worker_registration_object_map_.end()) {
    // 2.1. Let registrationObject be a new ServiceWorkerRegistration in
    // environment’s Realm.
    // 2.2. Set registrationObject’s service worker registration to
    // registration.
    // 2.3. Set registrationObject’s installing attribute to null.
    // 2.4. Set registrationObject’s waiting attribute to null.
    // 2.5. Set registrationObject’s active attribute to null.
    worker_registration = new worker::ServiceWorkerRegistration(
        environment_settings(), registration);

    // 2.6. If registration’s installing worker is not null, then set
    // registrationObject’s installing attribute to the result of getting the
    // service worker object that represents registration’s installing worker in
    // environment.
    if (registration->installing_worker()) {
      worker_registration->set_installing(
          GetServiceWorker(registration->installing_worker()));
    }

    // 2.7. If registration’s waiting worker is not null, then set
    // registrationObject’s waiting attribute to the result of getting the
    // service worker object that represents registration’s waiting worker in
    // environment.
    if (registration->waiting_worker()) {
      worker_registration->set_waiting(
          GetServiceWorker(registration->waiting_worker()));
    }

    // 2.8. If registration’s active worker is not null, then set
    // registrationObject’s active attribute to the result of getting the
    // service worker object that represents registration’s active worker in
    // environment.
    if (registration->active_worker()) {
      worker_registration->set_active(
          GetServiceWorker(registration->active_worker()));
    }

    // 2.9. Set objectMap[registration] to registrationObject.
    service_worker_registration_object_map_.insert(
        std::make_pair(registration, worker_registration));
  } else {
    worker_registration = registration_lookup->second;
  }
  // 3. Return objectMap[registration].
  return worker_registration;
}


void Impl::RemoveServiceWorker(worker::ServiceWorkerObject* worker) {
  service_worker_object_map_.erase(worker);
}

scoped_refptr<worker::ServiceWorker> Impl::LookupServiceWorker(
    worker::ServiceWorkerObject* worker) {
  // Algorithm for 'get the service worker object':
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#get-the-service-worker-object
  scoped_refptr<worker::ServiceWorker> service_worker;

  if (!worker) {
    // Return undefined when worker is null.
    return service_worker;
  }

  // 1. Let objectMap be environment’s service worker object map.
  // 2. If objectMap[serviceWorker] does not exist, then:
  auto worker_lookup = service_worker_object_map_.find(worker);
  if (worker_lookup != service_worker_object_map_.end()) {
    service_worker = worker_lookup->second;
  }
  return service_worker;
}

scoped_refptr<worker::ServiceWorker> Impl::GetServiceWorker(
    worker::ServiceWorkerObject* worker) {
  // Algorithm for 'get the service worker object':
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#get-the-service-worker-object
  scoped_refptr<worker::ServiceWorker> service_worker;

  if (!worker) {
    // Return undefined when worker is null.
    return service_worker;
  }

  // 1. Let objectMap be environment’s service worker object map.
  // 2. If objectMap[serviceWorker] does not exist, then:
  auto worker_lookup = service_worker_object_map_.find(worker);
  if (worker_lookup == service_worker_object_map_.end()) {
    // 2.1. Let serviceWorkerObj be a new ServiceWorker in environment’s Realm,
    // and associate it with serviceWorker.
    // 2.2. Set serviceWorkerObj’s state to serviceWorker’s state.
    service_worker = new worker::ServiceWorker(environment_settings(), worker);

    // 2.3. Set objectMap[serviceWorker] to serviceWorkerObj.
    service_worker_object_map_.insert(std::make_pair(worker, service_worker));
  } else {
    service_worker = worker_lookup->second;
  }

  // 3. Return objectMap[serviceWorker].
  return service_worker;
}

WindowOrWorkerGlobalScope* Impl::GetWindowOrWorkerGlobalScope() {
  script::Wrappable* global_wrappable =
      global_environment_ ? global_environment_->global_wrappable() : nullptr;
  if (!global_wrappable) {
    return nullptr;
  }
  DCHECK(global_wrappable->IsWrappable());
  DCHECK_EQ(script::Wrappable::JSObjectType::kObject,
            global_wrappable->GetJSObjectType());

  return base::polymorphic_downcast<WindowOrWorkerGlobalScope*>(
      global_wrappable);
}

// Signals the given WaitableEvent.
void SignalWaitableEvent(base::WaitableEvent* event) { event->Signal(); }
}  // namespace

void Agent::WillDestroyCurrentMessageLoop() { context_.reset(); }

Agent::Agent(const std::string& name) : thread_(name) {}

void Agent::Stop() {
  DCHECK(message_loop());
  DCHECK(thread_.IsRunning());

  if (context() && context()->service_worker_context()) {
    context()->service_worker_context()->UnregisterWebContext(context());
  }

  watchdog::Watchdog* watchdog = watchdog::Watchdog::GetInstance();
  if (watchdog && watchdog_registered_) {
    watchdog_registered_ = false;
    watchdog->Unregister(watchdog_name_);
  }

  // Ensure that the destruction observer got added before stopping the thread.
  destruction_observer_added_.Wait();
  // Wait for all previously posted tasks to finish.
  thread_.message_loop()->task_runner()->WaitForFence();
  // Stop the thread. This will cause the destruction observer to be notified.
  thread_.Stop();
}

Agent::~Agent() {
  DCHECK(!thread_.IsRunning());
  if (thread_.IsRunning()) {
    Stop();
  }
}

void Agent::Run(const Options& options, InitializeCallback initialize_callback,
                DestructionObserver* destruction_observer) {
  // Start the dedicated thread and create the internal implementation
  // object on that thread.
  if (!thread_.StartWithOptions(base::Thread::Options(options.thread_priority)))
    return;
  DCHECK(message_loop());

  // Registers service worker thread as a watchdog client.
  watchdog::Watchdog* watchdog = watchdog::Watchdog::GetInstance();

  if (watchdog) {
    watchdog_name_ = thread_.thread_name();
    if (watchdog_name_ == worker::WorkerConsts::kServiceWorkerName ||
        watchdog_name_ == worker::WorkerConsts::kDedicatedWorkerName) {
      watchdog_name_ += std::to_string(thread_.GetThreadId());
    }
    watchdog_registered_ = true;
    watchdog->Register(watchdog_name_, watchdog_name_,
                       base::kApplicationStateStarted, kWatchdogTimeInterval,
                       kWatchdogTimeWait, watchdog::PING);
    message_loop()->task_runner()->PostDelayedTask(
        FROM_HERE, base::Bind(&Agent::PingWatchdog, base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(kWatchdogTimePing));
  }

  message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&Agent::InitializeTaskInThread, base::Unretained(this),
                 options, initialize_callback));

  if (destruction_observer) {
    message_loop()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&base::MessageLoop::AddDestructionObserver,
                              base::Unretained(message_loop()),
                              base::Unretained(destruction_observer)));
  }
  // Register as a destruction observer to shut down the Web Agent once all
  // pending tasks have been executed and the message loop is about to be
  // destroyed. This allows us to safely stop the thread, drain the task queue,
  // then destroy the internal components before the message loop is reset.
  // No posted tasks will be executed once the thread is stopped.
  message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&base::MessageLoop::AddDestructionObserver,
                 base::Unretained(message_loop()), base::Unretained(this)));

  // This works almost like a PostBlockingTask, except that any blocking that
  // may be necessary happens when Stop() is called instead of right now.
  message_loop()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&SignalWaitableEvent,
                            base::Unretained(&destruction_observer_added_)));
}

void Agent::InitializeTaskInThread(const Options& options,
                                   InitializeCallback initialize_callback) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop());
  context_.reset(
      CreateContext(thread_.thread_name(), options, thread_.message_loop()));
  initialize_callback.Run(context_.get());
}

Context* Agent::CreateContext(const std::string& name, const Options& options,
                              base::MessageLoop* message_loop) {
  auto* context = new Impl(name, options);
  context->set_message_loop(message_loop);
  return context;
}

void Agent::WaitUntilDone() {
  DCHECK(message_loop());
  if (base::MessageLoop::current() != message_loop()) {
    message_loop()->task_runner()->PostBlockingTask(
        FROM_HERE, base::Bind(&Agent::WaitUntilDone, base::Unretained(this)));
    return;
  }
  DCHECK_EQ(base::MessageLoop::current(), message_loop());
}

void Agent::RequestJavaScriptHeapStatistics(
    const JavaScriptHeapStatisticsCallback& callback) {
  TRACE_EVENT0("cobalt::web", "Agent::RequestJavaScriptHeapStatistics()");
  DCHECK(message_loop());
  if (base::MessageLoop::current() != message_loop()) {
    message_loop()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&Agent::RequestJavaScriptHeapStatistics,
                              base::Unretained(this), callback));
    return;
  }
  script::HeapStatistics heap_statistics =
      context_->javascript_engine()->GetHeapStatistics();
  callback.Run(heap_statistics);
}

// Ping watchdog every 5 second, otherwise a violation will be triggered.
void Agent::PingWatchdog() {
  DCHECK_EQ(base::MessageLoop::current(), message_loop());

  watchdog::Watchdog* watchdog = watchdog::Watchdog::GetInstance();
  // If watchdog is already unregistered or shut down, stop ping watchdog.
  if (!watchdog_registered_ || !watchdog) return;

  watchdog->Ping(watchdog_name_);
  message_loop()->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&Agent::PingWatchdog, base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(kWatchdogTimePing));
}

}  // namespace web
}  // namespace cobalt
