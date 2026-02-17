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

#ifndef COBALT_SHELL_RENDERER_SHELL_CONTENT_RENDERER_CLIENT_H_
#define COBALT_SHELL_RENDERER_SHELL_CONTENT_RENDERER_CLIENT_H_

#include <memory>
#include <string>

#include "build/build_config.h"
#include "content/public/common/alternative_error_page_override_info.mojom-forward.h"
#include "content/public/renderer/content_renderer_client.h"
#include "media/mojo/buildflags.h"

namespace blink {
class URLLoaderThrottleProvider;
enum class URLLoaderThrottleProviderType;
}  // namespace blink

namespace web_cache {
class WebCacheImpl;
}

namespace content {

class ShellContentRendererClient : public ContentRendererClient {
 public:
  ShellContentRendererClient();
  ~ShellContentRendererClient() override;

  // ContentRendererClient implementation.
  void RenderThreadStarted() override;
  void ExposeInterfacesToBrowser(mojo::BinderMap* binders) override;

  void PrepareErrorPage(RenderFrame* render_frame,
                        const blink::WebURLError& error,
                        const std::string& http_method,
                        content::mojom::AlternativeErrorPageOverrideInfoPtr
                            alternative_error_page_info,
                        std::string* error_html) override;
  void PrepareErrorPageForHttpStatusError(
      content::RenderFrame* render_frame,
      const blink::WebURLError& error,
      const std::string& http_method,
      int http_status,
      content::mojom::AlternativeErrorPageOverrideInfoPtr
          alternative_error_page_info,
      std::string* error_html) override;

  std::unique_ptr<blink::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType provider_type) override;

#if BUILDFLAG(ENABLE_MOJO_CDM)
  void GetSupportedKeySystems(media::GetSupportedKeySystemsCB cb) override;
#endif

  std::unique_ptr<blink::WebPrescientNetworking> CreatePrescientNetworking(
      RenderFrame* render_frame) override;

 private:
  std::unique_ptr<web_cache::WebCacheImpl> web_cache_impl_;
};

}  // namespace content

#endif  // COBALT_SHELL_RENDERER_SHELL_CONTENT_RENDERER_CLIENT_H_
