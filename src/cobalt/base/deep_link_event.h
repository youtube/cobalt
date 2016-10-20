/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BASE_DEEP_LINK_EVENT_H_
#define COBALT_BASE_DEEP_LINK_EVENT_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/string_util.h"
#include "cobalt/base/event.h"

namespace base {

class DeepLinkEvent : public Event {
 public:
  explicit DeepLinkEvent(const std::string& link) : link_(link) {}
  const std::string& link() const { return link_; }
  bool IsH5vccLink() const {
    return StartsWithASCII(link_, "h5vcc", true);
  }

  BASE_EVENT_SUBCLASS(DeepLinkEvent);

 private:
  std::string link_;
};

}  // namespace base

#endif  // COBALT_BASE_DEEP_LINK_EVENT_H_
