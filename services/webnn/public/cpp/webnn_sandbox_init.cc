// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/public/cpp/webnn_sandbox_init.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/scoped_native_library.h"
#include "services/webnn/public/cpp/webnn_buildflags.h"

namespace webnn {

#if BUILDFLAG(IS_WIN)
void PreSandboxWebNNInitialization() {
#if BUILDFLAG(WEBNN_USE_LITERT)
  // Preload the LiteRT WebGPU Accelerator DLL.
  base::FilePath library_path(
      FILE_PATH_LITERAL("libLiteRtWebGpuAccelerator.dll"));
  base::ScopedNativeLibrary library(library_path);
  if (!library.is_valid()) {
    LOG(ERROR) << "Failed to preload WebNN LiteRT WebGPU Accelerator: "
               << (library.GetError() ? library.GetError()->ToString()
                                      : "unknown error");
    return;
  }

  // Keep the library loaded in memory for the lifetime of the process.
  [[maybe_unused]] base::NativeLibrary raw_library = library.release();
#endif
}
#endif

}  // namespace webnn
