// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBNN_WEBNN_COMPILER_PROCESS_HOST_H_
#define CONTENT_BROWSER_WEBNN_WEBNN_COMPILER_PROCESS_HOST_H_

#include "base/containers/flat_map.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/webnn/public/cpp/context_properties.h"
#include "services/webnn/public/mojom/ep_package_info.mojom.h"
#include "services/webnn/public/mojom/webnn_compiler_service.mojom.h"

namespace content {

// Manages the WebNN Compiler utility process lifecycle and related behavior.
// Examples include process launch-on-demand, disconnect handling, crash
// throttling, and brokered CompilerContext creation. Owned by GpuProcessHost.
//
// This class must be used on the UI thread.
class CONTENT_EXPORT WebNNCompilerProcessHost {
 public:
  using RequestCallback = base::OnceCallback<void(
      mojo::PendingRemote<webnn::mojom::WebNNCompilerContext>,
      mojo::PendingReceiver<webnn::mojom::WebNNModelLoader>)>;

  WebNNCompilerProcessHost();

  WebNNCompilerProcessHost(const WebNNCompilerProcessHost&) = delete;
  WebNNCompilerProcessHost& operator=(const WebNNCompilerProcessHost&) = delete;

  ~WebNNCompilerProcessHost();

  // Requests a new CompilerContext from the Compiler process, launching
  // the process if needed. Calls |callback| with a null remote/receiver
  // on failure.
  void RequestCompilerContext(
      webnn::mojom::CreateContextOptionsPtr context_options,
      const webnn::ContextProperties& context_properties,
      base::flat_map<std::string, webnn::mojom::EpPackageInfoPtr>
          ep_package_info,
      RequestCallback callback);

 private:
  // Launches the WebNN Compiler utility process and returns its mojo remote.
  mojo::Remote<webnn::mojom::WebNNCompilerService> LaunchCompilerProcess();

  void OnDisconnected(uint32_t reason, const std::string& description);

  mojo::Remote<webnn::mojom::WebNNCompilerService> remote_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBNN_WEBNN_COMPILER_PROCESS_HOST_H_
