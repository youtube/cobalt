// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BASE_DEEP_LINK_EVENT_H_
#define COBALT_BASE_DEEP_LINK_EVENT_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/strings/string_util.h"
#include "cobalt/base/event.h"
#include "cobalt/base/polymorphic_downcast.h"

namespace base {

class DeepLinkEvent : public Event {
 public:
  DeepLinkEvent(const std::string& link, const base::Closure& consumed_callback)
      : link_(link), consumed_callback_(std::move(consumed_callback)) {}
  explicit DeepLinkEvent(const Event* event) {
    const base::DeepLinkEvent* deep_link_event =
        base::polymorphic_downcast<const base::DeepLinkEvent*>(event);
    link_ = deep_link_event->link();
    consumed_callback_ = deep_link_event->callback();
  }
  const std::string& link() const { return link_; }
  base::Closure callback() const { return consumed_callback_; }

  BASE_EVENT_SUBCLASS(DeepLinkEvent);

 private:
  std::string link_;
  base::Closure consumed_callback_;
};

}  // namespace base

#endif  // COBALT_BASE_DEEP_LINK_EVENT_H_
