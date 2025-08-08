// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_RENDERER_CONTENT_RENDERER_CLIENT_MIXINS_IMPL_H_
#define COMPONENTS_CAST_RECEIVER_RENDERER_CONTENT_RENDERER_CLIENT_MIXINS_IMPL_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/strings/string_piece.h"
#include "components/cast_receiver/renderer/public/content_renderer_client_mixins.h"
#include "components/cast_receiver/renderer/wrapping_url_loader_throttle_provider.h"

namespace blink {
class URLLoaderThrottleProvider;
}  // namespace blink

namespace content {
class RenderFrame;
}  // namespace content

namespace cast_receiver {

class UrlRewriteRulesProvider;

// Functions to provide additional ContentRendererClient functionality as
// required for a functioning Cast receiver.
//
// TODO(crbug.com/1359580): Use this class in the
// CastRuntimeContentRendererClient.
class ContentRendererClientMixinsImpl
    : public ContentRendererClientMixins,
      public WrappingURLLoaderThrottleProvider::Client {
 public:
  using ContentRendererClientMixins::IsCorsExemptHeadersCallback;
  explicit ContentRendererClientMixinsImpl(
      IsCorsExemptHeadersCallback is_cors_exempt_header_callback);
  ~ContentRendererClientMixinsImpl() override;

  ContentRendererClientMixinsImpl(const ContentRendererClientMixinsImpl&) =
      delete;
  ContentRendererClientMixinsImpl& operator=(
      const ContentRendererClientMixinsImpl&) = delete;

  // ContentRendererClientMixins implementation.
  void RenderFrameCreated(content::RenderFrame& render_frame) override;
  bool DeferMediaLoad(content::RenderFrame& render_frame,
                      base::OnceClosure closure) override;
  std::unique_ptr<blink::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider() override;
  std::unique_ptr<blink::URLLoaderThrottleProvider>
  ExtendURLLoaderThrottleProvider(
      std::unique_ptr<blink::URLLoaderThrottleProvider> delegated_load_provider)
      override;

 private:
  // Called by UrlRewriteRulesProvider instances as part of frame RenderFrame
  // deletion.
  void OnRenderFrameRemoved(int render_frame_id);

  // WrappingURLLoaderThrottleProvider::Client implementation.
  UrlRewriteRulesProvider* GetUrlRewriteRulesProvider(
      int render_frame_id) override;
  bool IsCorsExemptHeader(base::StringPiece header) override;

  IsCorsExemptHeadersCallback is_cors_exempt_header_callback_;

  base::flat_map<int /* render_frame_id */,
                 std::unique_ptr<UrlRewriteRulesProvider>>
      url_rewrite_rules_providers_;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_RENDERER_CONTENT_RENDERER_CLIENT_MIXINS_IMPL_H_
