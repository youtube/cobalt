// Copyright 2017 Google Inc. All Rights Reserved.
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

package dev.cobalt.media;

import android.view.accessibility.CaptioningManager;
import dev.cobalt.util.UsedByNative;

/**
 * Captures the system Caption style in properties as needed by the Starboard implementation.
 */
public class CaptionSettings {

  @UsedByNative public final boolean isEnabled;
  @UsedByNative public final float fontScale;
  @UsedByNative public final int edgeType;
  @UsedByNative public final boolean hasEdgeType;
  @UsedByNative public final int foregroundColor;
  @UsedByNative public final boolean hasForegroundColor;
  @UsedByNative public final int backgroundColor;
  @UsedByNative public final boolean hasBackgroundColor;
  @UsedByNative public final int windowColor;
  @UsedByNative public final boolean hasWindowColor;

  public CaptionSettings(CaptioningManager cm) {
    CaptioningManager.CaptionStyle style = cm.getUserStyle();
    isEnabled = cm.isEnabled();
    fontScale = cm.getFontScale();
    edgeType = style.edgeType;
    hasEdgeType = style.hasEdgeType();
    foregroundColor = style.foregroundColor;
    hasForegroundColor = style.hasForegroundColor();
    backgroundColor = style.backgroundColor;
    hasBackgroundColor = style.hasBackgroundColor();
    windowColor = style.windowColor;
    hasWindowColor = style.hasWindowColor();
  }
}
