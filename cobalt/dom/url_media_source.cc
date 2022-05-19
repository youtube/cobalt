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

#include "cobalt/web/url.h"

#include "base/guid.h"
#include "base/logging.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/media_source.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"
#include "url/gurl.h"

namespace cobalt {
namespace dom {

// extern
void RegisterMediaSourceObjectURL(
    script::EnvironmentSettings* environment_settings,
    const std::string& blob_url,
    const scoped_refptr<dom::MediaSource>& media_source) {
  dom::DOMSettings* dom_settings =
      base::polymorphic_downcast<dom::DOMSettings*>(environment_settings);
  DCHECK(dom_settings);
  DCHECK(dom_settings->media_source_registry());
  dom_settings->media_source_registry()->Register(blob_url, media_source);
}

// extern
bool UnregisterMediaSourceObjectURL(
    script::EnvironmentSettings* environment_settings, const std::string& url) {
  dom::DOMSettings* dom_settings =
      base::polymorphic_downcast<dom::DOMSettings*>(environment_settings);
  DCHECK(dom_settings);
  DCHECK(dom_settings->media_source_registry());
  return dom_settings->media_source_registry()->Unregister(url);
}

}  // namespace dom
}  // namespace cobalt
