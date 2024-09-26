// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_RESPONSE_INTERCEPTOR_URL_LOADER_THROTTLE_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_RESPONSE_INTERCEPTOR_URL_LOADER_THROTTLE_H_

#include "base/memory/weak_ptr.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

// Used to watch navigation responses to look for mime types that are handled by
// extensions. When it finds such a response, it will intercept it by extracting
// the URLLoader interface pointer. It will create a random string and send that
// to the extension which handles the mime type. It will also write that string
// into the object tag for the plugin, which will cause the pepper plugin to
// make a request for that URL. The renderer would have gotten a
// TransferrableURLLoader that allows it to map from that URL to the original
// URLLoader interface pointer.
class PluginResponseInterceptorURLLoaderThrottle
    : public blink::URLLoaderThrottle {
 public:
  PluginResponseInterceptorURLLoaderThrottle(
      network::mojom::RequestDestination request_destination,
      int frame_tree_node_id);

  PluginResponseInterceptorURLLoaderThrottle(
      const PluginResponseInterceptorURLLoaderThrottle&) = delete;
  PluginResponseInterceptorURLLoaderThrottle& operator=(
      const PluginResponseInterceptorURLLoaderThrottle&) = delete;

  ~PluginResponseInterceptorURLLoaderThrottle() override;

 private:
  // blink::URLLoaderThrottle overrides;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;
  // Resumes loading for an intercepted response. This would give the extension
  // layer chance to initialize its browser side state.
  void ResumeLoad();

  const network::mojom::RequestDestination request_destination_;
  const int frame_tree_node_id_;

  base::WeakPtrFactory<PluginResponseInterceptorURLLoaderThrottle>
      weak_factory_{this};
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_RESPONSE_INTERCEPTOR_URL_LOADER_THROTTLE_H_
