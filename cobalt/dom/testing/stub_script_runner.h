// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_TESTING_STUB_SCRIPT_RUNNER_H_
#define COBALT_DOM_TESTING_STUB_SCRIPT_RUNNER_H_

#include <string>

#include "cobalt/script/script_runner.h"

namespace cobalt {
namespace dom {
namespace testing {

class StubScriptRunner : public script::ScriptRunner {
 public:
  std::string Execute(const std::string& script_utf8,
                      const base::SourceLocation& script_location,
                      bool mute_errors, bool* out_succeeded) override;
};

}  // namespace testing
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TESTING_STUB_SCRIPT_RUNNER_H_
