// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_RENDERER_COBALT_CONTENT_RENDERER_CLIENT_H_
#define COBALT_RENDERER_COBALT_CONTENT_RENDERER_CLIENT_H_

#include <memory>
#include <string>

#include "build/build_config.h"
#include "content/public/common/alternative_error_page_override_info.mojom-forward.h"
#include "content/public/renderer/content_renderer_client.h"
#include "media/mojo/buildflags.h"

// For BUILDFLAG(USE_STARBOARD_MEDIA)
#include "build/build_config.h"

#include "components/js_injection/renderer/js_communication.h"

namespace blink {
class URLLoaderThrottleProvider;
enum class URLLoaderThrottleProviderType;
}  // namespace blink

namespace web_cache {
class WebCacheImpl;
}

namespace cobalt {

class CobaltContentRendererClient : public content::ContentRendererClient {
 public:
  CobaltContentRendererClient();
  ~CobaltContentRendererClient() override;

  // ContentRendererClient implementation.
  void RenderThreadStarted() override;
  void ExposeInterfacesToBrowser(mojo::BinderMap* binders) override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void PrepareErrorPage(content::RenderFrame* render_frame,
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

  void DidInitializeWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context) override;

  std::unique_ptr<blink::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType provider_type) override;

#if BUILDFLAG(ENABLE_MOJO_CDM)
  void GetSupportedKeySystems(media::GetSupportedKeySystemsCB cb) override;
#endif

#if BUILDFLAG(USE_STARBOARD_MEDIA)
  bool IsSupportedAudioType(const media::AudioType& type) override;
  bool IsSupportedVideoType(const media::VideoType& type) override;
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

  // JS Injection hook
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;

  std::unique_ptr<blink::WebPrescientNetworking> CreatePrescientNetworking(
      content::RenderFrame* render_frame) override;

 private:
  std::unique_ptr<web_cache::WebCacheImpl> web_cache_impl_;
};

}  // namespace cobalt

#endif  // COBALT_RENDERER_COBALT_CONTENT_RENDERER_CLIENT_H_
