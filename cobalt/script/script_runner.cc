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

#include "cobalt/script/script_runner.h"

#include "base/logging.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/source_code.h"
#if defined(OS_STARBOARD)
#include "starboard/configuration.h"
#if SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
#define HANDLE_CORE_DUMP
#include "base/lazy_instance.h"
#include "starboard/ps4/core_dump_handler.h"
#endif  // SB_HAS(CORE_DUMP_HANDLER_SUPPORT)
#endif  // defined(OS_STARBOARD)

namespace cobalt {
namespace script {

namespace {

#if defined(HANDLE_CORE_DUMP)

class ScriptRunnerLog {
 public:
  ScriptRunnerLog() : success_count_(0), fail_count_(0) {
    SbCoreDumpRegisterHandler(CoreDumpHandler, this);
  }
  ~ScriptRunnerLog() { SbCoreDumpUnregisterHandler(CoreDumpHandler, this); }

  static void CoreDumpHandler(void* context) {
    SbCoreDumpLogInteger(
        "ScriptRunner successful executions",
        static_cast<ScriptRunnerLog*>(context)->success_count_);
    SbCoreDumpLogInteger("ScriptRunner failed executions",
                         static_cast<ScriptRunnerLog*>(context)->fail_count_);
  }

  void IncrementSuccessCount() { success_count_++; }
  void IncrementFailCount() { fail_count_++; }

 private:
  int success_count_;
  int fail_count_;
  DISALLOW_COPY_AND_ASSIGN(ScriptRunnerLog);
};

base::LazyInstance<ScriptRunnerLog> script_runner_log =
    LAZY_INSTANCE_INITIALIZER;

#endif  // defined(HANDLE_CORE_DUMP)

class ScriptRunnerImpl : public ScriptRunner {
 public:
  explicit ScriptRunnerImpl(
      const scoped_refptr<GlobalEnvironment> global_environment)
      : global_environment_(global_environment) {}

  std::string Execute(const std::string& script_utf8,
                      const base::SourceLocation& script_location,
                      bool mute_errors, bool* out_succeeded) override;
  GlobalEnvironment* GetGlobalEnvironment() const override {
    return global_environment_;
  }

 private:
  scoped_refptr<GlobalEnvironment> global_environment_;
};

std::string ScriptRunnerImpl::Execute(
    const std::string& script_utf8, const base::SourceLocation& script_location,
    bool mute_errors, bool* out_succeeded) {
  scoped_refptr<SourceCode> source_code =
      SourceCode::CreateSourceCode(script_utf8, script_location, mute_errors);
  if (out_succeeded) {
    *out_succeeded = false;
  }
  if (source_code == NULL) {
    NOTREACHED() << "Failed to pre-process JavaScript source.";
    return "";
  }
  std::string result;
  if (!global_environment_->EvaluateScript(source_code, &result)) {
    LOG(WARNING) << "Failed to execute JavaScript: " << result;
#if defined(HANDLE_CORE_DUMP)
    script_runner_log.Get().IncrementFailCount();
#endif
    return "";
  }
#if defined(HANDLE_CORE_DUMP)
  script_runner_log.Get().IncrementSuccessCount();
#endif
  if (out_succeeded) {
    *out_succeeded = true;
  }
  return result;
}

}  // namespace

scoped_ptr<ScriptRunner> ScriptRunner::CreateScriptRunner(
    const scoped_refptr<GlobalEnvironment>& global_environment) {
  return scoped_ptr<ScriptRunner>(new ScriptRunnerImpl(global_environment));
}

}  // namespace script
}  // namespace cobalt
