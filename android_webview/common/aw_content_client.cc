// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_content_client.h"

#include "android_webview/common/aw_features.h"
#include "android_webview/common/aw_media_drm_bridge_client.h"
#include "android_webview/common/aw_resource.h"
#include "android_webview/common/crash_reporter/crash_keys.h"
#include "android_webview/common/url_constants.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "components/embedder_support/origin_trials/origin_trial_policy_impl.h"
#include "components/services/heap_profiling/public/cpp/profiling_client.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_switches.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_util.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace android_webview {

AwContentClient::AwContentClient() = default;
AwContentClient::~AwContentClient() = default;

void AwContentClient::AddAdditionalSchemes(Schemes* schemes) {
  schemes->local_schemes.push_back(url::kContentScheme);
  schemes->secure_schemes.push_back(
      android_webview::kAndroidWebViewVideoPosterScheme);
  schemes->csp_bypassing_schemes.push_back(
      android_webview::kAndroidWebViewVideoPosterScheme);
  schemes->allow_non_standard_schemes_in_origins = true;
}

std::u16string AwContentClient::GetLocalizedString(int message_id) {
  // TODO(boliu): Used only by WebKit, so only bundle those resources for
  // Android WebView.
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece AwContentClient::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) {
  // TODO(boliu): Used only by WebKit, so only bundle those resources for
  // Android WebView.
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* AwContentClient::GetDataResourceBytes(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

std::string AwContentClient::GetDataResourceString(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
      resource_id);
}

void AwContentClient::SetGpuInfo(const gpu::GPUInfo& gpu_info) {
  gpu::SetKeysForCrashLogging(gpu_info);
}

bool AwContentClient::UsingSynchronousCompositing() {
  return true;
}

media::MediaDrmBridgeClient* AwContentClient::GetMediaDrmBridgeClient() {
  return new AwMediaDrmBridgeClient(
      AwResource::GetConfigKeySystemUuidMapping());
}

void AwContentClient::ExposeInterfacesToBrowser(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    mojo::BinderMap* binders) {
  // This creates a process-wide heap_profiling::ProfilingClient that listens
  // for requests from the HeapProfilingService to start profiling the current
  // process.
  binders->Add<heap_profiling::mojom::ProfilingClient>(
      base::BindRepeating(
          [](mojo::PendingReceiver<heap_profiling::mojom::ProfilingClient>
                 receiver) {
            static base::NoDestructor<heap_profiling::ProfilingClient>
                profiling_client;
            profiling_client->BindToInterface(std::move(receiver));
          }),
      io_task_runner);
}

blink::OriginTrialPolicy* AwContentClient::GetOriginTrialPolicy() {
  if (!base::FeatureList::IsEnabled(features::kWebViewOriginTrials)) {
    return nullptr;
  }

  // Prevent initialization race (see crbug.com/721144). There may be a
  // race when the policy is needed for worker startup (which happens on a
  // separate worker thread).
  base::AutoLock auto_lock(origin_trial_policy_lock_);
  if (!origin_trial_policy_)
    origin_trial_policy_ =
        std::make_unique<embedder_support::OriginTrialPolicyImpl>();
  return origin_trial_policy_.get();
}

}  // namespace android_webview
