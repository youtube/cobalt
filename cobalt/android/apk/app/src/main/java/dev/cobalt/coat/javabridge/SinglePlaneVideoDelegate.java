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

package dev.cobalt.coat.javabridge;

/**
 * Delegate interface for managing Single-Plane Video Passthrough (1-Surface Mode). Implemented by
 * the host Activity or platform window manager to control surface composition.
 */
public interface SinglePlaneVideoDelegate {
  /** Engages or disengages single plane mode. */
  void setSinglePlaneEngaged(boolean engage);

  /** Immediately wakes up the UI surface and disengages single plane mode. */
  void wakeUpUiSurface();

  /** Sets whether captions / subtitles are currently active. */
  void setCaptionsActive(boolean active);

  /** Sets whether video is playing in fullscreen mode. */
  void setVideoPlayingFullscreen(boolean playing);

  /** Returns true if single plane mode is currently engaged (UI window hidden). */
  boolean isSinglePlaneModeEngaged();

  /** Returns true if captions are currently active. */
  boolean isCaptionsActive();

  /** Runs the specified action on the UI thread. */
  void runOnUiThread(Runnable action);
}
