// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/shell/renderer/shell_content_renderer_client.h"

#include <string>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "cobalt/shell/common/shell_switches.h"
#include "components/cdm/renderer/external_clear_key_key_system_info.h"
#include "components/network_hints/renderer/web_prescient_networking_impl.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/public/common/pseudonymization_util.h"
#include "content/public/common/web_identity.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/net_errors.h"
#include "ppapi/buildflags/buildflags.h"
#include "sandbox/policy/sandbox.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/web/modules/credentialmanagement/throttle_helper.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "v8/include/v8.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "ppapi/shared_impl/ppapi_switches.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_MOJO_CDM)
#include "base/feature_list.h"
#include "media/base/media_switches.h"
#endif

namespace content {

namespace {

class ShellContentRendererUrlLoaderThrottleProvider
    : public blink::URLLoaderThrottleProvider {
 public:
  std::unique_ptr<URLLoaderThrottleProvider> Clone() override {
    return std::make_unique<ShellContentRendererUrlLoaderThrottleProvider>();
  }

  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      int render_frame_id,
      const blink::WebURLRequest& request) override {
    blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;
    // Workers can call us on a background thread. We don't care about such
    // requests because we purposefully only look at resources from frames
    // that the user can interact with.`
    content::RenderFrame* frame =
        RenderThread::IsMainThread()
            ? content::RenderFrame::FromRoutingID(render_frame_id)
            : nullptr;
    if (frame) {
      auto throttle = content::MaybeCreateIdentityUrlLoaderThrottle(
          base::BindRepeating(blink::SetIdpSigninStatus, frame->GetWebFrame()));
      if (throttle) {
        throttles.push_back(std::move(throttle));
      }
    }

    return throttles;
  }

  void SetOnline(bool is_online) override {}
};

}  // namespace

ShellContentRendererClient::ShellContentRendererClient() {}

ShellContentRendererClient::~ShellContentRendererClient() {}

void ShellContentRendererClient::RenderThreadStarted() {
  web_cache_impl_ = std::make_unique<web_cache::WebCacheImpl>();
}

void ShellContentRendererClient::ExposeInterfacesToBrowser(
    mojo::BinderMap* binders) {
  binders->Add<web_cache::mojom::WebCache>(
      base::BindRepeating(&web_cache::WebCacheImpl::BindReceiver,
                          base::Unretained(web_cache_impl_.get())),
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

void ShellContentRendererClient::PrepareErrorPage(
    RenderFrame* render_frame,
    const blink::WebURLError& error,
    const std::string& http_method,
    content::mojom::AlternativeErrorPageOverrideInfoPtr
        alternative_error_page_info,
    std::string* error_html) {
  if (error_html && error_html->empty()) {
    *error_html =
        "<head><title>Error</title></head><body>Could not load the requested "
        "resource.<br/>Error code: " +
        base::NumberToString(error.reason()) +
        (error.reason() < 0 ? " (" + net::ErrorToString(error.reason()) + ")"
                            : "") +
        "</body>";
  }
}

void ShellContentRendererClient::PrepareErrorPageForHttpStatusError(
    content::RenderFrame* render_frame,
    const blink::WebURLError& error,
    const std::string& http_method,
    int http_status,
    content::mojom::AlternativeErrorPageOverrideInfoPtr
        alternative_error_page_info,
    std::string* error_html) {
  if (error_html) {
    *error_html =
        "<head><title>Error</title></head><body>Server returned HTTP status " +
        base::NumberToString(http_status) + "</body>";
  }
}

std::unique_ptr<blink::URLLoaderThrottleProvider>
ShellContentRendererClient::CreateURLLoaderThrottleProvider(
    blink::URLLoaderThrottleProviderType provider_type) {
  return std::make_unique<ShellContentRendererUrlLoaderThrottleProvider>();
}

#if BUILDFLAG(ENABLE_MOJO_CDM)
void ShellContentRendererClient::GetSupportedKeySystems(
    media::GetSupportedKeySystemsCB cb) {
  media::KeySystemInfos key_systems;
  if (base::FeatureList::IsEnabled(media::kExternalClearKeyForTesting)) {
    key_systems.push_back(
        std::make_unique<cdm::ExternalClearKeyKeySystemInfo>());
  }
  std::move(cb).Run(std::move(key_systems));
}
#endif

std::unique_ptr<blink::WebPrescientNetworking>
ShellContentRendererClient::CreatePrescientNetworking(
    RenderFrame* render_frame) {
  return std::make_unique<network_hints::WebPrescientNetworkingImpl>(
      render_frame);
}

}  // namespace content
