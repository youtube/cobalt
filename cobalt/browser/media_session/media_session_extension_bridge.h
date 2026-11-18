// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_EXTENSION_BRIDGE_H_
#define COBALT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_EXTENSION_BRIDGE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/cpp/media_image.h"
#include "services/media_session/public/cpp/media_metadata.h"
#include "services/media_session/public/cpp/media_position.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "starboard/extension/media_session.h"

namespace content {
class MediaSession;
class WebContents;
}  // namespace content

namespace cobalt {

// Bridges Chromium's content::MediaSession (which backs the web app's
// navigator.mediaSession) to the platform's Starboard MediaSession extension
// (dev.cobalt.extension.MediaSession).
//
// In Cobalt 25 this role was filled by cobalt/media_session/
// media_session_client.cc, which owned its own Blink-side MediaSession
// implementation. Cobalt 27 uses the upstream Blink/content implementation, so
// this class only translates between the two:
//
//  * Web -> platform: observes the MediaSession over mojo and forwards the
//    playback state, metadata, artwork, available actions and position state
//    to CobaltExtensionMediaSessionApi::OnMediaSessionStateChanged.
//  * Platform -> web: registers callbacks via RegisterMediaSessionCallbacks and
//    turns CobaltExtensionMediaSessionActionDetails into content::MediaSession
//    calls (Resume/Suspend/SeekTo/...), which route to the page's action
//    handlers when present.
//
// The extension API is a process-wide singleton, so at most one bridge is
// registered with the platform at a time. Creating a new bridge (e.g. for a
// new main WebContents) takes over the registration from the previous one.
//
// All methods must be called on the browser UI thread. The static extension
// callbacks may be invoked by the platform on any thread; they hop to the UI
// thread before touching the bridge.
class MediaSessionExtensionBridge
    : public content::WebContentsObserver,
      public media_session::mojom::MediaSessionObserver {
 public:
  // Seek offset used for seekforward/seekbackward when the platform does not
  // specify one.
  static constexpr base::TimeDelta kDefaultSeekOffset = base::Seconds(10);

  // Returns a bridge observing |web_contents| if the platform provides a
  // usable MediaSession extension, or nullptr otherwise.
  static std::unique_ptr<MediaSessionExtensionBridge> MaybeCreate(
      content::WebContents* web_contents);

  MediaSessionExtensionBridge(const MediaSessionExtensionBridge&) = delete;
  MediaSessionExtensionBridge& operator=(const MediaSessionExtensionBridge&) =
      delete;

  ~MediaSessionExtensionBridge() override;

  // media_session::mojom::MediaSessionObserver:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr info) override;
  void MediaSessionMetadataChanged(
      const std::optional<media_session::MediaMetadata>& metadata) override;
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& actions)
      override;
  void MediaSessionImagesChanged(
      const base::flat_map<media_session::mojom::MediaSessionImageType,
                           std::vector<media_session::MediaImage>>& images)
      override;
  void MediaSessionPositionChanged(
      const std::optional<media_session::MediaPosition>& position) override;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

 protected:
  // |extension| must outlive this object (extensions are static platform
  // data). |web_contents| may be null in tests that override
  // GetMediaSession().
  MediaSessionExtensionBridge(const CobaltExtensionMediaSessionApi* extension,
                              content::WebContents* web_contents);

  // Starts observing the MediaSession and registers with the platform.
  // Separate from the constructor so that tests can override
  // GetMediaSession().
  void Initialize();

  // Returns the session to observe and control, or nullptr if the
  // WebContents is gone. Virtual for testing.
  virtual content::MediaSession* GetMediaSession();

  // Dispatches a platform action to the MediaSession. Exposed for testing;
  // production code reaches this through the static InvokeActionCallback.
  void HandleAction(const CobaltExtensionMediaSessionActionDetails& details);

 private:
  // Static trampolines handed to the platform. |callback_context| is the
  // registered bridge, but it is never dereferenced off the UI thread.
  static void InvokeActionCallback(
      CobaltExtensionMediaSessionActionDetails details,
      void* callback_context);
  static void UpdatePlatformPlaybackStateCallback(
      CobaltExtensionMediaSessionPlaybackState state,
      void* callback_context);

  void RegisterWithPlatform();
  void UnregisterFromPlatform();

  // Builds a CobaltExtensionMediaSessionState from the cached session state
  // and passes it to the platform.
  void PushStateToPlatform();

  CobaltExtensionMediaSessionPlaybackState ComputePlaybackState() const;

  const raw_ptr<const CobaltExtensionMediaSessionApi> extension_;

  // Latest state received from the MediaSession.
  media_session::mojom::MediaSessionInfoPtr session_info_;
  std::optional<media_session::MediaMetadata> metadata_;
  std::vector<media_session::mojom::MediaSessionAction> actions_;
  std::vector<media_session::MediaImage> artwork_;
  std::optional<media_session::MediaPosition> position_;

  mojo::Receiver<media_session::mojom::MediaSessionObserver> receiver_{this};

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<MediaSessionExtensionBridge> weak_factory_{this};
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_MEDIA_SESSION_MEDIA_SESSION_EXTENSION_BRIDGE_H_
