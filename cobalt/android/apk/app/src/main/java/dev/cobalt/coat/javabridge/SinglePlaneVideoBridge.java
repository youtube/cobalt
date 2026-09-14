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

import dev.cobalt.util.Log;

/**
 * JavaScript bridge interface for Single-Plane Video Passthrough (1-Surface Mode).
 *
 * <p>Allows the YouTube web application and video players to signal playback states, player control
 * visibility, and closed caption status, enabling the platform to dynamically transition between
 * 2-surface mode (Video + UI Window) and 1-surface mode (Video Surface alone) to eliminate DRAM
 * scanout memory bandwidth during steady-state fullscreen video playback.
 */
public class SinglePlaneVideoBridge implements CobaltJavaScriptAndroidObject {
  private static final String TAG = "SinglePlaneVideoBridge";

  private final SinglePlaneVideoDelegate mDelegate;

  public SinglePlaneVideoBridge(SinglePlaneVideoDelegate delegate) {
    this.mDelegate = delegate;
  }

  @Override
  public String getJavaScriptInterfaceName() {
    return "CobaltSinglePlaneBridge";
  }

  /**
   * Called by the web player when UI controls are hidden or shown during video playback.
   *
   * @param hide true when controls have faded out and UI is quiescent, false when controls are
   *     shown.
   */
  @CobaltJavaScriptInterface
  public void setUiHiddenDuringPlayback(boolean hide) {
    Log.i(TAG, "setUiHiddenDuringPlayback: " + hide);
    mDelegate.runOnUiThread(
        () -> {
          if (hide) {
            mDelegate.setSinglePlaneEngaged(true);
          } else {
            mDelegate.wakeUpUiSurface();
          }
        });
  }

  /**
   * Called when closed captions / subtitles are toggled in the player.
   *
   * @param active true if captions are currently being rendered on screen, false otherwise.
   */
  @CobaltJavaScriptInterface
  public void setCaptionsActive(boolean active) {
    Log.i(TAG, "setCaptionsActive: " + active);
    mDelegate.runOnUiThread(() -> mDelegate.setCaptionsActive(active));
  }

  /**
   * Called when video enters or exits fullscreen playback.
   *
   * @param playing true if video is playing in fullscreen mode.
   */
  @CobaltJavaScriptInterface
  public void setVideoPlayingFullscreen(boolean playing) {
    Log.i(TAG, "setVideoPlayingFullscreen: " + playing);
    mDelegate.runOnUiThread(() -> mDelegate.setVideoPlayingFullscreen(playing));
  }

  /** Returns whether Single-Plane Mode is currently engaged (UI surface hidden). */
  @CobaltJavaScriptInterface
  public boolean isSinglePlaneEngaged() {
    return mDelegate.isSinglePlaneModeEngaged();
  }

  /** Returns whether captions are currently active (either system accessibility or player [CC]). */
  @CobaltJavaScriptInterface
  public boolean isCaptionsActive() {
    return mDelegate.isCaptionsActive();
  }

  /** Forces an immediate UI wake-up. */
  @CobaltJavaScriptInterface
  public void wakeUp() {
    mDelegate.runOnUiThread(mDelegate::wakeUpUiSurface);
  }
}
