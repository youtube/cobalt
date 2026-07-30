// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.util.ArrayMap;

import androidx.annotation.IntDef;
import androidx.annotation.Px;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.ElementType;
import java.lang.annotation.Target;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * Coordinator for "side UI," with "side UI" referring to views that will anchor to either the left
 * or right side of the main browser window.
 */
@NullMarked
public interface SideUiCoordinator extends SideUiStateProvider {

    /**
     * Minimum width (in dp) reserved for {@code WebContents} when calculating {@link SideUiSpecs}
     * and determining {@link SideUiContainer}s' visibility.
     */
    int MIN_WEB_CONTENTS_WIDTH_DP = 412;

    /**
     * The IDs assigned to known {@link SideUiContainer}s listed in descending order of their
     * priorities by which they consume available space. The smaller number indicates higher
     * priority.
     */
    @IntDef({
        SideUiId.VERTICAL_TABS,
        SideUiId.SIDE_PANEL,
        SideUiId.SIDE_UI_FOR_TESTING_HIGH_PRIORITY,
        SideUiId.SIDE_UI_FOR_TESTING_LOW_PRIORITY
    })
    @Target(ElementType.TYPE_USE)
    @interface SideUiId {
        int VERTICAL_TABS = 0;
        int SIDE_PANEL = 1;
        int SIDE_UI_FOR_TESTING_HIGH_PRIORITY = 2;
        int SIDE_UI_FOR_TESTING_LOW_PRIORITY = 3;
        int NUM_ENTRIES = 4;
    }

    /**
     * The sides of the window that a {@link SideUiContainer} will anchor to. Each value should have
     * a corresponding container view in main_forked_with_secondary_ui_container.xml.
     */
    @IntDef({AnchorSide.LEFT, AnchorSide.RIGHT})
    @Target(ElementType.TYPE_USE)
    @interface AnchorSide {
        int LEFT = 0;
        int RIGHT = 1;
        int NUM_ENTRIES = 2;
    }

    /**
     * POD-type that holds the showability for {@link SideUiContainer}s.
     *
     * <p>What "showability" means:
     *
     * <ul>
     *   <li>Showable: There is enough space to show a {@link SideUiContainer}, but it may not be
     *       actually shown.
     *   <li>Unshowable: There is not enough space to show a {@link SideUiContainer}, and that
     *       container is guaranteed to be hidden.
     * </ul>
     *
     * <p>One use case of showability is using it to control the entry point visibility of a feature
     * that needs a {@link SideUiContainer}.
     */
    final class SideUiShowability {
        /** IDs of showable {@link SideUiContainer}s. */
        public final List<@SideUiId Integer> mShowableSideUiIds;

        /** IDs of unshowable {@link SideUiContainer}s. */
        public final List<@SideUiId Integer> mUnshowableSideUiIds;

        public SideUiShowability(
                List<@SideUiId Integer> showableSideUiIds,
                List<@SideUiId Integer> unshowableSideUiIds) {
            mShowableSideUiIds = List.copyOf(showableSideUiIds);
            mUnshowableSideUiIds = List.copyOf(unshowableSideUiIds);
        }
    }

    /**
     * POD-type that holds the info for a request to reposition or resize a {@link SideUiContainer}.
     *
     * <p>TODO(crbug.com/478338737): Consider renaming this class as "SideUiUpdateRequest".
     */
    final class SideUiContainerProperties {
        final @SideUiId int mSideUiId;
        final @AnchorSide int mAnchorSide;

        public SideUiContainerProperties(@SideUiId int id, @AnchorSide int side) {
            mSideUiId = id;
            mAnchorSide = side;
        }
    }

    /**
     * POD-type that holds the info about the Side UI specs to be used by a {@link SideUiObserver}.
     * Specifically, this holds the widths (in px) for the parent ViewGroups (one for left-anchored
     * UI and one for right-anchored UI for now ) that hold a {@link SideUiContainer}'s View, based
     * on the SideUiContainer's specified {@link AnchorSide}.
     *
     * <p><strong>Note:</strong> This is a passive data spec and does not guarantee that these specs
     * are currently applied to the active UI. To query the actual active UI state, use {@link
     * SideUiStateProvider} instead.
     */
    final class SideUiSpecs {
        /** Maps @AnchorSide to ContainerWidth. */
        private final Map<@AnchorSide Integer, Integer> mSideUiWidths = new ArrayMap<>();

        public SideUiSpecs(Map<@AnchorSide Integer, Integer> sideUiWidths) {
            mSideUiWidths.putAll(sideUiWidths);
        }

        public SideUiSpecs(@Px int leftContainerWidth, @Px int rightContainerWidth) {
            assert leftContainerWidth >= 0;
            assert rightContainerWidth >= 0;
            mSideUiWidths.put(AnchorSide.LEFT, leftContainerWidth);
            mSideUiWidths.put(AnchorSide.RIGHT, rightContainerWidth);
        }

        public int getWidth(@AnchorSide int side) {
            return mSideUiWidths.getOrDefault(side, 0);
        }

        /**
         * Returns all the entries in the SideUiSpecs. Each entry has a mapping from
         * {@link @AnchorSide} to width.
         */
        public Set<Map.Entry<@AnchorSide Integer, Integer>> entrySet() {
            return mSideUiWidths.entrySet();
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (!(obj instanceof SideUiSpecs that)) return false;
            return this.mSideUiWidths.equals(that.mSideUiWidths);
        }

        @Override
        public String toString() {
            return String.format(
                    Locale.ENGLISH,
                    "[LeftContainerWidth: %d, RightContainerWidth: %d]",
                    mSideUiWidths.get(AnchorSide.LEFT),
                    mSideUiWidths.get(AnchorSide.RIGHT));
        }
    }

    /**
     * Registers a {@link SideUiContainer} to be maintained by this coordinator.
     *
     * @param sideUiContainer The {@link SideUiContainer} to register.
     * @throw IllegalArgumentException if the given sideUiContainer has conflicts with the existing
     *     ones, such as duplicated {@link SideUiId} or {@link AnchorSide}.
     */
    void registerSideUiContainer(SideUiContainer sideUiContainer);

    /**
     * Unregisters a {@link SideUiContainer} such that it will no longer be maintained by this
     * coordinator.
     *
     * @param sideUiContainer The {@link SideUiContainer} to unregister.
     */
    void unregisterSideUiContainer(SideUiContainer sideUiContainer);

    /**
     * Requests that the registered {@link SideUiContainer} change its width.
     * <strong>Important:</strong> this should only be called by the feature that owns the affected
     * {@link SideUiContainer}.
     *
     * @param properties The {@link SideUiContainerProperties} that defines the new requested
     *     position for the registered {@link SideUiContainer}.
     * @param suppressAnimations Whether animations should be suppressed for the container update.
     *     If true, the update will happen immediately, without animations.
     * @throw IllegalArgumentException if the given properties comes with an invalid {@link
     *     SideUiId} not found in the registered containers, such as duplicated {@link SideUiId} or
     *     {@link AnchorSide}.
     */
    void requestUpdateContainer(SideUiContainerProperties properties, boolean suppressAnimations);

    /** Destroys all objects owned by this coordinator. */
    void destroy();
}
