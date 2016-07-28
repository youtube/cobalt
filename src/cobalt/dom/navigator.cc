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

#include "cobalt/dom/navigator.h"

namespace cobalt {
namespace dom {

Navigator::Navigator(const std::string& user_agent, const std::string& language)
    : user_agent_(user_agent),
      language_(language),
      mime_types_(new MimeTypeArray()),
      plugins_(new PluginArray()) {}

const std::string& Navigator::language() const { return language_; }

const std::string& Navigator::user_agent() const { return user_agent_; }

bool Navigator::java_enabled() const { return false; }

bool Navigator::cookie_enabled() const { return false; }

const scoped_refptr<MimeTypeArray>& Navigator::mime_types() const {
  return mime_types_;
}

const scoped_refptr<PluginArray>& Navigator::plugins() const {
  return plugins_;
}

}  // namespace dom
}  // namespace cobalt
