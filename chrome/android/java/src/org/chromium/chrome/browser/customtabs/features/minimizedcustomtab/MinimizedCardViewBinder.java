// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedCardProperties.FAVICON;
import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedCardProperties.TITLE;
import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedCardProperties.URL;

import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View binder for the minimized card.
 */
public class MinimizedCardViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey key) {
        if (TITLE == key) {
            TextView title = view.findViewById(R.id.title);
            title.setText(model.get(TITLE));
        } else if (URL == key) {
            TextView title = view.findViewById(R.id.url);
            title.setText(model.get(URL));
        } else if (FAVICON == key) {
            ImageView favicon = view.findViewById(R.id.favicon);
            Bitmap bitmap = model.get(FAVICON);
            if (bitmap == null) {
                Drawable icon =
                        UiUtils.getTintedDrawable(
                                view.getContext(),
                                R.drawable.ic_globe_24dp,
                                R.color.default_icon_color_tint_list);
                favicon.setImageDrawable(icon);
            } else {
                favicon.setImageBitmap(model.get(FAVICON));
            }
        }
    }
}
