// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/webnn_sandbox_init.h"

#include "base/files/file_path.h"
#include "base/native_library.h"
#include "services/webnn/public/cpp/webnn_buildflags.h"

namespace webnn {

#if BUILDFLAG(IS_WIN)
void PreSandboxWebNNInitialization() {
#if BUILDFLAG(WEBNN_USE_WEBGPU_ACCELERATOR)
  // Preload the LiteRT WebGPU Accelerator DLL.
  base::FilePath library_path(
      FILE_PATH_LITERAL("libLiteRtWebGpuAccelerator.dll"));
  base::LoadNativeLibrary(library_path, nullptr);
#endif
}
#endif

}  // namespace webnn
