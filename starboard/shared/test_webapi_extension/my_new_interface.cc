// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/shared/test_webapi_extension/my_new_interface.h"

namespace cobalt {
namespace webapi_extension {

MyNewInterface::MyNewInterface(const scoped_refptr<dom::Window>& window) {
  UNREFERENCED_PARAMETER(window);
  // Provide an initial value for the enum.
  enum_value_ = kMyNewEnumApples;
}

MyNewInterface::~MyNewInterface() {}

}  // namespace webapi_extension
}  // namespace cobalt
