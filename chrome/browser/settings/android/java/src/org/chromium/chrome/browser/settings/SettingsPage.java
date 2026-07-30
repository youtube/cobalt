// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.embedder_support.util.UrlConstants;

/** A native page holding the Chrome settings UI in a tab. */
@NullMarked
public class SettingsPage extends BasicNativePage {
    private final String mTitle;
    private final FrameLayout mContentView;

    /**
     * Create a new instance of the settings page.
     *
     * @param activity The current {@link Activity} used to obtain resources or inflate views.
     * @param profile The Profile associated with the settings UI.
     * @param host A NativePageHost to load urls.
     */
    public SettingsPage(Activity activity, Profile profile, NativePageHost host) {
        super(host);

        mTitle = activity.getString(R.string.settings);
        mContentView = new FrameLayout(activity);
        mContentView.setId(View.generateViewId());

        // TODO(crbug.com/521895796): Add actual settings UI here. For now, just add a placeholder.
        TextView label = new TextView(activity);
        label.setText(R.string.settings);
        label.setTextSize(16.f);
        label.setTextColor(android.graphics.Color.BLACK);
        FrameLayout.LayoutParams layoutParams =
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.WRAP_CONTENT,
                        FrameLayout.LayoutParams.WRAP_CONTENT);
        layoutParams.setMargins(48, 48, 48, 48);
        label.setLayoutParams(layoutParams);
        mContentView.addView(label);

        initWithView(mContentView);
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public String getHost() {
        return UrlConstants.SETTINGS_HOST;
    }
}
