// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_RENDER_FRAME_AUDIO_OUTPUT_STREAM_FACTORY_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_RENDER_FRAME_AUDIO_OUTPUT_STREAM_FACTORY_H_

#include <cstddef>
#include <memory>

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/media/renderer_audio_output_stream_factory.mojom.h"

namespace media {
class AudioSystem;
}  // namespace media

namespace content {

class MediaStreamManager;
class RenderFrameHostImpl;

// This class is related to ForwardingAudioStreamFactory as follows:
//
//     WebContentsImpl       <--        RenderFrameHostImpl
//           ^                                  ^
//           |                                  |
//  ForwardingAudioStreamFactory   RenderFrameAudioOutputStreamFactory
//           ^                                  ^
//           |                                  |
//      FASF::Core           <--          RFAOSF::Core
//
// Both FASF::Core and RFAOSF::Core live on (and are destructed on) the IO
// thread. A weak pointer to ForwardingAudioStreamFactory is used since
// WebContentsImpl is sometimes destructed shortly before RenderFrameHostImpl.

// This class takes care of stream requests from a render frame. It verifies
// that the stream creation is allowed and then forwards the request to the
// appropriate ForwardingAudioStreamFactory. It should be constructed and
// destructed on the UI thread, but will process Mojo messages on the IO thread
// unless it is running in the restricted mode.
class CONTENT_EXPORT RenderFrameAudioOutputStreamFactory final {
 public:
  // Initializing it with a non-null `restricted_callback` indicates that
  // `this` should run in the restricted mode until `ReleaseRestriction` is
  // called. In this mode, frames requests for output devices are not granted.
  // The given `restricted_callback` will be invoked if the frame requests an
  // output device.
  RenderFrameAudioOutputStreamFactory(
      RenderFrameHostImpl* frame,
      media::AudioSystem* audio_system,
      MediaStreamManager* media_stream_manager,
      mojo::PendingReceiver<blink::mojom::RendererAudioOutputStreamFactory>
          receiver,
      absl::optional<base::OnceClosure> restricted_callback);

  RenderFrameAudioOutputStreamFactory(
      const RenderFrameAudioOutputStreamFactory&) = delete;
  RenderFrameAudioOutputStreamFactory& operator=(
      const RenderFrameAudioOutputStreamFactory&) = delete;

  ~RenderFrameAudioOutputStreamFactory();

  void SetAuthorizedDeviceIdForGlobalMediaControls(
      std::string hashed_device_id);

  // Called when the frame is allowed to request output devices, e.g., when the
  // prerendering frame is about to be activated.
  void ReleaseRestriction();

  size_t CurrentNumberOfProvidersForTesting();

 private:
  class Core;
  class RestrictedModeCore;
  std::unique_ptr<Core> core_;

  // Used while the frame is disallowed to request output devices.
  std::unique_ptr<RestrictedModeCore> restricted_mode_core_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_RENDER_FRAME_AUDIO_OUTPUT_STREAM_FACTORY_H_
