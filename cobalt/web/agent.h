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

#ifndef COBALT_WEB_AGENT_H_
#define COBALT_WEB_AGENT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/watchdog/watchdog.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/user_agent_platform_info.h"
#include "cobalt/web/web_settings.h"

namespace cobalt {
namespace worker {
class ServiceWorkerContext;
}
namespace web {

class Agent : public base::MessageLoop::DestructionObserver {
 public:
  struct Options {
    typedef base::Callback<scoped_refptr<script::Wrappable>(
        web::EnvironmentSettings*)>
        CreateObjectFunction;
    typedef base::hash_map<std::string, CreateObjectFunction>
        InjectedGlobalObjectAttributes;

    // Specifies the priority of the web agent's thread.  This is the thread
    // that is responsible for executing JavaScript.
    base::ThreadType thread_priority = base::ThreadType::kDefault;

    size_t stack_size = 0;
    // injected_global_attributes contains a map of attributes to be injected
    // into the Web Agent's window object upon construction.  This provides
    // a mechanism to inject custom APIs into the Web Agent object.
    InjectedGlobalObjectAttributes injected_global_object_attributes;

    script::JavaScriptEngine::Options javascript_engine_options;

    web::WebSettings* web_settings = nullptr;
    network::NetworkModule* network_module = nullptr;

    // Optional directory to add to the search path for web files (file://).
    base::FilePath extra_web_file_dir;

    // Optional callback for fetching from cache via h5vcc-cache://.
    base::Callback<int(const std::string&, std::unique_ptr<char[]>*)>
        read_cache_callback;

    worker::ServiceWorkerContext* service_worker_context = nullptr;

    const UserAgentPlatformInfo* platform_info = nullptr;
  };

  typedef base::Callback<void(const script::HeapStatistics&)>
      JavaScriptHeapStatisticsCallback;

  typedef base::Callback<void(Context*)> InitializeCallback;
  explicit Agent(const std::string& name);
  ~Agent();

  void Run(const Options& options, InitializeCallback initialize_callback,
           DestructionObserver* destruction_observer = nullptr);
  void Stop();

  static Context* CreateContext(const std::string& name, const Options& options,
                                base::MessageLoop* message_loop = nullptr);
  static Context* CreateContext(const std::string& name) {
    return CreateContext(name, Options());
  }

  Context* context() {
    DCHECK(context_);
    return context_.get();
  }

  // The message loop this object is running on.
  base::MessageLoop* message_loop() const { return thread_.message_loop(); }

  // Wait until all posted tasks have executed.
  void WaitUntilDone();

  // Post a task that gets the current |script::HeapStatistics| for our
  // |JavaScriptEngine| to the web module thread, and then passes that to
  // |callback|.  Note that |callback| will be called on the main web module
  // thread.  It is the responsibility of |callback| to get back to its
  // intended thread should it want to.
  void RequestJavaScriptHeapStatistics(
      const JavaScriptHeapStatisticsCallback& callback);

  // From base::MessageLoop::DestructionObserver.
  void WillDestroyCurrentMessageLoop() override;

 private:
  // Called by the constructor to create the private implementation object and
  // perform any other initialization required on the dedicated thread.
  void InitializeTaskInThread(const Options& options,
                              InitializeCallback initialize_callback);

  void PingWatchdog();

  // The thread created and owned by this Web Agent.
  // All sub-objects of this object are created on this thread, and all public
  // member functions are re-posted to this thread if necessary.
  base::Thread thread_;

  bool watchdog_registered_ = false;

  std::string watchdog_name_;

  // Interface to the web Context
  std::unique_ptr<Context> context_;

  // This event is used to signal when the destruction observers have been
  // added to the message loop. This is then used in Stop() to ensure that
  // processing doesn't continue until the thread is cleanly shutdown.
  base::WaitableEvent destruction_observer_added_ = {
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED};
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_AGENT_H_
