// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

/**
 * Lists base::Features that can be accessed through {@link MessageFeatureMap}.
 *
 * Should be kept in sync with |kFeaturesExposedToJava| in
 * //components/messages/android/messages_feature.cc
 */
public abstract class MessageFeatureList {
    public static final String MESSAGES_FOR_ANDROID_STACKING_ANIMATION =
            "MessagesForAndroidStackingAnimation";
    public static final String MESSAGES_FOR_ANDROID_FULLY_VISIBLE_CALLBACK =
            "MessagesForAndroidFullyVisibleCallback";

    public static boolean isStackAnimationEnabled() {
        return MessageFeatureMap.isEnabled(MESSAGES_FOR_ANDROID_STACKING_ANIMATION);
    }

    public static boolean isFullyVisibleCallbackEnabled() {
        return MessageFeatureMap.isEnabled(MESSAGES_FOR_ANDROID_FULLY_VISIBLE_CALLBACK);
    }
}
