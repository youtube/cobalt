/*
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Simple tests to exercise basic NativePromise functionality. Difficult to
// verify functionality without bindings support.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/javascript_engine.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace script {
namespace mozjs {
namespace {
typedef ScriptValue<Promise<void> > VoidPromise;
class NativePromiseTest : public ::testing::Test {
 protected:
  NativePromiseTest()
      : environment_settings_(new script::EnvironmentSettings),
        engine_(script::JavaScriptEngine::CreateEngine()),
        global_environment_(engine_->CreateGlobalEnvironment()) {
    global_environment_->CreateGlobalObject();
  }

  const scoped_ptr<script::EnvironmentSettings> environment_settings_;
  const scoped_ptr<script::JavaScriptEngine> engine_;
  const scoped_refptr<script::GlobalEnvironment> global_environment_;
};

TEST_F(NativePromiseTest, CreateNativePromise) {
  scoped_ptr<VoidPromise> promise;
  promise =
      global_environment_->script_value_factory()->CreateBasicPromise<void>();
}

TEST_F(NativePromiseTest, ResolveNativePromise) {
  scoped_ptr<VoidPromise> promise;
  promise =
      global_environment_->script_value_factory()->CreateBasicPromise<void>();
  VoidPromise::StrongReference reference(*promise.get());
  reference.value().Resolve();
}

TEST_F(NativePromiseTest, RejectNativePromise) {
  scoped_ptr<VoidPromise> promise;
  promise =
      global_environment_->script_value_factory()->CreateBasicPromise<void>();
  VoidPromise::StrongReference reference(*promise.get());
  reference.value().Reject();
}

}  // namespace
}  // namespace mozjs
}  // namespace script
}  // namespace cobalt
