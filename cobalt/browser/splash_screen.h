// Copyright 2015 Google Inc. All Rights Reserved.
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

#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/browser/lifecycle_observer.h"
#include "cobalt/browser/web_module.h"
#include "cobalt/media/media_module_stub.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace browser {

// SplashScreen uses a WebModule to present a splash screen.
//
class SplashScreen : public LifecycleObserver {
 public:
  struct Options {
    Options() : url(kDefaultSplashScreenURL) {}
    static const char kDefaultSplashScreenURL[];
    GURL url;
  };

  SplashScreen(base::ApplicationState initial_application_state,
               const WebModule::OnRenderTreeProducedCallback&
                   render_tree_produced_callback,
               network::NetworkModule* network_module,
               const math::Size& window_dimensions,
               render_tree::ResourceProvider* resource_provider,
               float layout_refresh_rate, const Options& options = Options());
  ~SplashScreen();

  // LifecycleObserver implementation.
  void Start(render_tree::ResourceProvider* resource_provider) OVERRIDE {
    web_module_->Start(resource_provider);
  }
  void Pause() OVERRIDE { web_module_->Pause(); }
  void Unpause() OVERRIDE { web_module_->Unpause(); }
  void Suspend() OVERRIDE { web_module_->Suspend(); }
  void Resume(render_tree::ResourceProvider* resource_provider) OVERRIDE {
    web_module_->Resume(resource_provider);
  }

  // Block the caller until the splash screen is ready to be rendered.
  void WaitUntilReady();

 private:
  void OnRenderTreeProduced(
      const browser::WebModule::LayoutResults& layout_results);

  void OnError(const GURL& /* url */, const std::string& error) {
    is_ready_.Signal();
    LOG(ERROR) << error;
  }

  void OnWindowClosed();

  media::MediaModuleStub stub_media_module_;

  WebModule::OnRenderTreeProducedCallback render_tree_produced_callback_;

  // Signalled once the splash screen has produced its first render tree or
  // an error occurred.
  base::WaitableEvent is_ready_;

  scoped_ptr<WebModule> web_module_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_SPLASH_SCREEN_H_
