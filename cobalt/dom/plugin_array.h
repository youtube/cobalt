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

#ifndef COBALT_DOM_PLUGIN_ARRAY_H_
#define COBALT_DOM_PLUGIN_ARRAY_H_

#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// A PluginArray object represents none, some, or all of the plugins supported
// by the user agent, each of which is represented by a Plugin object.
// Only stub support is currently required.
// https://www.w3.org/html/wg/drafts/html/master/webappapis.html#pluginarray
class PluginArray : public script::Wrappable {
 public:
  PluginArray();

  // Web API: PluginArray
  int length() const;

  DEFINE_WRAPPABLE_TYPE(PluginArray);

 private:
  ~PluginArray() override {}

  DISALLOW_COPY_AND_ASSIGN(PluginArray);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_PLUGIN_ARRAY_H_
