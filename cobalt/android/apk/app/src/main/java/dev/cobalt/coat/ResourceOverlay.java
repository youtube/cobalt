// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

package dev.cobalt.coat;

import android.content.Context;
import org.jni_zero.CalledByNative;

/**
 * A class for managing the resource overlays of Cobalt. Client can turn on certain feature by
 * setting the resource overlay.
 */
public class ResourceOverlay {
  // To facilitate maintenance, these member names should match what is in the
  // resource XML file.
  private final boolean mSupportsSphericalVideos;

  private final int mMaxVideoBufferBudget;

  private final int mMinAudioSinkBufferSizeInFrames;

  public ResourceOverlay(Context context) {
    // Load the values for all Overlay variables.
    mSupportsSphericalVideos =
        context.getResources().getBoolean(R.bool.supports_spherical_videos);
    mMaxVideoBufferBudget =
        context.getResources().getInteger(R.integer.max_video_buffer_budget);
    mMinAudioSinkBufferSizeInFrames =
        context.getResources().getInteger(R.integer.min_audio_sink_buffer_size_in_frames);
  }

  @CalledByNative
  public boolean getSupportsSphericalVideos() {
    return mSupportsSphericalVideos;
  }

  @CalledByNative
  public int getMaxVideoBufferBudget() {
    return mMaxVideoBufferBudget;
  }

  @CalledByNative
  public int getMinAudioSinkBufferSizeInFrames() {
    return mMinAudioSinkBufferSizeInFrames;
  }
}
