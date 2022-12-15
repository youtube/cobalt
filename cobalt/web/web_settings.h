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

#ifndef COBALT_WEB_WEB_SETTINGS_H_
#define COBALT_WEB_WEB_SETTINGS_H_

#include <string>

#include "cobalt/dom/media_settings.h"
#include "cobalt/xhr/xhr_settings.h"
#include "starboard/types.h"

namespace cobalt {
namespace web {

// The `WebSettings` interface and its concrete implementation
// `WebSettingsImpl` provide a convenient way to share browser wide settings
// that can be read by various modules of Cobalt across the main web module and
// all Workers.
//
// These settings can be updated by the web app via `h5vcc.settings` or other
// web interfaces.
//
// The following should be introduced when a group of new settings are needed;
// 1. A new interface close to where the code using these settings to allow C++
//    code read the settings, like the `XhrSettings` used below.
//    A pure virtual function should be added to `WebSettings` to return a
//    pointer to this interface.
// 2. A concrete class inherited from the new interface to allow the settings to
//    be updated.
//    A member variable as an instance of this class should be added to
//    `WebSettingsImpl` and hooked up in `WebSettingsImpl::Set()`.

// A centralized interface to hold all setting interfaces to allow them to be
// accessed.  Any modifications to the settings should be done through the
// concrete class below.
class WebSettings {
 public:
  virtual const dom::MediaSettings& media_settings() const = 0;
  virtual const xhr::XhrSettings& xhr_settings() const = 0;

 protected:
  ~WebSettings() {}
};

// A centralized class to hold all concrete setting classes to allow
// modifications.  It exposes the Set() function to allow updating settings via
// a name.
class WebSettingsImpl final : public web::WebSettings {
 public:
  const dom::MediaSettings& media_settings() const override {
    return media_settings_impl_;
  }

  const xhr::XhrSettingsImpl& xhr_settings() const override {
    return xhr_settings_impl_;
  }

  bool Set(const std::string& name, int32_t value) {
    if (media_settings_impl_.Set(name, value)) {
      return true;
    }

    if (xhr_settings_impl_.Set(name, value)) {
      return true;
    }

    LOG(WARNING) << "Failed to set " << name << " to " << value;
    return false;
  }

 private:
  dom::MediaSettingsImpl media_settings_impl_;
  xhr::XhrSettingsImpl xhr_settings_impl_;
};

}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_WEB_SETTINGS_H_
