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
import dev.cobalt.util.UsedByNative;

/**
 * A class for managing the resource overlays of Cobalt. Client can turn on certain feature by
 * setting the resource overlay.
 */
public class ResourceOverlay {
  // To facilitate maintenance, these member names should match what is in the
  // resource XML file.
  @SuppressWarnings("MemberName")
  @UsedByNative
  public final boolean supports_spherical_videos;

  public ResourceOverlay(Context context) {
    // Load the values for all Overlay variables.
    this.supports_spherical_videos =
        context.getResources().getBoolean(R.bool.supports_spherical_videos);
  }
}
