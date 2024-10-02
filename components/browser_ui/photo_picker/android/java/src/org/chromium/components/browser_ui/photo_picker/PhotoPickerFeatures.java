// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import org.chromium.base.Features;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Provides an API for querying the status of Photo Picker features.
 */
@JNINamespace("photo_picker::features")
public class PhotoPickerFeatures extends Features {
    public static final String MEDIA_PICKER_ADOPTION_NAME = "MediaPickerAdoption";

    // This list must be kept in sync with kFeaturesExposedToJava in native.
    public static final PhotoPickerFeatures ANDROID_MEDIA_PICKER_ADOPTION =
            new PhotoPickerFeatures(0, MEDIA_PICKER_ADOPTION_NAME);

    // The feature param for determining which flavor of the PhotoPicker to adopt.
    private static final String PARAM_ACTION_GET_CONTENT = "use_action_get_content";
    private static final String PARAM_ACTION_PICK_IMAGES = "use_action_pick_images";
    private static final String PARAM_ACTION_PICK_IMAGES_PLUS = "use_action_pick_images_plus";
    private static final String PARAM_CHROME_PICKER_SUPPRESS_BROWSE =
            "chrome_picker_suppress_browse";

    private final int mOrdinal;

    public static boolean launchViaActionGetContent() {
        return ANDROID_MEDIA_PICKER_ADOPTION.getFieldTrialParamByFeatureAsBoolean(
                PARAM_ACTION_GET_CONTENT, false);
    }

    public static boolean launchViaActionPickImages() {
        return ANDROID_MEDIA_PICKER_ADOPTION.getFieldTrialParamByFeatureAsBoolean(
                PARAM_ACTION_PICK_IMAGES, false);
    }

    public static boolean launchViaActionPickImagesPlus() {
        return ANDROID_MEDIA_PICKER_ADOPTION.getFieldTrialParamByFeatureAsBoolean(
                PARAM_ACTION_PICK_IMAGES_PLUS, false);
    }

    public static boolean launchRegularWithoutBrowse() {
        return ANDROID_MEDIA_PICKER_ADOPTION.getFieldTrialParamByFeatureAsBoolean(
                PARAM_CHROME_PICKER_SUPPRESS_BROWSE, false);
    }

    private PhotoPickerFeatures(int ordinal, String name) {
        super(name);
        mOrdinal = ordinal;
    }

    @Override
    protected long getFeaturePointer() {
        return PhotoPickerFeaturesJni.get().getFeature(mOrdinal);
    }

    @NativeMethods
    interface Natives {
        long getFeature(int ordinal);
    }
}
