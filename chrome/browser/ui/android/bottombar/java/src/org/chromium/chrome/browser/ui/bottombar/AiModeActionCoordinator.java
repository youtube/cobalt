// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.bottombar;

import android.app.Activity;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Coordinator for the AI Mode action in the bottom bar. */
@NullMarked
public class AiModeActionCoordinator {
    private final NullableObservableSupplier<Tab> mTabSupplier;
    private final @Nullable NullableObservableSupplier<PropertyModel> mAiModeActionModelSupplier;
    private final Callback<@Nullable PropertyModel> mModelCallback = this::onModelChanged;
    private final UserEducationHelper mUserEducationHelper;

    /**
     * Constructs a new {@link AiModeActionCoordinator}.
     *
     * @param activity The current Android activity.
     * @param actionRegistry The registry containing registered bottom bar actions.
     * @param tabSupplier Supplier of the currently active tab.
     * @param userEducationHelper The {@link UserEducationHelper} for showing IPHs.
     */
    public AiModeActionCoordinator(
            Activity activity,
            ActionRegistry actionRegistry,
            NullableObservableSupplier<Tab> tabSupplier,
            UserEducationHelper userEducationHelper) {
        mTabSupplier = tabSupplier;
        mUserEducationHelper = userEducationHelper;
        mAiModeActionModelSupplier = actionRegistry.get(ActionId.AI_MODE);
        if (mAiModeActionModelSupplier != null) {
            mAiModeActionModelSupplier.addSyncObserverAndCallIfNonNull(mModelCallback);
        }
    }

    private void onModelChanged(@Nullable PropertyModel model) {
        if (model == null) return;
        model.set(ActionProperties.ON_PRESS_CALLBACK, this::onAiModePressed);
        model.set(ActionProperties.USER_EDUCATION_HELPER, mUserEducationHelper);
    }

    private void onAiModePressed(View view) {
        Tab currentTab = mTabSupplier.get();
        if (currentTab == null) return;

        Profile profile = currentTab.getProfile();
        GURL url = TemplateUrlServiceFactory.getForProfile(profile).getComposeplateUrl();
        if (url != null && url.isValid() && !url.isEmpty()) {
            currentTab.loadUrl(new LoadUrlParams(url.getSpec()));
        }
    }

    /** Clean up observers to prevent memory leaks. */
    public void destroy() {
        if (mAiModeActionModelSupplier != null) {
            mAiModeActionModelSupplier.removeObserver(mModelCallback);
        }
    }
}
