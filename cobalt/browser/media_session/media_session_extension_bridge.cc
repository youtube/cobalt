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

#include "cobalt/browser/media_session/media_session_extension_bridge.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"
#include "starboard/system.h"

namespace cobalt {

namespace {

using media_session::mojom::MediaPlaybackState;
using media_session::mojom::MediaSessionAction;
using media_session::mojom::MediaSessionImageType;
using media_session::mojom::MediaSessionInfo;
using SuspendType = media_session::mojom::MediaSession::SuspendType;

// The platform holds a single (callback, context) pair, so the bridge that is
// currently registered is tracked process-wide. The platform may invoke the
// callbacks from any thread, so access is guarded by a lock and the bridge is
// only ever dereferenced on its own sequence via |weak_bridge|.
struct Registration {
  // Identity of the registered bridge. Only compared, never dereferenced.
  const void* bridge = nullptr;
  scoped_refptr<base::SequencedTaskRunner> task_runner;
  base::WeakPtr<MediaSessionExtensionBridge> weak_bridge;
};

base::Lock& GetRegistrationLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

Registration& GetRegistration() {
  static base::NoDestructor<Registration> registration;
  return *registration;
}

// Maps a Chromium MediaSessionAction to the extension's action enum. Returns
// false for actions that the extension has no equivalent for.
bool ToExtensionAction(MediaSessionAction action,
                       CobaltExtensionMediaSessionAction* out) {
  switch (action) {
    case MediaSessionAction::kPlay:
      *out = kCobaltExtensionMediaSessionActionPlay;
      return true;
    case MediaSessionAction::kPause:
      *out = kCobaltExtensionMediaSessionActionPause;
      return true;
    case MediaSessionAction::kSeekBackward:
      *out = kCobaltExtensionMediaSessionActionSeekbackward;
      return true;
    case MediaSessionAction::kSeekForward:
      *out = kCobaltExtensionMediaSessionActionSeekforward;
      return true;
    case MediaSessionAction::kPreviousTrack:
      *out = kCobaltExtensionMediaSessionActionPrevioustrack;
      return true;
    case MediaSessionAction::kNextTrack:
      *out = kCobaltExtensionMediaSessionActionNexttrack;
      return true;
    case MediaSessionAction::kSkipAd:
      *out = kCobaltExtensionMediaSessionActionSkipad;
      return true;
    case MediaSessionAction::kStop:
      *out = kCobaltExtensionMediaSessionActionStop;
      return true;
    case MediaSessionAction::kSeekTo:
      *out = kCobaltExtensionMediaSessionActionSeekto;
      return true;
    default:
      return false;
  }
}

// Formats MediaImage::sizes as the space-separated "WxH" list used by the
// MediaImage.sizes IDL attribute, which is what the extension expects.
std::string FormatSizes(const std::vector<gfx::Size>& sizes) {
  std::vector<std::string> parts;
  parts.reserve(sizes.size());
  for (const auto& size : sizes) {
    parts.push_back(size.ToString());
  }
  return base::JoinString(parts, " ");
}

}  // namespace

// static
std::unique_ptr<MediaSessionExtensionBridge>
MediaSessionExtensionBridge::MaybeCreate(content::WebContents* web_contents) {
  DCHECK(web_contents);
  const auto* extension = static_cast<const CobaltExtensionMediaSessionApi*>(
      SbSystemGetExtension(kCobaltExtensionMediaSessionName));
  if (!extension) {
    return nullptr;
  }
  if (strcmp(extension->name, kCobaltExtensionMediaSessionName) != 0 ||
      extension->version < 1) {
    LOG(WARNING) << "Ignoring incompatible MediaSession extension.";
    return nullptr;
  }
  if (!extension->OnMediaSessionStateChanged ||
      !extension->RegisterMediaSessionCallbacks) {
    LOG(WARNING) << "MediaSession extension is missing required functions.";
    return nullptr;
  }

  // Not using std::make_unique because the constructor is protected.
  std::unique_ptr<MediaSessionExtensionBridge> bridge(
      new MediaSessionExtensionBridge(extension, web_contents));
  bridge->Initialize();
  LOG(INFO) << "MediaSession extension bridge created.";
  return bridge;
}

MediaSessionExtensionBridge::MediaSessionExtensionBridge(
    const CobaltExtensionMediaSessionApi* extension,
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents), extension_(extension) {
  DCHECK(extension_);
}

MediaSessionExtensionBridge::~MediaSessionExtensionBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UnregisterFromPlatform();
}

void MediaSessionExtensionBridge::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::MediaSession* media_session = GetMediaSession();
  if (media_session) {
    // The session immediately reports its current state to new observers.
    media_session->AddObserver(receiver_.BindNewPipeAndPassRemote());
  }
  RegisterWithPlatform();
}

