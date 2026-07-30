// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webnn/webnn_compiler_process_host.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "sandbox/policy/switches.h"
#include "services/webnn/public/cpp/compiler_disconnect_reason.h"
#include "services/webnn/public/mojom/features.mojom-features.h"
#include "services/webnn/public/mojom/webnn_compiler_context.mojom.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_model_loader.mojom.h"

namespace content {

namespace {

// Number of times the Compiler process has crashed unexpectedly.
// Stop relaunching after too many crashes to avoid an infinite loop.
// This is intentionally file-scoped to keep process-wide crash accounting
// across WebNNCompilerProcessHost re-creation within the browser process.
int g_compiler_crash_count = 0;

// Maximum number of unexpected Compiler-process disconnects allowed before
// relaunch is blocked.
constexpr int kMaxCompilerCrashCount = 3;

}  // namespace

WebNNCompilerProcessHost::WebNNCompilerProcessHost() = default;

WebNNCompilerProcessHost::~WebNNCompilerProcessHost() = default;

void WebNNCompilerProcessHost::RequestCompilerContext(
    webnn::mojom::CreateContextOptionsPtr context_options,
    const webnn::ContextProperties& context_properties,
    base::flat_map<std::string, webnn::mojom::EpPackageInfoPtr> ep_package_info,
    RequestCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Launch the Compiler process if not already running.
  if (!remote_.is_bound()) {
    if (g_compiler_crash_count >= kMaxCompilerCrashCount ||
        !base::FeatureList::IsEnabled(
            webnn::mojom::features::kWebNNCompilerProcess) ||
        !base::FeatureList::IsEnabled(
            webnn::mojom::features::kWebNNOnnxRuntime)) {
      std::move(callback).Run(mojo::NullRemote(), mojo::NullReceiver());
      return;
    }

    remote_ = LaunchCompilerProcess();
    remote_.set_disconnect_with_reason_handler(base::BindOnce(
        &WebNNCompilerProcessHost::OnDisconnected, base::Unretained(this)));
  }

  if (!remote_.is_bound()) {
    // Compiler process could not be launched — return nulls.
    std::move(callback).Run(mojo::NullRemote(), mojo::NullReceiver());
    return;
  }

  // Create CompilerContext pipe pair: Renderer gets the remote, Compiler gets
  // the receiver.
  mojo::PendingRemote<webnn::mojom::WebNNCompilerContext>
      compiler_context_remote;
  auto compiler_context_receiver =
      compiler_context_remote.InitWithNewPipeAndPassReceiver();

  // Create ModelLoader pipe pair: Compiler gets the remote (to send compiled
  // models), GPU gets the receiver (to load them).
  mojo::PendingRemote<webnn::mojom::WebNNModelLoader> model_loader_remote;
  auto model_loader_receiver =
      model_loader_remote.InitWithNewPipeAndPassReceiver();

  // Tell the Compiler process to create a per-context compiler state.
  // EP package info is forwarded via mojom so the Compiler process can
  // initialize its ORT Environment with the correct EPs.
  remote_->CreateCompilerContext(std::move(context_options), context_properties,
                                 std::move(ep_package_info),
                                 std::move(model_loader_remote),
                                 std::move(compiler_context_receiver));

  std::move(callback).Run(std::move(compiler_context_remote),
                          std::move(model_loader_receiver));
}

void WebNNCompilerProcessHost::OnDisconnected(uint32_t reason,
                                              const std::string& description) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  remote_.reset();

  // `reason` comes from a less-trusted child process. Verify it matches a
  // known disconnect reason before acting on it; treat any unrecognized
  // value as an unexpected crash.
  switch (reason) {
    case static_cast<uint32_t>(webnn::CompilerDisconnectReason::kIdleShutdown):
      // The compiler process shut down gracefully after all compiler
      // contexts disconnected and the idle timeout elapsed. Not a crash.
      DVLOG(1) << "WebNN Compiler process idle shutdown: " << description;
      return;
    default:
      break;
  }

  // Any other disconnect is unexpected and treated as a crash.
  ++g_compiler_crash_count;
  base::UmaHistogramExactLinear("WebNN.CompilerProcess.CrashCount",
                                g_compiler_crash_count,
                                kMaxCompilerCrashCount + 1);

  LOG(ERROR) << "WebNN Compiler process disconnected unexpectedly (count: "
             << g_compiler_crash_count << "): " << description;
}

mojo::Remote<webnn::mojom::WebNNCompilerService>
WebNNCompilerProcessHost::LaunchCompilerProcess() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ServiceProcessHost::Options options;
  options.WithDisplayName("WebNN Compiler");

  // Only bypass MITIGATION_FORCE_MS_SIGNED_BINS when the browser was launched
  // with --allow-third-party-modules (for testing with non-MS-signed DLLs).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          sandbox::policy::switches::kAllowThirdPartyModules)) {
    options.WithExtraCommandLineSwitches(
        {sandbox::policy::switches::kAllowThirdPartyModules});
  }

  return ServiceProcessHost::Launch<webnn::mojom::WebNNCompilerService>(
      std::move(options));
}

}  // namespace content
