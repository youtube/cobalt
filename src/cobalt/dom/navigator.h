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

#ifndef COBALT_DOM_NAVIGATOR_H_
#define COBALT_DOM_NAVIGATOR_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/mime_type_array.h"
#include "cobalt/dom/plugin_array.h"
#include "cobalt/media_session/media_session.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The Navigator object represents the identity and state of the user agent (the
// client), and allows Web pages to register themselves as potential protocol
// and content handlers.
// https://www.w3.org/TR/html5/webappapis.html#navigator
class Navigator : public script::Wrappable {
 public:
  Navigator(const std::string& user_agent, const std::string& language);

  // Web API: NavigatorID
  const std::string& user_agent() const;

  // Web API: NavigatorLanguage
  const std::string& language() const;

  // Web API: NavigatorStorageUtils
  bool java_enabled() const;

  // Web API: NavigatorPlugins
  bool cookie_enabled() const;
  const scoped_refptr<MimeTypeArray>& mime_types() const;
  const scoped_refptr<PluginArray>& plugins() const;

  const scoped_refptr<cobalt::media_session::MediaSession>& media_session()
      const;

  DEFINE_WRAPPABLE_TYPE(Navigator);

 private:
  ~Navigator() OVERRIDE {}

  std::string user_agent_;
  std::string language_;
  scoped_refptr<MimeTypeArray> mime_types_;
  scoped_refptr<PluginArray> plugins_;
  scoped_refptr<cobalt::media_session::MediaSession> media_session_;

  DISALLOW_COPY_AND_ASSIGN(Navigator);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_NAVIGATOR_H_
