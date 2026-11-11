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

#include "cobalt/shell/browser/splash/native_splash.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

namespace {

SbWindow g_sb_window = kSbWindowInvalid;
NativeSplash* g_waiting_splash = nullptr;

// Whether video can be played before Starboard creates its window.
constexpr bool kNeedsSbWindow = BUILDFLAG(IS_STARBOARD);

gfx::Rect GetVideoBounds(SbWindow window) {
#if BUILDFLAG(IS_STARBOARD)
  SbWindowSize size{};
  if (SbWindowIsValid(window) && SbWindowGetSize(window, &size)) {
    return gfx::Rect(size.width, size.height);
  }
#endif
  if (display::Screen* screen = display::Screen::GetScreen()) {
    return gfx::Rect(screen->GetPrimaryDisplay().GetSizeInPixel());
  }
  return gfx::Rect(1920, 1080);
}

}  // namespace

NativeSplash::Config::Config() = default;
NativeSplash::Config::Config(Config&&) = default;
NativeSplash::Config& NativeSplash::Config::operator=(Config&&) = default;
NativeSplash::Config::~Config() = default;

// static
void NativeSplash::SetSbWindow(SbWindow window) {
  g_sb_window = window;
  if (SbWindowIsValid(window) && g_waiting_splash) {
    g_waiting_splash->MaybePlay();
  }
}

NativeSplash::NativeSplash(BrowserContext* browser_context, Config config)
    : config_(std::move(config)),
      selector_(browser_context, config_.splash_origin, config_.cache_name) {}

NativeSplash::~NativeSplash() {
  if (g_waiting_splash == this) {
    g_waiting_splash = nullptr;
  }
}

void NativeSplash::Start(base::OnceClosure on_presenting,
                         base::OnceClosure on_finished) {
  on_presenting_ = std::move(on_presenting);
  on_finished_ = std::move(on_finished);
  start_time_ = base::TimeTicks::Now();
  g_waiting_splash = this;
  selector_.Select(
      config_.selector_input,
      base::BindOnce(&NativeSplash::OnSelected, weak_factory_.GetWeakPtr()));
}

void NativeSplash::OnSelected(SplashSelection selection) {
  if (selection.video.empty()) {
    LOG(INFO) << "NativeSplash: no cached video, playing the built-in one.";
    video_ = std::move(config_.builtin_video);
  } else {
    LOG(INFO) << "NativeSplash: playing cached " << selection.key;
    video_ = std::move(selection.video);
  }
  MaybePlay();
}

void NativeSplash::MaybePlay() {
  if (playing_ || !video_) {
    return;
  }
  if (kNeedsSbWindow && !SbWindowIsValid(g_sb_window)) {
    LOG(INFO) << "NativeSplash: waiting for the Starboard window.";
    return;
  }
  g_waiting_splash = nullptr;
  playing_ = true;
  const gfx::Rect bounds = GetVideoBounds(g_sb_window);
  LOG(INFO) << "NativeSplash: starting playback in " << bounds.ToString()
            << ", " << (base::TimeTicks::Now() - start_time_).InMilliseconds()
            << " ms after start.";
  SplashVideoPlayer::Callbacks callbacks;
  callbacks.on_presenting = base::BindOnce(
      [](base::WeakPtr<NativeSplash> self) {
        if (!self) {
          return;
        }
        LOG(INFO)
            << "NativeSplash: first frame, "
            << (base::TimeTicks::Now() - self->start_time_).InMilliseconds()
            << " ms after start.";
        if (self->on_presenting_) {
          std::move(self->on_presenting_).Run();
        }
      },
      weak_factory_.GetWeakPtr());
  callbacks.on_finished = base::BindOnce(&NativeSplash::OnPlayerFinished,
                                         weak_factory_.GetWeakPtr());
  player_.Play(*video_, g_sb_window, bounds, config_.max_duration,
               std::move(callbacks));
}

void NativeSplash::OnPlayerFinished(SplashVideoPlayer::Result result) {
  LOG(INFO) << "NativeSplash: done ("
            << SplashVideoPlayer::ResultToString(result) << "), "
            << (base::TimeTicks::Now() - start_time_).InMilliseconds()
            << " ms after start.";
  if (on_presenting_) {
    // Nothing was shown; still let the caller take down the system splash.
    std::move(on_presenting_).Run();
  }
  if (on_finished_) {
    std::move(on_finished_).Run();
  }
}

}  // namespace content
