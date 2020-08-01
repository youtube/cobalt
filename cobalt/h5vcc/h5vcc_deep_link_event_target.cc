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

#include "cobalt/h5vcc/h5vcc_deep_link_event_target.h"

namespace cobalt {
namespace h5vcc {

namespace {
const std::string& OnGetArgument(const std::string& link) { return link; }
}  // namespace

H5vccDeepLinkEventTarget::H5vccDeepLinkEventTarget(
    base::Callback<const std::string&()> unconsumed_deep_link_callback)
    : ALLOW_THIS_IN_INITIALIZER_LIST(listeners_(this)),
      unconsumed_deep_link_callback_(unconsumed_deep_link_callback) {}

void H5vccDeepLinkEventTarget::AddListener(
    const H5vccDeepLinkEventTarget::H5vccDeepLinkEventCallbackHolder&
        callback_holder) {
  listeners_.AddListenerAndCallIfFirst(callback_holder,
                                       unconsumed_deep_link_callback_);
}

void H5vccDeepLinkEventTarget::DispatchEvent(const std::string& link) {
  listeners_.DispatchEvent(base::Bind(OnGetArgument, link));
}

}  // namespace h5vcc
}  // namespace cobalt