content::MediaSession* MediaSessionExtensionBridge::GetMediaSession() {
  return web_contents() ? content::MediaSession::Get(web_contents()) : nullptr;
}

void MediaSessionExtensionBridge::WebContentsDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receiver_.reset();
  UnregisterFromPlatform();
}

void MediaSessionExtensionBridge::RegisterWithPlatform() {
  {
    base::AutoLock lock(GetRegistrationLock());
    Registration& registration = GetRegistration();
    registration.bridge = this;
    registration.task_runner = base::SequencedTaskRunner::GetCurrentDefault();
    registration.weak_bridge = weak_factory_.GetWeakPtr();
  }
  // Re-registering replaces any callbacks a previous bridge installed. The
  // context must be non-null: some platforms (e.g. tvOS) drop actions when it
  // is.
  extension_->RegisterMediaSessionCallbacks(
      this, &MediaSessionExtensionBridge::InvokeActionCallback,
      &MediaSessionExtensionBridge::UpdatePlatformPlaybackStateCallback);
}

void MediaSessionExtensionBridge::UnregisterFromPlatform() {
  {
    base::AutoLock lock(GetRegistrationLock());
    Registration& registration = GetRegistration();
    if (registration.bridge != this) {
      // Never registered, already unregistered, or a newer bridge has taken
      // over. In the latter case the platform's callbacks must stay intact.
      return;
    }
    registration = Registration();
  }
  weak_factory_.InvalidateWeakPtrs();
  if (extension_->DestroyMediaSessionClientCallback) {
    extension_->DestroyMediaSessionClientCallback();
  }
}

// static
void MediaSessionExtensionBridge::InvokeActionCallback(
    CobaltExtensionMediaSessionActionDetails details,
    void* callback_context) {
  base::AutoLock lock(GetRegistrationLock());
  const Registration& registration = GetRegistration();
  if (!registration.task_runner || registration.bridge != callback_context) {
    LOG(WARNING) << "Dropping MediaSession action " << details.action
                 << ": no bridge registered for this context.";
    return;
  }
  registration.task_runner->PostTask(
      FROM_HERE, base::BindOnce(&MediaSessionExtensionBridge::HandleAction,
                                registration.weak_bridge, details));
}

// static
void MediaSessionExtensionBridge::UpdatePlatformPlaybackStateCallback(
    CobaltExtensionMediaSessionPlaybackState state,
    void* callback_context) {
  // In Cobalt 25 the platform reported SbPlayer state here so that the web
  // engine could guess the playback state. content::MediaSessionImpl tracks
  // its players directly, so there is nothing to do; the callback is still
  // provided because platforms may call it unconditionally.
  VLOG(1) << "Ignoring platform playback state update: " << state;
}

void MediaSessionExtensionBridge::HandleAction(
    const CobaltExtensionMediaSessionActionDetails& details) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::MediaSession* media_session = GetMediaSession();
  if (!media_session) {
    return;
  }
  VLOG(1) << "MediaSession action from platform: " << details.action;

  switch (details.action) {
    case kCobaltExtensionMediaSessionActionPlay:
      // Routes to the page's "play" handler if any, else resumes players.
      media_session->Resume(SuspendType::kUI);
      break;
    case kCobaltExtensionMediaSessionActionPause:
      // Routes to the page's "pause" handler if any, else pauses players.
      media_session->Suspend(SuspendType::kUI);
      break;
    case kCobaltExtensionMediaSessionActionStop:
      // MediaSession::Stop() DCHECKs an active session and tears down audio
      // focus; only notify the page, which is what Cobalt 25 did.
      media_session->DidReceiveAction(MediaSessionAction::kStop);
      break;
    case kCobaltExtensionMediaSessionActionPrevioustrack:
      media_session->PreviousTrack();
      break;
    case kCobaltExtensionMediaSessionActionNexttrack:
      media_session->NextTrack();
      break;
    case kCobaltExtensionMediaSessionActionSkipad:
      media_session->SkipAd();
      break;
    case kCobaltExtensionMediaSessionActionSeekforward:
    case kCobaltExtensionMediaSessionActionSeekbackward: {
      const bool forward =
          details.action == kCobaltExtensionMediaSessionActionSeekforward;
      // content::MediaSession has no way to pass |seek_offset| to the page's
      // seekforward/seekbackward handlers. When the platform asks for a
      // specific offset (e.g. "skip ahead 30 seconds") and the position is
      // known, translate it into an absolute seek so the offset is honored.
      if (details.seek_offset > 0 && position_.has_value()) {
        base::TimeDelta target =
            position_->GetPosition() +
            (forward ? 1 : -1) * base::Seconds(details.seek_offset);
        target = std::max(target, base::TimeDelta());
        if (!position_->duration().is_zero() &&
            !position_->duration().is_max()) {
          target = std::min(target, position_->duration());
        }
        media_session->SeekTo(target);
        break;
      }
      const base::TimeDelta offset = details.seek_offset > 0
                                         ? base::Seconds(details.seek_offset)
                                         : kDefaultSeekOffset;
      // Routes to the page's seek handler if any, else seeks players.
      media_session->Seek(forward ? offset : -offset);
      break;
    }
    case kCobaltExtensionMediaSessionActionSeekto:
      if (details.seek_time < 0) {
        LOG(WARNING) << "Ignoring seekto without a seek time.";
        break;
      }
      if (details.fast_seek) {
        media_session->ScrubTo(base::Seconds(details.seek_time));
      } else {
        media_session->SeekTo(base::Seconds(details.seek_time));
      }
      break;
    default:
      LOG(WARNING) << "Unsupported MediaSession action " << details.action;
      break;
  }
}

