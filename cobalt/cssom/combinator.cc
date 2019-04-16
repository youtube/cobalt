// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/cssom/combinator.h"

#include "cobalt/cssom/compound_selector.h"

namespace cobalt {
namespace cssom {

Combinator::Combinator() : left_selector_(NULL) {}

Combinator::~Combinator() {}

void Combinator::set_right_selector(
    std::unique_ptr<CompoundSelector> right_selector) {
  right_selector_ = std::move(right_selector);
}

}  // namespace cssom
}  // namespace cobalt
