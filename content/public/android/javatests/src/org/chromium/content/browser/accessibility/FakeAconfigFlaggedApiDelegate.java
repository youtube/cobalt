// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility;

import android.os.Build;
import android.os.Bundle;
import android.util.Pair;

import androidx.annotation.RequiresApi;
import androidx.core.view.accessibility.AccessibilityNodeInfoCompat;

import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.ServiceImpl;

// This class provides a mock implementation for extended selection. For
// simplicity this class keeps track of only one extended selection.
@NullMarked
@ServiceImpl(AconfigFlaggedApiDelegate.class)
public class FakeAconfigFlaggedApiDelegate implements AconfigFlaggedApiDelegate {
    private int mStartVirtualDescendantId = -1;
    private int mStartOffset;
    private int mEndVirtualDescendantId;
    private int mEndOffset;

    @Override
    public void setSelection(
            AccessibilityNodeInfoCompat info,
            android.view.View view,
            int startVirtualDescendantId,
            int startOffset,
            int endVirtualDescendantId,
            int endOffset) {
        mStartVirtualDescendantId = startVirtualDescendantId;
        mStartOffset = startOffset;
        mEndVirtualDescendantId = endVirtualDescendantId;
        mEndOffset = endOffset;
    }

    @Override
    public void clearSelection(AccessibilityNodeInfoCompat info) {
        mStartVirtualDescendantId = -1;
    }

    @Override
    public @Nullable Pair<Integer, Integer> getExtendedSelectionStart(
            AccessibilityNodeInfoCompat info) {
        return mStartVirtualDescendantId == -1
                ? null
                : new Pair<>(mStartVirtualDescendantId, mStartOffset);
    }

    @Override
    public @Nullable Pair<Integer, Integer> getExtendedSelectionEnd(
            AccessibilityNodeInfoCompat info) {
        return mStartVirtualDescendantId == -1
                ? null
                : new Pair<>(mEndVirtualDescendantId, mEndOffset);
    }

    // In production code, this function and the next ones also check
    // `android.view.accessibility.ExportedFlags.a11ySelectionPositionAppGettersApi()` which is not
    // available here. If the test runs on a device with SDK 36.1 but not the flag, the test will
    // fail.
    @Override
    public boolean isActionSetExtendedSelectionSupported() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.BAKLAVA
                && Build.VERSION.SDK_INT_FULL >= Build.VERSION_CODES_FULL.BAKLAVA_1;
    }

    @Override
    public @Nullable Pair<Integer, Integer> getActionSetExtendedSelectionStartArgument(
            Bundle arguments) {
        if (arguments == null) return null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.BAKLAVA
                && Build.VERSION.SDK_INT_FULL >= Build.VERSION_CODES_FULL.BAKLAVA_1) {
            return Api36Helper.getSelectionStart(arguments);
        }
        return null;
    }

    @Override
    public @Nullable Pair<Integer, Integer> getActionSetExtendedSelectionEndArgument(
            Bundle arguments) {
        if (arguments == null) return null;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.BAKLAVA
                && Build.VERSION.SDK_INT_FULL >= Build.VERSION_CODES_FULL.BAKLAVA_1) {
            return Api36Helper.getSelectionEnd(arguments);
        }
        return null;
    }

    @RequiresApi(Build.VERSION_CODES_FULL.BAKLAVA_1)
    private static class Api36Helper {
        public static @Nullable Pair<Integer, Integer> getSelectionStart(Bundle arguments) {
            android.view.accessibility.AccessibilityNodeInfo.Selection selection =
                    arguments.getParcelable(
                            android.view.accessibility.AccessibilityNodeInfo
                                    .ACTION_ARGUMENT_SELECTION_PARCELABLE,
                            android.view.accessibility.AccessibilityNodeInfo.Selection.class);
            if (selection == null) {
                selection =
                        arguments.getParcelable(
                                androidx.core.view.accessibility.AccessibilityNodeInfoCompat
                                        .ACTION_ARGUMENT_SELECTION_PARCELABLE,
                                android.view.accessibility.AccessibilityNodeInfo.Selection.class);
            }
            if (selection != null) {
                return new Pair<>(
                        selection.getStart().getVirtualDescendantId(),
                        selection.getStart().getOffset());
            }
            return null;
        }

        public static @Nullable Pair<Integer, Integer> getSelectionEnd(Bundle arguments) {
            android.view.accessibility.AccessibilityNodeInfo.Selection selection =
                    arguments.getParcelable(
                            android.view.accessibility.AccessibilityNodeInfo
                                    .ACTION_ARGUMENT_SELECTION_PARCELABLE,
                            android.view.accessibility.AccessibilityNodeInfo.Selection.class);
            if (selection == null) {
                selection =
                        arguments.getParcelable(
                                androidx.core.view.accessibility.AccessibilityNodeInfoCompat
                                        .ACTION_ARGUMENT_SELECTION_PARCELABLE,
                                android.view.accessibility.AccessibilityNodeInfo.Selection.class);
            }
            if (selection != null) {
                return new Pair<>(
                        selection.getEnd().getVirtualDescendantId(),
                        selection.getEnd().getOffset());
            }
            return null;
        }
    }
}
