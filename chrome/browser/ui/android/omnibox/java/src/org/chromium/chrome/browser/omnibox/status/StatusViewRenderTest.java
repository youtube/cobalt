// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.status;

import static org.mockito.Mockito.doReturn;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.core.content.res.ResourcesCompat;
import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.PermissionIconResource;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.chrome.browser.toolbar.LocationBarModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ToolbarUnitTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.widget.CompositeTouchDelegate;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.io.IOException;

/**
 * Render tests for {@link StatusView}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class StatusViewRenderTest extends BlankUiTestActivityTestCase {
    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_OMNIBOX)
                    .build();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    SearchEngineLogoUtils mSearchEngineLogoUtils;

    private StatusView mStatusView;
    private PropertyModel mStatusModel;
    private LocationBarModel mLocationBarModel;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);

        doReturn(true).when(mSearchEngineLogoUtils).shouldShowSearchEngineLogo(false);

        runOnUiThreadBlocking(() -> {
            ViewGroup view = new LinearLayout(getActivity());

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);

            getActivity().setContentView(view, params);

            mStatusView = getActivity()
                                  .getLayoutInflater()
                                  .inflate(R.layout.location_status, view, true)
                                  .findViewById(R.id.location_bar_status);
            mStatusView.setCompositeTouchDelegate(new CompositeTouchDelegate(view));
            // clang-format off
            mLocationBarModel = new LocationBarModel(mStatusView.getContext(),
                    NewTabPageDelegate.EMPTY, url -> url.getSpec(), window -> null,
                    ToolbarUnitTestUtils.OFFLINE_STATUS, mSearchEngineLogoUtils);
            // clang-format on
            mLocationBarModel.setTab(null, /*  incognito= */ false);
            mStatusModel = new PropertyModel.Builder(StatusProperties.ALL_KEYS).build();
            PropertyModelChangeProcessor.create(mStatusModel, mStatusView, new StatusViewBinder());

            // Increases visibility for manual parsing of diffs. Status view matches the parent
            // height, so this white will stretch vertically.
            mStatusView.setBackgroundColor(Color.WHITE);
        });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStatusViewIncognitoWithIcon() throws IOException {
        runOnUiThreadBlocking(() -> {
            mLocationBarModel.setTab(null, /*  incognito= */ true);
            mStatusView.setIncognitoBadgeVisibility(true);
            mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE,
                    new StatusIconResource(R.drawable.ic_search, 0));
        });
        mRenderTestRule.render(mStatusView, "status_view_incognito_with_icon");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStatusViewIncognitoNoIcon() throws IOException {
        runOnUiThreadBlocking(() -> {
            mLocationBarModel.setTab(null, /*  incognito= */ true);
            mStatusView.setIncognitoBadgeVisibility(true);
            mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE, null);
        });
        mRenderTestRule.render(mStatusView, "status_view_incognito_no_icon");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStatusViewWithIcon() throws IOException {
        runOnUiThreadBlocking(() -> {
            mStatusModel.set(StatusProperties.STATUS_ICON_ALPHA, 1f);
            mStatusModel.set(StatusProperties.SHOW_STATUS_ICON, true);
            mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE,
                    new StatusIconResource(R.drawable.ic_search, 0));
        });
        mRenderTestRule.render(mStatusView, "status_view_with_icon");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStatusViewWithLocationPermissionIcon() throws IOException {
        runOnUiThreadBlocking(() -> {
            Drawable locationIcon =
                    ContentSettingsResources.getIconForOmnibox(mStatusView.getContext(),
                            ContentSettingsType.GEOLOCATION, ContentSettingValues.ALLOW, false);
            PermissionIconResource statusIcon = new PermissionIconResource(locationIcon, false);
            statusIcon.setTransitionType(StatusView.IconTransitionType.ROTATE);
            mStatusModel.set(StatusProperties.STATUS_ICON_ALPHA, 1f);
            mStatusModel.set(StatusProperties.SHOW_STATUS_ICON, true);
            mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE, statusIcon);
        });
        mRenderTestRule.render(mStatusView, "status_view_with_location_permission_icon");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testStatusViewWithStoreIcon() throws IOException {
        runOnUiThreadBlocking(() -> {
            Drawable storeIconDrawable = ResourcesCompat.getDrawable(getActivity().getResources(),
                    R.drawable.ic_storefront_blue, getActivity().getTheme());
            StatusIconResource statusIcon = new PermissionIconResource(storeIconDrawable, false);
            statusIcon.setTransitionType(StatusView.IconTransitionType.ROTATE);
            mStatusModel.set(StatusProperties.STATUS_ICON_ALPHA, 1f);
            mStatusModel.set(StatusProperties.SHOW_STATUS_ICON, true);
            mStatusModel.set(StatusProperties.STATUS_ICON_RESOURCE, statusIcon);
        });
        mRenderTestRule.render(mStatusView, "status_view_with_store_icon");
    }
}
