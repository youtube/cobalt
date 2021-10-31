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

#ifndef COBALT_WEBDRIVER_KEYBOARD_H_
#define COBALT_WEBDRIVER_KEYBOARD_H_

#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/keyboard_event.h"
#include "cobalt/dom/keyboard_event_init.h"

namespace cobalt {
namespace webdriver {

class Keyboard {
 public:
  enum TerminationBehaviour {
    kReleaseModifiers,
    kKeepModifiers,
  };
  typedef std::vector<std::pair<base::Token, dom::KeyboardEventInit> >
      KeyboardEventVector;
  static void TranslateToKeyEvents(const std::string& utf8_keys,
                                   TerminationBehaviour termination_behaviour,
                                   KeyboardEventVector* out_events);
};

}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_KEYBOARD_H_
