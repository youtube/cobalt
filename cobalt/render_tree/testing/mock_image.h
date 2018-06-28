// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_TESTING_MOCK_IMAGE_H_
#define COBALT_RENDER_TREE_TESTING_MOCK_IMAGE_H_

#include "cobalt/render_tree/image.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace cobalt {
namespace render_tree {
namespace testing {

class MockImage : public Image {
 public:
  MOCK_CONST_METHOD0(GetSize, const math::Size&());
};

}  // namespace testing
}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_TESTING_MOCK_IMAGE_H_
