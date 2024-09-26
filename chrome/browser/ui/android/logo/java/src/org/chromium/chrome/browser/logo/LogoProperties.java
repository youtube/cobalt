// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.logo;

import android.graphics.Bitmap;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/** The properties required to build the logo on start surface or ntp.*/
interface LogoProperties {
    // TODO(crbug.com/1394983): It doesn't really make sense for those
    //  WritableObjectPropertyKey<Boolean> with skipEquality equals to true property keys;
    //  if we're not going to read the value out of this in the ViewBinder.
    WritableFloatPropertyKey ALPHA = new WritableFloatPropertyKey();
    WritableIntPropertyKey LOGO_TOP_MARGIN = new WritableIntPropertyKey();
    WritableIntPropertyKey LOGO_BOTTOM_MARGIN = new WritableIntPropertyKey();
    WritableObjectPropertyKey<Boolean> SET_END_FADE_ANIMATION =
            new WritableObjectPropertyKey<>(true /* skipEquality */);
    // TODO(crbug.com/1394983): Change the VISIBILITY properties to some sort of state
    //  enum if possible.
    WritableBooleanPropertyKey VISIBILITY = new WritableBooleanPropertyKey();
    WritableBooleanPropertyKey ANIMATION_ENABLED = new WritableBooleanPropertyKey();
    WritableObjectPropertyKey<LogoView.ClickHandler> LOGO_CLICK_HANDLER =
            new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Boolean> SHOW_SEARCH_PROVIDER_INITIAL_VIEW =
            new WritableObjectPropertyKey<>(true /* skipEquality */);
    // TODO(crbug.com/1394983): Generate the LOGO, DEFAULT_GOOGLE_LOGO and ANIMATED_LOGO properties
    //  into one property that takes an object generic/powerful enough to represent all three of
    //  these if possible.
    WritableObjectPropertyKey<LogoBridge.Logo> LOGO = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Bitmap> DEFAULT_GOOGLE_LOGO = new WritableObjectPropertyKey<>();
    WritableObjectPropertyKey<Boolean> SHOW_LOADING_VIEW =
            new WritableObjectPropertyKey<>(true /* skipEquality */);
    WritableObjectPropertyKey<BaseGifImage> ANIMATED_LOGO = new WritableObjectPropertyKey<>();

    PropertyKey[] ALL_KEYS = new PropertyKey[] {ALPHA, LOGO_TOP_MARGIN, LOGO_BOTTOM_MARGIN,
            SET_END_FADE_ANIMATION, VISIBILITY, ANIMATION_ENABLED, LOGO_CLICK_HANDLER,
            SHOW_SEARCH_PROVIDER_INITIAL_VIEW, LOGO, DEFAULT_GOOGLE_LOGO, SHOW_LOADING_VIEW,
            ANIMATED_LOGO};
}
