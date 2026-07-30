// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/utility/webnn/webnn_sandbox_init.h"

#include "base/logging.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "services/webnn/ort/platform_functions_ort.h"
#endif

namespace webnn {

#if BUILDFLAG(IS_WIN)

bool PreSandboxInit() {
  // Stage 2 of the WebNN compiler sandbox: load and one-time-initialize
  // the third-party execution-provider preload helper. This runs after
  // the broker has placed the process inside its LPAC (with the
  // chromeInstallFiles impersonation capability, so chrome.exe's install
  // directory is reachable for DLL loads) and just before LowerToken()
  // in utility_main.cc drops the token to USER_LOCKDOWN. See
  // content/browser/service_host/utility_sandbox_delegate_win.cc for
  // the full sandbox configuration.
  //
  // Today the only execution-provider backend is the ONNX Runtime, loaded
  // via PlatformFunctions::EnsureInitialized(). As additional backends
  // (LiteRT, etc.) come online, this function is the extension point for
  // their preload work; the surrounding sandbox lifecycle does not change.
  // TODO(crbug.com/500769395): Drive execution-provider discovery / preload
  // through whichever consolidated helper the WebNN compiler service
  // ultimately exposes.
  if (!ort::PlatformFunctions::EnsureInitialized()) {
    LOG(ERROR) << "[WebNN] Failed to initialize the ONNX Runtime "
                  "execution-provider backend before sandbox lockdown.";
    return false;
  }

  return true;
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace webnn
