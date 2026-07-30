/**
* Copyright 2020 Comcast Cable Communications Management, LLC
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* SPDX-License-Identifier: Apache-2.0
*/
#include <interfaces/json/JsonData_Browser.h>
#include <interfaces/json/JsonData_StateControl.h>
#include <interfaces/IDictionary.h>
#include <core/Optional.h>

#include "Cobalt.h"
#include "Module.h"

namespace WPEFramework {
namespace Plugin {

using namespace JsonData::Browser;
using namespace JsonData::StateControl;

// Registration
//
void Cobalt::RegisterAll() {
  // Property < Core::JSON::String >    (_T("url"), &Cobalt::get_url, &Cobalt::set_url, this); /* Browser */
  // Property < Core::JSON::EnumType < VisibilityType >> (_T("visibility"), &Cobalt::get_visibility, &Cobalt::set_visibility, this); /* Browser */
  // Property < Core::JSON::DecUInt32 > (_T("fps"), &Cobalt::get_fps, nullptr, this); /* Browser */
  Register<Core::JSON::String,void>(_T("deeplink"), &Cobalt::endpoint_deeplink, this);
  Property < Core::JSON::EnumType < StateType >> (_T("state"), &Cobalt::get_state, &Cobalt::set_state, this); /* StateControl */
  Property < JsonObject >(_T("accessibility"), &Cobalt::get_accessibility, &Cobalt::set_accessibility, this);
}

void Cobalt::UnregisterAll() {
  Unregister(_T("deeplink"));
  Unregister(_T("state"));
  Unregister(_T("accessibility"));
  // Unregister(_T("fps"));
  // Unregister(_T("visibility"));
  // Unregister(_T("url"));
}

// API implementation
//

// Method: deeplink - Send a deep link to the application
// Return codes:
//  - ERROR_NONE: Success
uint32_t Cobalt::endpoint_deeplink(const Core::JSON::String& param)
{
  uint32_t result = Core::ERROR_INCORRECT_URL;
  if (param.IsSet() && !param.Value().empty()) {
    _cobalt->SetURL(param.Value());  // Abuse SetURL
    result = Core::ERROR_NONE;
  }
  return result;
}


// Property: url - URL loaded in the browser
// Return codes:
//  - ERROR_NONE: Success
uint32_t Cobalt::get_url(Core::JSON::String &response) const /* Browser */
{
  ASSERT(_cobalt != nullptr);
  response = _cobalt->GetURL();
  return Core::ERROR_NONE;
}

// Property: url - URL loaded in the browser
// Return codes:
//  - ERROR_NONE: Success
//  - ERROR_INCORRECT_URL: Incorrect URL given
uint32_t Cobalt::set_url(const Core::JSON::String &param) /* Browser */
{
  ASSERT(_cobalt != nullptr);
  uint32_t result = Core::ERROR_INCORRECT_URL;
  if (param.IsSet() && !param.Value().empty()) {
    _cobalt->SetURL(param.Value());
    result = Core::ERROR_NONE;
  }
  return result;
}

// Property: visibility - Current browser visibility
// Return codes:
//  - ERROR_NONE: Success
uint32_t Cobalt::get_visibility(
  Core::JSON::EnumType<VisibilityType> &response) const /* Browser */ {
  response = (_hidden ? VisibilityType::HIDDEN : VisibilityType::VISIBLE);
  return Core::ERROR_NONE;
}

// Property: visibility - Current browser visibility
// Return codes:
//  - ERROR_NONE: Success
uint32_t Cobalt::set_visibility(
  const Core::JSON::EnumType<VisibilityType> &param) /* Browser */ {
  ASSERT(_cobalt != nullptr);
  uint32_t result = Core::ERROR_BAD_REQUEST;

  if (param.IsSet()) {
    if (param == VisibilityType::VISIBLE) {
      _cobalt->Hide(true);
    } else {
      _cobalt->Hide(false);
    }
    result = Core::ERROR_NONE;
  }
  return result;
}

// Property: fps - Current number of frames per second the browser is rendering
// Return codes:
//  - ERROR_NONE: Success
uint32_t Cobalt::get_fps(Core::JSON::DecUInt32 &response) const /* Browser */
{
  ASSERT(_cobalt != nullptr);
  response = _cobalt->GetFPS();
  return Core::ERROR_NONE;
}

// Property: state - Running state of the service
// Return codes:
//  - ERROR_NONE: Success
uint32_t Cobalt::get_state(Core::JSON::EnumType<StateType> &response) const /* StateControl */
{
  ASSERT(_cobalt != nullptr);

  response.Clear();

  PluginHost::IStateControl *stateControl(_cobalt->QueryInterface<PluginHost::IStateControl>());
  if (stateControl != nullptr) {
    PluginHost::IStateControl::state currentState = stateControl->State();
    switch (currentState) {
      case PluginHost::IStateControl::SUSPENDED:
        response = StateType::SUSPENDED;
        break;
      case PluginHost::IStateControl::RESUMED:
        response = StateType::RESUMED;
        break;
      default:
        break;
    };
    stateControl->Release();
  }

  return response.IsSet() ? Core::ERROR_NONE : Core::ERROR_ILLEGAL_STATE;
}

// Property: state - Running state of the service
// Return codes:
//  - ERROR_NONE: Success
uint32_t Cobalt::set_state(const Core::JSON::EnumType<StateType> &param) /* StateControl */
{
  ASSERT(_cobalt != nullptr);
  uint32_t result = Core::ERROR_BAD_REQUEST;

  if (param.IsSet()) {
    PluginHost::IStateControl *stateControl(
      _cobalt->QueryInterface<PluginHost::IStateControl>());
    if (stateControl != nullptr) {
      Core::OptionalType<PluginHost::IStateControl::command> cmd;

      switch(param.Value()) {
        case StateType::SUSPENDED:
          cmd = PluginHost::IStateControl::SUSPEND;
          break;
        case StateType::RESUMED:
          cmd = PluginHost::IStateControl::RESUME;
          break;
        default:
          break;
      }

      if (cmd.IsSet())
        result = stateControl->Request(cmd.Value());

      stateControl->Release();
    }
    else {
      result = Core::ERROR_ILLEGAL_STATE;
    }
  }
  return result;
}

// Property: accessibility - Accessibility settings
// Return codes:
//  - ERROR_NONE: Success
//  - ERROR_GENERAL: Failed to get settings
uint32_t Cobalt::get_accessibility(JsonObject &response) const
{
  ASSERT(_cobalt != nullptr);
  uint32_t result = Core::ERROR_GENERAL;

  Exchange::IDictionary *dict(
    _cobalt->QueryInterface<Exchange::IDictionary>());
  if (dict == nullptr) {
    SYSLOG(Logging::Error, (_T("IDictionary is not implemented")));
  } else {
    std::string json;
    if (!dict->Get("settings", "accessibility", json)) {
      SYSLOG(Logging::Error, (_T("Cannot get 'accessibility' setting")));
    }
    else if (!response.FromString(json)) {
      SYSLOG(Logging::Error, (_T("Cannot convert to JSON object")));
    }
    else {
      result = Core::ERROR_NONE;
    }
    dict->Release();
  }

  return result;
}

// Property: accessibility - Accessibility settings
// Return codes:
//  - ERROR_NONE: Success
//  - ERROR_BAD_REQUEST: Param is not set
//  - ERROR_GENERAL: Failed to set settings
uint32_t Cobalt::set_accessibility(const JsonObject &param)
{
  ASSERT(_cobalt != nullptr);
  uint32_t result = Core::ERROR_BAD_REQUEST;

  if (param.IsSet()) {
    result = Core::ERROR_GENERAL;
    Exchange::IDictionary *dict(
      _cobalt->QueryInterface<Exchange::IDictionary>());
    if (dict == nullptr) {
      SYSLOG(Logging::Error, (_T("IDictionary is not implemented")));
    } else {
      std::string json;
      if (!param.ToString(json)) {
        SYSLOG(Logging::Error, (_T("Cannot convert to string")));
      }
      else if (!dict->Set("settings", "accessibility", json)) {
        SYSLOG(Logging::Error, (_T("Cannot set 'accessibility' setting")));
      }
      else {
        result = Core::ERROR_NONE;
      }
      dict->Release();
    }
  }
  return result;
}

// Event: urlchange - Signals a URL change in the browser
void Cobalt::event_urlchange(const string &url, const bool &loaded) /* Browser */
{
  UrlchangeParamsData params;
  params.Url = url;
  params.Loaded = loaded;

  Notify(_T("urlchange"), params);
}

// Event: visibilitychange - Signals a visibility change of the browser
void Cobalt::event_visibilitychange(const bool &hidden) /* Browser */
{
  VisibilitychangeParamsData params;
  params.Hidden = hidden;

  Notify(_T("visibilitychange"), params);
}

// Event: statechange - Signals a state change of the service
void Cobalt::event_statechange(const bool &suspended) /* StateControl */
{
  StatechangeParamsData params;
  params.Suspended = suspended;

  Notify(_T("statechange"), params);
}

// Event: closure - Notifies that app requested to close its window
void Cobalt::event_closure() /* Browser */
{
  Notify(_T("closure"));
}

}  // namespace Plugin
}  // namespace WPEFramework
