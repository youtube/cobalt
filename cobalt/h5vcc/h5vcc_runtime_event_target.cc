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

#include "cobalt/h5vcc/h5vcc_runtime_event_target.h"

namespace cobalt {
namespace h5vcc {

namespace {
void OnGetArgument() {}
}  // namespace

H5vccRuntimeEventTarget::H5vccRuntimeEventTarget()
    : ALLOW_THIS_IN_INITIALIZER_LIST(listeners_(this)) {}

void H5vccRuntimeEventTarget::AddListener(
    const H5vccRuntimeEventCallbackHolder& callback_holder) {
  listeners_.AddListener(callback_holder);
}

void H5vccRuntimeEventTarget::DispatchEvent() {
  listeners_.DispatchEvent(base::Bind(OnGetArgument));
}

}  // namespace h5vcc
}  // namespace cobalt
