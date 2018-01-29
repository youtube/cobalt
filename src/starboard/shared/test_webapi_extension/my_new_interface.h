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

#ifndef STARBOARD_SHARED_TEST_WEBAPI_EXTENSION_MY_NEW_INTERFACE_H_
#define STARBOARD_SHARED_TEST_WEBAPI_EXTENSION_MY_NEW_INTERFACE_H_

#include <string>

#include "cobalt/dom/window.h"
#include "cobalt/script/wrappable.h"
#include "starboard/shared/test_webapi_extension/my_new_enum.h"

namespace cobalt {
namespace webapi_extension {

class MyNewInterface : public script::Wrappable {
 public:
  explicit MyNewInterface(const scoped_refptr<dom::Window>& window);

  const std::string& foo() const { return foo_; }
  void set_foo(const std::string& value) { foo_ = value; }

  void SetMyNewEnum(MyNewEnum value) { enum_value_ = value; }
  MyNewEnum GetMyNewEnum() const { return enum_value_; }

  // All types derived from script::Wrappable must have this annotation.
  DEFINE_WRAPPABLE_TYPE(MyNewInterface);

 private:
  // Since script::Wrappable inherits from base::RefCounted<>, we make the
  // destructor private.
  ~MyNewInterface() override;

  std::string foo_;

  MyNewEnum enum_value_;

  DISALLOW_COPY_AND_ASSIGN(MyNewInterface);
};

}  // namespace webapi_extension
}  // namespace cobalt

#endif  // STARBOARD_SHARED_TEST_WEBAPI_EXTENSION_MY_NEW_INTERFACE_H_
