// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/test_api_standardized_behavior_native_handler.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "extensions/common/switches.h"
#include "extensions/renderer/script_context.h"

namespace extensions {

TestApiStandardizedBehaviorNativeHandler::
    TestApiStandardizedBehaviorNativeHandler(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void TestApiStandardizedBehaviorNativeHandler::AddRoutes() {
  RouteHandlerFunction(
      "GetUseStandardizedApiBehavior", "test",
      base::BindRepeating(&TestApiStandardizedBehaviorNativeHandler::
                              GetUseStandardizedApiBehavior,
                          base::Unretained(this)));
}

void TestApiStandardizedBehaviorNativeHandler::GetUseStandardizedApiBehavior(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  bool use_standardized = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kExtensionTestApiStandardizedBehavior);
  args.GetReturnValue().Set(
      v8::Boolean::New(context()->isolate(), use_standardized));
}

}  // namespace extensions
