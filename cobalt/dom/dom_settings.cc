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

#include "cobalt/dom/dom_settings.h"

#include "cobalt/dom/document.h"

namespace cobalt {
namespace dom {

DOMSettings::DOMSettings(loader::FetcherFactory* fetcher_factory,
                         const scoped_refptr<Window>& window,
                         script::JavaScriptEngine* engine)
    : fetcher_factory_(fetcher_factory),
      window_(window),
      javascript_engine_(engine) {}
DOMSettings::~DOMSettings() {}

GURL DOMSettings::base_url() const {
  return window()->document()->url_as_gurl();
}

}  // namespace dom
}  // namespace cobalt
