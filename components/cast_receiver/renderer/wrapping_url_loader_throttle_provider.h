// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_RENDERER_WRAPPING_URL_LOADER_THROTTLE_PROVIDER_H_
#define COMPONENTS_CAST_RECEIVER_RENDERER_WRAPPING_URL_LOADER_THROTTLE_PROVIDER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace cast_receiver {

class UrlRewriteRulesProvider;

class WrappingURLLoaderThrottleProvider
    : public blink::URLLoaderThrottleProvider {
 public:
  // Provides callbacks needed for an instance of this class to function.
  class Client {
   public:
    virtual ~Client();

    // Returns the UrlRewriteRulesProvider associated with RenderFrame with id
    // |render_frame_id|, or nullptr if no such provider exists.
    virtual UrlRewriteRulesProvider* GetUrlRewriteRulesProvider(
        int render_frame_id) = 0;

    // Returns whether |header| is a cors exempt header.
    virtual bool IsCorsExemptHeader(base::StringPiece header) = 0;
  };

  // |client| is expected to outlive this instance.
  //
  // Creating an instance of this class without a |wrapped_provider| results in
  // CreateThrottles() returning url_rewrite::URLLoaderThrottle instances but
  // no other URLLoaderThrottle instances.
  WrappingURLLoaderThrottleProvider(
      std::unique_ptr<blink::URLLoaderThrottleProvider> wrapped_provider,
      Client& client);
  explicit WrappingURLLoaderThrottleProvider(Client& client);
  ~WrappingURLLoaderThrottleProvider() override;

  WrappingURLLoaderThrottleProvider(const WrappingURLLoaderThrottleProvider&) =
      delete;
  WrappingURLLoaderThrottleProvider& operator=(
      const WrappingURLLoaderThrottleProvider&) = delete;

  // blink::URLLoaderThrottleProvider implementation.
  std::unique_ptr<blink::URLLoaderThrottleProvider> Clone() override;
  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      int render_frame_id,
      const blink::WebURLRequest& request) override;
  void SetOnline(bool is_online) override;

 private:
  base::raw_ref<Client> client_;
  std::unique_ptr<blink::URLLoaderThrottleProvider> wrapped_provider_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_RENDERER_WRAPPING_URL_LOADER_THROTTLE_PROVIDER_H_
