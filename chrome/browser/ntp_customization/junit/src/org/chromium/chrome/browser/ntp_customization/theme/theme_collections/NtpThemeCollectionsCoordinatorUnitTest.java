// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.theme_collections;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.THEME_COLLECTIONS;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType;
import org.chromium.chrome.browser.ntp_customization.R;

/** Unit tests for {@link NtpThemeCollectionsCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpThemeCollectionsCoordinatorUnitTest {

    private static final String TEST_COLLECTION_TITLE = "Test Collection";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetDelegate mBottomSheetDelegate;
    @Mock private NtpSingleThemeCollectionCoordinator mNtpSingleThemeCollectionCoordinator;

    private NtpThemeCollectionsCoordinator mCoordinator;
    private Context mContext;
    private View mBottomSheetView;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);

        mCoordinator = new NtpThemeCollectionsCoordinator(mContext, mBottomSheetDelegate);

        ArgumentCaptor<View> viewCaptor = ArgumentCaptor.forClass(View.class);
        verify(mBottomSheetDelegate)
                .registerBottomSheetLayout(eq(THEME_COLLECTIONS), viewCaptor.capture());
        mBottomSheetView = viewCaptor.getValue();
    }

    @Test
    public void testConstructor() {
        assertNotNull(mBottomSheetView);
    }

    @Test
    public void testBackButton() {
        View backButton = mBottomSheetView.findViewById(R.id.back_button);
        assertNotNull(backButton);
        assertTrue(backButton.hasOnClickListeners());

        backButton.performClick();

        verify(mBottomSheetDelegate).showBottomSheet(eq(THEME));
    }

    @Test
    public void testLearnMoreButton() {
        View learnMoreButton = mBottomSheetView.findViewById(R.id.learn_more_button);
        assertNotNull(learnMoreButton);
        assertTrue(learnMoreButton.hasOnClickListeners());
    }

    @Test
    public void testBuildRecyclerView() {
        RecyclerView recyclerView =
                mBottomSheetView.findViewById(R.id.theme_collections_recycler_view);
        assertNotNull(recyclerView);

        // Verify LayoutManager
        assertTrue(recyclerView.getLayoutManager() instanceof GridLayoutManager);
        assertEquals(2, ((GridLayoutManager) recyclerView.getLayoutManager()).getSpanCount());

        // Verify Adapter
        assertTrue(recyclerView.getAdapter() instanceof NtpThemeCollectionsAdapter);
    }

    @Test
    public void testDestroy() {
        View backButton = mBottomSheetView.findViewById(R.id.back_button);
        ImageView learnMoreButton = mBottomSheetView.findViewById(R.id.learn_more_button);
        RecyclerView recyclerView =
                mBottomSheetView.findViewById(R.id.theme_collections_recycler_view);
        NtpThemeCollectionsAdapter adapter = (NtpThemeCollectionsAdapter) recyclerView.getAdapter();
        NtpThemeCollectionsAdapter spiedAdapter = spy(adapter);
        mCoordinator.setNtpThemeCollectionsAdapterForTesting(spiedAdapter);
        mCoordinator.setNtpSingleThemeCollectionCoordinatorForTesting(
                mNtpSingleThemeCollectionCoordinator);

        assertTrue(backButton.hasOnClickListeners());
        assertTrue(learnMoreButton.hasOnClickListeners());
        assertNotNull(mCoordinator.getNtpSingleThemeCollectionCoordinatorForTesting());

        mCoordinator.destroy();

        assertFalse(backButton.hasOnClickListeners());
        assertFalse(learnMoreButton.hasOnClickListeners());
        verify(spiedAdapter).clearOnClickListeners();
        verify(mNtpSingleThemeCollectionCoordinator).destroy();
    }

    @Test
    public void testHandleThemeCollectionClick() {
        // Create a fake view for the collection item.
        View fakeThemeCollectionView =
                LayoutInflater.from(mContext)
                        .inflate(
                                R.layout.ntp_customization_theme_collections_list_item_layout,
                                null);
        TextView titleView = fakeThemeCollectionView.findViewById(R.id.theme_collection_title);
        titleView.setText(TEST_COLLECTION_TITLE);

        // On first click, a new single theme coordinator is created and the sheet is shown.
        assertNull(mCoordinator.getNtpSingleThemeCollectionCoordinatorForTesting());
        mCoordinator.handleThemeCollectionClick(fakeThemeCollectionView);
        assertNotNull(mCoordinator.getNtpSingleThemeCollectionCoordinatorForTesting());
        verify(mBottomSheetDelegate).showBottomSheet(eq(BottomSheetType.SINGLE_THEME_COLLECTION));

        // On second click, the existing single theme coordinator is updated and the sheet is shown.
        mCoordinator.setNtpSingleThemeCollectionCoordinatorForTesting(
                mNtpSingleThemeCollectionCoordinator);
        mCoordinator.handleThemeCollectionClick(fakeThemeCollectionView);
        verify(mNtpSingleThemeCollectionCoordinator)
                .updateThemeCollection(eq(TEST_COLLECTION_TITLE));
        verify(mBottomSheetDelegate, times(2))
                .showBottomSheet(eq(BottomSheetType.SINGLE_THEME_COLLECTION));
    }
}