void MediaSessionExtensionBridge::MediaSessionInfoChanged(
    media_session::mojom::MediaSessionInfoPtr info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  session_info_ = std::move(info);
  PushStateToPlatform();
}

void MediaSessionExtensionBridge::MediaSessionMetadataChanged(
    const std::optional<media_session::MediaMetadata>& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  metadata_ = metadata;
  PushStateToPlatform();
}

void MediaSessionExtensionBridge::MediaSessionActionsChanged(
    const std::vector<MediaSessionAction>& actions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  actions_ = actions;
  PushStateToPlatform();
}

void MediaSessionExtensionBridge::MediaSessionImagesChanged(
    const base::flat_map<MediaSessionImageType,
                         std::vector<media_session::MediaImage>>& images) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = images.find(MediaSessionImageType::kArtwork);
  if (it != images.end()) {
    artwork_ = it->second;
  } else {
    artwork_.clear();
  }
  PushStateToPlatform();
}

void MediaSessionExtensionBridge::MediaSessionPositionChanged(
    const std::optional<media_session::MediaPosition>& position) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  position_ = position;
  PushStateToPlatform();
}

CobaltExtensionMediaSessionPlaybackState
MediaSessionExtensionBridge::ComputePlaybackState() const {
  if (!session_info_ ||
      session_info_->state == MediaSessionInfo::SessionState::kInactive) {
    return kCobaltExtensionMediaSessionNone;
  }
  return session_info_->playback_state == MediaPlaybackState::kPlaying
             ? kCobaltExtensionMediaSessionPlaying
             : kCobaltExtensionMediaSessionPaused;
}

void MediaSessionExtensionBridge::PushStateToPlatform() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CobaltExtensionMediaSessionState state = {};
  state.actual_playback_state = ComputePlaybackState();

  for (MediaSessionAction action : actions_) {
    CobaltExtensionMediaSessionAction ext_action;
    if (ToExtensionAction(action, &ext_action)) {
      state.available_actions[ext_action] = true;
    }
  }

  if (position_.has_value()) {
    state.has_position_state = true;
    state.duration = position_->duration().InMicroseconds();
    state.current_playback_position = position_->GetPosition().InMicroseconds();
    state.actual_playback_rate = position_->playback_rate();
  } else {
    state.has_position_state = false;
    state.actual_playback_rate =
        state.actual_playback_state == kCobaltExtensionMediaSessionPlaying
            ? 1.0
            : 0.0;
  }

  // The extension only borrows these pointers for the duration of the call,
  // so the backing strings just need to outlive OnMediaSessionStateChanged().
  std::string title, artist, album;
  if (metadata_.has_value()) {
    title = base::UTF16ToUTF8(metadata_->title);
    artist = base::UTF16ToUTF8(metadata_->artist);
    album = base::UTF16ToUTF8(metadata_->album);
  }

  std::vector<std::string> artwork_src, artwork_sizes, artwork_type;
  artwork_src.reserve(artwork_.size());
  artwork_sizes.reserve(artwork_.size());
  artwork_type.reserve(artwork_.size());
  for (const auto& image : artwork_) {
    artwork_src.push_back(image.src.spec());
    artwork_sizes.push_back(FormatSizes(image.sizes));
    artwork_type.push_back(base::UTF16ToUTF8(image.type));
  }
  std::vector<CobaltExtensionMediaImage> ext_artwork(artwork_.size());
  for (size_t i = 0; i < artwork_.size(); ++i) {
    ext_artwork[i].src = artwork_src[i].c_str();
    ext_artwork[i].size = artwork_sizes[i].c_str();
    ext_artwork[i].type = artwork_type[i].c_str();
  }

  CobaltExtensionMediaMetadata ext_metadata = {};
  ext_metadata.album = album.c_str();
  ext_metadata.artist = artist.c_str();
  ext_metadata.title = title.c_str();
  ext_metadata.artwork = ext_artwork.empty() ? nullptr : ext_artwork.data();
  ext_metadata.artwork_count = ext_artwork.size();
  state.metadata = &ext_metadata;

  extension_->OnMediaSessionStateChanged(state);
}

}  // namespace cobalt
