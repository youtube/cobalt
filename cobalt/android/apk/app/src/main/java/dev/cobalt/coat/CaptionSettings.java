// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

import android.view.accessibility.CaptioningManager;
import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** Captures the system Caption style in properties as needed by the Starboard implementation. */
@JNINamespace("starboard")
public class CaptionSettings {

  private final boolean isEnabled;
  private final float fontScale;
  private final int edgeType;
  private final boolean hasEdgeType;
  private final int foregroundColor;
  private final boolean hasForegroundColor;
  private final int backgroundColor;
  private final boolean hasBackgroundColor;
  private final int windowColor;
  private final boolean hasWindowColor;

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

  @CalledByNative
  public boolean isEnabled() {
    return isEnabled;
  }

  @CalledByNative
  public float getFontScale() {
    return fontScale;
  }

  @CalledByNative
  public int getEdgeType() {
    return edgeType;
  }

  @CalledByNative
  public boolean hasEdgeType() {
    return hasEdgeType;
  }

  @CalledByNative
  public int getForegroundColor() {
    return foregroundColor;
  }

  @CalledByNative
  public boolean hasForegroundColor() {
    return hasForegroundColor;
  }

  @CalledByNative
  public int getBackgroundColor() {
    return backgroundColor;
  }

  @CalledByNative
  public boolean hasBackgroundColor() {
    return hasBackgroundColor;
  }

  @CalledByNative
  public int getWindowColor() {
    return windowColor;
  }

  @CalledByNative
  public boolean hasWindowColor() {
    return hasWindowColor;
  }
}
