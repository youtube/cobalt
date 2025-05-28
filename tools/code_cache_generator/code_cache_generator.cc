// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "gin/v8_initializer.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_bundled_code_cache_generator.h"
#include "tools/v8_context_snapshot/buildflags.h"
#include "v8/include/v8.h"

namespace {

class CodeCacheGenPlatform final : public blink::Platform {
 public:
  // blink::Platform:
  bool DisallowV8FeatureFlagOverrides() const override { return true; }
};

// Returns true if the module code cache was generated and written successfully.
bool GenerateModuleCodeCache(v8::Isolate* isolate,
                             const base::FilePath& in_module_path,
                             const base::FilePath& out_code_cache_path) {
  v8::TryCatch trycatch(isolate);

  // Attempt to read the module file from disk.
  std::string module_file_string;
  if (!base::ReadFileToString(in_module_path, &module_file_string)) {
    LOG(ERROR) << "Error reading file " << in_module_path;
    return false;
  }

  const blink::WebBundledCodeCacheGenerator::SerializedCodeCacheData
      serialized_code_cache_data = blink::WebBundledCodeCacheGenerator::
          CreateSerializedCodeCacheForModule(
              isolate, blink::WebString::FromUTF8(module_file_string));

  // Attempt to write the cached metadata.
  if (!base::WriteFile(out_code_cache_path, serialized_code_cache_data)) {
    LOG(ERROR) << "Failed to write cached metadata to file.";
    return false;
  }

  return true;
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;

  const bool kRemoveRecognizedFlags = true;
  v8::V8::SetFlagsFromCommandLine(&argc, argv, kRemoveRecognizedFlags);
  base::CommandLine::Init(argc, argv);

  // Setup the Blink environment.
  base::SingleThreadTaskExecutor main_thread_task_executor;
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("CompileBytecode");
  mojo::core::Init();

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
#if BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
  gin::V8Initializer::LoadV8Snapshot(
      gin::V8SnapshotFileType::kWithAdditionalContext);
#else
  gin::V8Initializer::LoadV8Snapshot(gin::V8SnapshotFileType::kDefault);
#endif  // BUILDFLAG(USE_V8_CONTEXT_SNAPSHOT)
#endif  // V8_USE_EXTERNAL_STARTUP_DATA

  // Set predictable flag in V8 to generate consistent bytecode.
  static constexpr char kPredictableFlag[] = "--predictable";
  v8::V8::SetFlagsFromString(kPredictableFlag, sizeof(kPredictableFlag) - 1);

  CodeCacheGenPlatform platform;
  mojo::BinderMap binders;
  blink::CreateMainThreadAndInitialize(&platform, &binders);
  auto* isolate = blink::CreateMainThreadIsolate();

  base::FilePath in_module_file_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath("module-path");
  CHECK(!in_module_file_path.empty());
  base::FilePath out_code_cache_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          "code-cache-path");
  CHECK(!out_code_cache_path.empty());

  const int error_code = !GenerateModuleCodeCache(isolate, in_module_file_path,
                                                  out_code_cache_path);

  _exit(error_code);
}
