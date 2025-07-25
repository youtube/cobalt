// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import androidx.annotation.NonNull;

/**
 * Factory for creating {@link HubLayoutAnimatorProvider}s for translate animations. These
 * animations are primiarly used for large form factor devices.
 */
public class TranslateHubLayoutAnimationFactory {

    /**
     * Creates a {@link HubLayoutAnimatorProvider} for translating up to show.
     *
     * @param hubLayoutContainerView The {@link HubContainerView} to animate.
     * @param scrimController A controller to manage showing a scrim.
     * @param durationMs The duration in milliseconds of the animation.
     * @return a {@link HubLayoutAnimatorProvider} that provides the animation.
     */
    public static HubLayoutAnimatorProvider createTranslateUpAnimatorProvider(
            @NonNull HubContainerView hubContainerView,
            @NonNull ScrimController scrimController,
            long durationMs) {
        return TranslateHubLayoutAnimationFactoryImpl.createTranslateUpAnimatorProvider(
                hubContainerView, scrimController, durationMs);
    }

    /**
     * Creates a {@link HubLayoutAnimatorProvider} for translating down to hide.
     *
     * @param hubLayoutContainerView The {@link HubContainerView} to animate.
     * @param scrimController A controller to manage hiding a scrim.
     * @param durationMs The duration in milliseconds of the animation.
     * @return a {@link HubLayoutAnimatorProvider} that provides the animation.
     */
    public static HubLayoutAnimatorProvider createTranslateDownAnimatorProvider(
            @NonNull HubContainerView hubContainerView,
            @NonNull ScrimController scrimController,
            long durationMs) {
        return TranslateHubLayoutAnimationFactoryImpl.createTranslateDownAnimatorProvider(
                hubContainerView, scrimController, durationMs);
    }
}
