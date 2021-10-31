// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_SPLASH_SCREEN_H_
#define COBALT_BROWSER_SPLASH_SCREEN_H_

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/browser/lifecycle_observer.h"
#include "cobalt/browser/splash_screen_cache.h"
#include "cobalt/browser/web_module.h"
#include "cobalt/cssom/viewport_size.h"
#include "cobalt/dom/window.h"
#include "url/gurl.h"

namespace cobalt {
namespace browser {

// SplashScreen uses a WebModule to present a splash screen.
//
class SplashScreen : public LifecycleObserver {
 public:
  SplashScreen(base::ApplicationState initial_application_state,
               const WebModule::OnRenderTreeProducedCallback&
                   render_tree_produced_callback,
               network::NetworkModule* network_module,
               const cssom::ViewportSize& window_dimensions,
               render_tree::ResourceProvider* resource_provider,
               float layout_refresh_rate,
               const base::Optional<GURL>& fallback_splash_screen_url,
               cobalt::browser::SplashScreenCache* splash_screen_cache,
               const base::Callback<void(base::TimeDelta)>&
                   on_splash_screen_shutdown_complete,
               const base::Closure& maybe_freeze_callback);
  ~SplashScreen();

  void SetSize(const cssom::ViewportSize& viewport_size) {
    web_module_->SetSize(viewport_size);
  }

  // LifecycleObserver implementation.
  // LifecycleObserver implementation.
  void Blur(SbTimeMonotonic timestamp) override {
    web_module_->Blur(0);
  }
  void Conceal(render_tree::ResourceProvider* resource_provider,
               SbTimeMonotonic timestamp) override {
    web_module_->Conceal(resource_provider, 0);
  }
  void Freeze(SbTimeMonotonic timestamp) override {
    web_module_->Freeze(0);
  }
  void Unfreeze(render_tree::ResourceProvider* resource_provider,
                SbTimeMonotonic timestamp) override {
    web_module_->Unfreeze(resource_provider, 0);
  }
  void Reveal(render_tree::ResourceProvider* resource_provider,
              SbTimeMonotonic timestamp) override {
    web_module_->Reveal(resource_provider, 0);
  }
  void Focus(SbTimeMonotonic timestamp) override {
    web_module_->Focus(0);
  }

  void ReduceMemory() { web_module_->ReduceMemory(); }

  // This dispatches event beforeunload in the WebModule. If
  // beforeunload has any handlers or listeners, Shutdown waits for
  // window.close to be called or a maximum of kSplashShutdownSeconds
  // before running |on_splash_screen_shutdown_complete_|. If beforeunload has
  // no handlers, |on_splash_screen_shutdown_complete_| is run immediately.
  void Shutdown();

  // Returns whether Shutdown() has been called before or not.
  bool ShutdownSignaled() const { return shutdown_signaled_; }

  WebModule& web_module() { return *web_module_; }

  bool IsReadyToFreeze() { return web_module_->IsReadyToFreeze(); }

 private:
  // Run when window.close() is called by the WebModule.
  void OnWindowClosed();
  void OnWindowClosedInternal();

  WebModule::OnRenderTreeProducedCallback render_tree_produced_callback_;

  std::unique_ptr<WebModule> web_module_;

  // The splash screen runs on this message loop.
  base::MessageLoop* const self_message_loop_;

  // This is called by Shutdown (via window.close) or after
  // the time limit has been exceeded.
  base::CancelableCallback<void(base::TimeDelta)>
      on_splash_screen_shutdown_complete_;

  // True if SplashScreen::Shutdown() has been called.
  bool shutdown_signaled_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_SPLASH_SCREEN_H_
