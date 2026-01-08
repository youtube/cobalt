// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

package dev.cobalt.shell;

import android.graphics.Bitmap;
import android.view.ViewGroup;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.mojom.CursorType;

/**
 * Copied from org.chromium.content_shell.ShellViewAndroidDelegate.
 * Implementation of the abstract class {@link ViewAndroidDelegate} for content shell.
 * Extended for testing.
 */
@NullMarked
public class ShellViewAndroidDelegate extends ViewAndroidDelegate {
    /**
     * An interface delegates a {@link CallbackHelper} for cursor update. see more in {@link
     * ContentViewPointerTypeTest.OnCursorUpdateHelperImpl}.
     */
    public interface OnCursorUpdateHelper {
        /**
         * Record the last notifyCalled pointer type, see more {@link CallbackHelper#notifyCalled}.
         * @param type The pointer type of the notifyCalled.
         */
        void notifyCalled(int type);
    }

    private @Nullable OnCursorUpdateHelper mOnCursorUpdateHelper;

    public ShellViewAndroidDelegate(ViewGroup containerView) {
        super(containerView);
    }

    public void setOnCursorUpdateHelper(OnCursorUpdateHelper helper) {
        mOnCursorUpdateHelper = helper;
    }

    public @Nullable OnCursorUpdateHelper getOnCursorUpdateHelper() {
        return mOnCursorUpdateHelper;
    }

    @Override
    public void onCursorChangedToCustom(Bitmap customCursorBitmap, int hotspotX, int hotspotY) {
        super.onCursorChangedToCustom(customCursorBitmap, hotspotX, hotspotY);
        if (mOnCursorUpdateHelper != null) {
            mOnCursorUpdateHelper.notifyCalled(CursorType.CUSTOM);
        }
    }

    @Override
    public void onCursorChanged(int cursorType) {
        super.onCursorChanged(cursorType);
        if (mOnCursorUpdateHelper != null) {
            mOnCursorUpdateHelper.notifyCalled(cursorType);
        }
    }
}
