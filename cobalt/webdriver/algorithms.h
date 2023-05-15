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

#ifndef COBALT_WEBDRIVER_ALGORITHMS_H_
#define COBALT_WEBDRIVER_ALGORITHMS_H_

#include <string>

#include "cobalt/dom/element.h"

namespace cobalt {
namespace webdriver {
namespace algorithms {

// Implementation of getElementText algorithm.
// https://www.w3.org/TR/2015/WD-webdriver-20150808/#getelementtext
// The spec is not totally clear and, according to comments on the spec, does
// not exactly match the behavior of existing WebDriver implementations. This
// implementation will follow the de-facto standards where they differ.
std::string GetElementText(dom::Element* element);

// https://www.w3.org/TR/2015/WD-webdriver-20150808/#element-displayedness
// The spec does not exactly match the behavior of existing WebDriver
// implementations. Consistency with existing implementations will be preferred
// over strict conformance to the draft spec.
bool IsDisplayed(dom::Element* element);

}  // namespace algorithms
}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_ALGORITHMS_H_
