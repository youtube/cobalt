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

#ifndef COBALT_SHELL_BROWSER_NATIVE_SPLASH_VIDEO_PLAYER_H_
#define COBALT_SHELL_BROWSER_NATIVE_SPLASH_VIDEO_PLAYER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/threading/sequence_bound.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

// Plays a splash screen video with an SbPlayer that it drives directly, without
// Blink's media stack: no <video> element, Media Source Extensions, demuxer,
// Mojo renderer or StarboardRenderer. The frames are indexed with
// IndexSplashVideoWebM(), and written in place, without copies, to a
// video-only, punch-out SbPlayer on a dedicated thread.
// See cobalt::features::kNativeVideoSplashScreen.
//
// Must be created and destroyed on the same sequence, where the callbacks run.
class NativeSplashVideoPlayer {
 public:
  enum class Error {
    // The platform can't play the video natively: no VP9, or no punch-out.
    kUnsupported,
    // SbPlayerCreate() failed.
    kCreateFailed,
    // The SbPlayer reported an error.
    kPlaybackError,
  };

  struct Callbacks {
    Callbacks();
    Callbacks(Callbacks&&);
    Callbacks& operator=(Callbacks&&);
    ~Callbacks();

    // The SbPlayer started presenting the first frame.
    base::OnceClosure on_first_frame;
    // Playback reached the end of the video. The last frame stays on screen
    // until the player is destroyed.
    base::OnceClosure on_ended;
    // Playback failed. No other callback runs afterwards.
    base::OnceCallback<void(Error)> on_error;
  };

  // Starts playing `webm`, which must stay valid until the player is
  // destroyed, e.g., a built-in resource. The video is shown in `bounds`, in
  // pixels of the window, centered and fit inside, keeping its aspect ratio,
  // like a <video> with the default object-fit; it's left to the platform if
  // `bounds` is empty. Returns nullptr if `webm` isn't a valid splash screen
  // video.
  static std::unique_ptr<NativeSplashVideoPlayer> Create(
      base::span<const uint8_t> webm,
      const gfx::Rect& bounds,
      Callbacks callbacks);

  // Like Create(), for the built-in video that splash.html would play for
  // `topic`: the 1080p video if the platform plays 4K VP9, the 480p one
  // otherwise. `bounds` defaults to the whole screen if empty. Returns nullptr
  // if the video isn't valid. If the platform doesn't play VP9, reports
  // Error::kUnsupported.
  static std::unique_ptr<NativeSplashVideoPlayer> CreateForBuiltInVideo(
      const std::string& topic,
      const gfx::Rect& bounds,
      Callbacks callbacks);

  NativeSplashVideoPlayer(const NativeSplashVideoPlayer&) = delete;
  NativeSplashVideoPlayer& operator=(const NativeSplashVideoPlayer&) = delete;

  // Destroys the SbPlayer on its thread, without blocking.
  ~NativeSplashVideoPlayer();

  // Shows the video in `bounds` instead, as in Create(), e.g., when the window
  // is resized. Ignored if empty.
  void SetBounds(const gfx::Rect& bounds);

 private:
  class Core;
  // A video to play, and the MIME type that the platform must play for it.
  struct Video;

  // Starts playing the first of `videos` that the platform plays, after
  // querying its media capabilities on the player's thread.
  static std::unique_ptr<NativeSplashVideoPlayer> CreateForVideos(
      std::vector<Video> videos,
      const gfx::Rect& bounds,
      Callbacks callbacks);

  explicit NativeSplashVideoPlayer(base::SequenceBound<Core> core);

  base::SequenceBound<Core> core_;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_NATIVE_SPLASH_VIDEO_PLAYER_H_
