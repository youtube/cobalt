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

#include "cobalt/script/execution_state.h"

#include "cobalt/script/global_environment.h"

namespace cobalt {
namespace script {

namespace {

class ExecutionStateImpl : public ExecutionState {
 public:
  explicit ExecutionStateImpl(
      const scoped_refptr<GlobalEnvironment>& global_environment)
      : global_environment_(global_environment) {}

  std::string GetStackTrace() const override;

 private:
  scoped_refptr<GlobalEnvironment> global_environment_;
};

std::string ExecutionStateImpl::GetStackTrace() const {
  return StackTraceToString(
      global_environment_->GetStackTrace(0 /*max_frames*/));
}

}  // namespace

scoped_ptr<ExecutionState> ExecutionState::CreateExecutionState(
    const scoped_refptr<GlobalEnvironment>& global_environment) {
  return scoped_ptr<ExecutionState>(new ExecutionStateImpl(global_environment));
}

}  // namespace script
}  // namespace cobalt
