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

#ifndef COBALT_SCRIPT_FAKE_GLOBAL_ENVIRONMENT_H_
#define COBALT_SCRIPT_FAKE_GLOBAL_ENVIRONMENT_H_

#include <string>
#include <vector>

#include "cobalt/script/global_environment.h"

namespace cobalt {
namespace script {

class FakeGlobalEnvironment : public GlobalEnvironment {
 public:
  void CreateGlobalObject() OVERRIDE {}
  bool EvaluateScript(const scoped_refptr<SourceCode>& /*script_utf8*/,
                      bool /*mute_errors*/,
                      std::string* /*out_result*/) OVERRIDE {
    return false;
  }
  bool EvaluateScript(
      const scoped_refptr<SourceCode>& /*script_utf8*/,
      const scoped_refptr<Wrappable>& /*owning_object*/, bool /*mute_errors*/,
      base::optional<ValueHandleHolder::Reference>* /*out_value_handle*/)
      OVERRIDE {
    return false;
  }
  // False positive lint error.
  std::vector<StackFrame> GetStackTrace(
      int /*max_frames*/) OVERRIDE {  // NOLINT(readability/casting)
    return std::vector<StackFrame>();
  }
  void PreventGarbageCollection(
      const scoped_refptr<Wrappable>& /*wrappable*/) OVERRIDE {}
  void AllowGarbageCollection(
      const scoped_refptr<Wrappable>& /*wrappable*/) OVERRIDE {}
  void DisableEval(const std::string& /*message*/) OVERRIDE {}
  void EnableEval() OVERRIDE {}
  void DisableJit() OVERRIDE {}
  void SetReportEvalCallback(const base::Closure& /*report_eval*/) OVERRIDE {}
  void SetReportErrorCallback(
      const ReportErrorCallback& /*report_eval*/) OVERRIDE {}
  void Bind(const std::string& /*identifier*/,
            const scoped_refptr<Wrappable>& /*impl*/) OVERRIDE {}
  ScriptValueFactory* script_value_factory() { return NULL; }
};

}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_FAKE_GLOBAL_ENVIRONMENT_H_
