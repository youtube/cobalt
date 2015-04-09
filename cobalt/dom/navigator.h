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

#ifndef DOM_NAVIGATOR_H_
#define DOM_NAVIGATOR_H_

#include <string>

#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The Navigator object represents the identity and state of the user agent (the
// client), and allows Web pages to register themselves as potential protocol
// and content handlers.
//   http://www.w3.org/TR/html5/webappapis.html#navigator
class Navigator : public script::Wrappable {
 public:
  explicit Navigator(const std::string& user_agent);

  // Web API: Navigator
  const std::string& user_agent() const;

  DEFINE_WRAPPABLE_TYPE(Navigator);

 private:
  ~Navigator() OVERRIDE {}

  std::string user_agent_;

  DISALLOW_COPY_AND_ASSIGN(Navigator);
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_NAVIGATOR_H_
