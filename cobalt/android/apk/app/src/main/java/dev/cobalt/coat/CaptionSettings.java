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
import dev.cobalt.util.UsedByNative;

/** Captures the system Caption style in properties as needed by the Starboard implementation. */
public class CaptionSettings {

<<<<<<< HEAD
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
=======
  private final boolean mIsEnabled;
  private final float mFontScale;
  private final int mEdgeType;
  private final boolean mHasEdgeType;
  private final int mForegroundColor;
  private final boolean mHasForegroundColor;
  private final int mBackgroundColor;
  private final boolean mHasBackgroundColor;
  private final int mWindowColor;
  private final boolean mHasWindowColor;
>>>>>>> 59b92462d69 (Fix: Fix non-public, non-static field name in CaptionSettings.java (#7924))

  public CaptionSettings(CaptioningManager cm) {
    CaptioningManager.CaptionStyle style = cm.getUserStyle();
    mIsEnabled = cm.isEnabled();
    mFontScale = cm.getFontScale();
    mEdgeType = style.edgeType;
    mHasEdgeType = style.hasEdgeType();
    mForegroundColor = style.foregroundColor;
    mHasForegroundColor = style.hasForegroundColor();
    mBackgroundColor = style.backgroundColor;
    mHasBackgroundColor = style.hasBackgroundColor();
    mWindowColor = style.windowColor;
    mHasWindowColor = style.hasWindowColor();
  }
<<<<<<< HEAD
=======

  @CalledByNative
  public boolean isEnabled() {
    return mIsEnabled;
  }

  @CalledByNative
  public float getFontScale() {
    return mFontScale;
  }

  @CalledByNative
  public int getEdgeType() {
    return mEdgeType;
  }

  @CalledByNative
  public boolean hasEdgeType() {
    return mHasEdgeType;
  }

  @CalledByNative
  public int getForegroundColor() {
    return mForegroundColor;
  }

  @CalledByNative
  public boolean hasForegroundColor() {
    return mHasForegroundColor;
  }

  @CalledByNative
  public int getBackgroundColor() {
    return mBackgroundColor;
  }

  @CalledByNative
  public boolean hasBackgroundColor() {
    return mHasBackgroundColor;
  }

  @CalledByNative
  public int getWindowColor() {
    return mWindowColor;
  }

  @CalledByNative
  public boolean hasWindowColor() {
    return mHasWindowColor;
  }
>>>>>>> 59b92462d69 (Fix: Fix non-public, non-static field name in CaptionSettings.java (#7924))
}
