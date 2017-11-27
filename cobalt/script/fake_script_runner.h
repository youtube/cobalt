// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_FAKE_SCRIPT_RUNNER_H_
#define COBALT_SCRIPT_FAKE_SCRIPT_RUNNER_H_

#include <string>

#include "cobalt/base/source_location.h"
#include "cobalt/script/fake_global_environment.h"
#include "cobalt/script/script_runner.h"

namespace cobalt {
namespace script {

class FakeScriptRunner : public ScriptRunner {
 public:
  FakeScriptRunner() : fake_global_environment_(new FakeGlobalEnvironment()) {}
  std::string Execute(const std::string& /*script_utf8*/,
                      const base::SourceLocation& /*script_location*/,
                      bool /*mute_errors*/, bool* out_succeeded) override {
    if (out_succeeded) {
      *out_succeeded = true;
    }
    return "";
  }
  GlobalEnvironment* GetGlobalEnvironment() const override {
    return fake_global_environment_.get();
  }

 private:
  scoped_refptr<FakeGlobalEnvironment> fake_global_environment_;
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_FAKE_SCRIPT_RUNNER_H_
