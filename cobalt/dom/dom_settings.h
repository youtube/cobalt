/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DOM_DOM_SETTINGS_H_
#define DOM_DOM_SETTINGS_H_

#include "cobalt/dom/window.h"
#include "cobalt/script/environment_settings.h"

namespace cobalt {

namespace loader {
class FetcherFactory;
}
namespace script {
class GlobalObjectProxy;
class JavaScriptEngine;
}
namespace dom {

// A package of global state to be passed around to script objects
// that ask for it in their IDL custom attributes.
class DOMSettings : public script::EnvironmentSettings {
 public:
  DOMSettings(loader::FetcherFactory* fetcher_factory,
              const scoped_refptr<Window>& window,
              script::JavaScriptEngine* engine,
              script::GlobalObjectProxy* global_object_proxy);
  ~DOMSettings() OVERRIDE;

  void set_window(const scoped_refptr<Window>& window) { window_ = window; }
  scoped_refptr<Window> window() const { return window_; }

  void set_fetcher_factory(loader::FetcherFactory* fetcher_factory) {
    fetcher_factory_ = fetcher_factory;
  }
  loader::FetcherFactory* fetcher_factory() const { return fetcher_factory_; }
  script::JavaScriptEngine* javascript_engine() const {
    return javascript_engine_;
  }
  script::GlobalObjectProxy* global_object() const {
    return global_object_proxy_;
  }

  // An absolute URL used to resolve relative URLs.
  virtual GURL base_url() const;

 private:
  loader::FetcherFactory* fetcher_factory_;
  scoped_refptr<dom::Window> window_;
  script::JavaScriptEngine* javascript_engine_;
  script::GlobalObjectProxy* global_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(DOMSettings);
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_DOM_SETTINGS_H_
