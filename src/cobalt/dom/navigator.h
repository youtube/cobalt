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
#include "cobalt/dom/captions/system_caption_settings.h"
#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/dom/eme/media_key_system_configuration.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/dom/mime_type_array.h"
#include "cobalt/dom/plugin_array.h"
#include "cobalt/media_capture/media_devices.h"
#include "cobalt/media_session/media_session.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The Navigator object represents the identity and state of the user agent (the
// client), and allows Web pages to register themselves as potential protocol
// and content handlers.
// https://www.w3.org/TR/html5/webappapis.html#navigator
class Navigator : public script::Wrappable {
 public:
  Navigator(const std::string& user_agent, const std::string& language,
      scoped_refptr<cobalt::media_session::MediaSession> media_session,
      scoped_refptr<cobalt::dom::captions::SystemCaptionSettings> captions,
      script::ScriptValueFactory* script_value_factory);

  // Web API: NavigatorID
  const std::string& user_agent() const;

  // Web API: NavigatorLanguage
  const std::string& language() const;

  // Web API: NavigatorStorageUtils
  bool java_enabled() const;

  // Web API: NavigatorPlugins
  bool cookie_enabled() const;

  // Web API: MediaDevices
  scoped_refptr<media_capture::MediaDevices> media_devices();

  const scoped_refptr<MimeTypeArray>& mime_types() const;
  const scoped_refptr<PluginArray>& plugins() const;

  const scoped_refptr<cobalt::media_session::MediaSession>& media_session()
      const;

#if defined(COBALT_MEDIA_SOURCE_2016)
  // Web API: extension defined in Encrypted Media Extensions (16 March 2017).
  typedef script::ScriptValue<script::Promise<
      scoped_refptr<script::Wrappable> > > InterfacePromiseValue;
  scoped_ptr<InterfacePromiseValue> RequestMediaKeySystemAccess(
      const std::string& key_system,
      const script::Sequence<eme::MediaKeySystemConfiguration>&
          supported_configurations);
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

  const scoped_refptr<cobalt::dom::captions::SystemCaptionSettings>&
    system_caption_settings() const;

  DEFINE_WRAPPABLE_TYPE(Navigator);
  void TraceMembers(script::Tracer* tracer) override;

 private:
  ~Navigator() override {}

  std::string user_agent_;
  std::string language_;
  scoped_refptr<MimeTypeArray> mime_types_;
  scoped_refptr<PluginArray> plugins_;
  scoped_refptr<cobalt::media_session::MediaSession> media_session_;
  scoped_refptr<cobalt::media_capture::MediaDevices> media_devices_;
  scoped_refptr<cobalt::dom::captions::SystemCaptionSettings>
    system_caption_settings_;
  script::ScriptValueFactory* script_value_factory_;

  DISALLOW_COPY_AND_ASSIGN(Navigator);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_NAVIGATOR_H_
