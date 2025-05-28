// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;

import androidx.annotation.NonNull;
import androidx.appsearch.app.AppSearchSession;
import androidx.appsearch.app.SearchResult;
import androidx.appsearch.app.SearchResults;
import androidx.appsearch.app.SetSchemaResponse;
import androidx.appsearch.app.SetSchemaResponse.MigrationFailure;
import androidx.appsearch.builtintypes.GlobalSearchApplicationInfo;
import androidx.appsearch.builtintypes.WebPage;
import androidx.appsearch.exceptions.AppSearchException;
import androidx.test.filters.SmallTest;

import com.google.common.util.concurrent.ListenableFuture;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for AuxiliarySearchDonor. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressWarnings("DoNotMock") // Mock ListenableFuture.
public class AuxiliarySearchDonorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MigrationFailure mMigrationFailure;
    @Mock private AuxiliarySearchHooks mHooks;
    @Mock private ListenableFuture<AppSearchSession> mAppSearchSession;
    @Mock private AppSearchSession mSession;
    @Mock private Callback<Boolean> mCallback;

    private AuxiliarySearchDonor mAuxiliarySearchDonor;

    @Before
    public void setUp() {
        when(mHooks.isEnabled()).thenReturn(true);
        when(mHooks.isSettingDefaultEnabledByOs()).thenReturn(true);
        AuxiliarySearchControllerFactory.getInstance().setHooksForTesting(mHooks);
        assertTrue(AuxiliarySearchControllerFactory.getInstance().isSettingDefaultEnabledByOs());
        assertTrue(AuxiliarySearchUtils.isShareTabsWithOsEnabled());

        AuxiliarySearchDonor.setSkipInitializationForTesting(true);
        mAuxiliarySearchDonor = AuxiliarySearchDonor.createDonorForTesting();
        try {
            when(mAppSearchSession.get()).thenReturn(mSession);
            mAuxiliarySearchDonor.setAppSearchSessionForTesting(mAppSearchSession);
        } catch (Exception e) {
            // Just continue.
        }
    }

    @Test
    @SmallTest
    public void testCreateSessionAndInit() {
        // #createSessionAndInit() has been called in AuxiliarySearchDonor's constructor.
        // Verifies that calling createSessionAndInit() again will early exit.
        assertFalse(mAuxiliarySearchDonor.createSessionAndInit());
        assertTrue(mAuxiliarySearchDonor.getIsCreatedSessionAndInitForTesting());
    }

    @Test
    @SmallTest
    public void testCreateSessionAndInit_DefaultDisabled() {
        when(mHooks.isSettingDefaultEnabledByOs()).thenReturn(false);
        assertFalse(AuxiliarySearchControllerFactory.getInstance().isSettingDefaultEnabledByOs());

        mAuxiliarySearchDonor = AuxiliarySearchDonor.createDonorForTesting();
        assertTrue(mAuxiliarySearchDonor.getIsCreatedSessionAndInitForTesting());
    }

    @Test
    @SmallTest
    public void testDefaultTtlIsNotZero() {
        assertNotEquals(0L, mAuxiliarySearchDonor.getDocumentTtlMs());
        assertEquals(
                ((long) AuxiliarySearchUtils.DEFAULT_TTL_HOURS) * 60 * 60 * 1000,
                mAuxiliarySearchDonor.getDocumentTtlMs());
    }

    @Test
    @SmallTest
    @EnableFeatures("AndroidAppIntegration:content_ttl_hours/0")
    public void testConfiguredTtlCannotBeZero() {
        assertNotEquals(0L, mAuxiliarySearchDonor.getDocumentTtlMs());
        assertEquals(
                ((long) AuxiliarySearchUtils.DEFAULT_TTL_HOURS) * 60 * 60 * 1000,
                mAuxiliarySearchDonor.getDocumentTtlMs());
    }

    @Test
    @SmallTest
    public void testBuildDocument() {
        int id = 10;
        String url = "Url";
        String title = "Title";
        long lastAccessTimeStamp = 100;
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Config.RGB_565);
        String documentId = "Tab-10";
        assertEquals(documentId, AuxiliarySearchDonor.getDocumentId(id));

        WebPage webPage =
                mAuxiliarySearchDonor.buildDocument(id, url, title, lastAccessTimeStamp, bitmap);

        assertEquals(documentId, webPage.getId());
        assertEquals(url, webPage.getUrl());
        assertEquals(title, webPage.getName());
        assertEquals(lastAccessTimeStamp, webPage.getCreationTimestampMillis());
        assertEquals(mAuxiliarySearchDonor.getDocumentTtlMs(), webPage.getDocumentTtlMillis());
        assertTrue(
                Arrays.equals(
                        AuxiliarySearchUtils.bitmapToBytes(bitmap),
                        webPage.getFavicon().getBytes()));
    }

    @Test
    @SmallTest
    public void testOnSetSchemaResponseAvailable() {
        List<MigrationFailure> migrationFailures = new ArrayList<MigrationFailure>();
        migrationFailures.add(mMigrationFailure);
        SetSchemaResponse setSchemaResponse =
                new SetSchemaResponse.Builder().addMigrationFailures(migrationFailures).build();

        List<WebPage> pendingDocs = new ArrayList<WebPage>();
        WebPage webPage = new WebPage.Builder("namespace", "Id1").setUrl("Url1").build();
        pendingDocs.add(webPage);
        mAuxiliarySearchDonor.setPendingDocumentsForTesting(pendingDocs);
        mAuxiliarySearchDonor.resetSchemaSetForTesting();

        // Verifies that the pending donation isn't executed if setSchema is failed.
        mAuxiliarySearchDonor.onSetSchemaResponseAvailable(setSchemaResponse);
        assertNotNull(mAuxiliarySearchDonor.getPendingDocumentsForTesting());
        assertFalse(mAuxiliarySearchDonor.getIsSchemaSetForTesting());

        migrationFailures.clear();
        setSchemaResponse =
                new SetSchemaResponse.Builder().addMigrationFailures(migrationFailures).build();

        // Verifies that the pending donation will be executed if setSchema succeeds.
        mAuxiliarySearchDonor.onSetSchemaResponseAvailable(setSchemaResponse);
        assertNull(mAuxiliarySearchDonor.getPendingDocumentsForTesting());
        assertTrue(mAuxiliarySearchDonor.getIsSchemaSetForTesting());
    }

    @Test
    @SmallTest
    public void testSharedPreferenceKeyIsUpdated() {
        SetSchemaResponse setSchemaResponse = new SetSchemaResponse.Builder().build();
        assertTrue(setSchemaResponse.getMigrationFailures().isEmpty());

        SharedPreferencesManager chromeSharedPreferences = ChromeSharedPreferences.getInstance();
        assertFalse(mAuxiliarySearchDonor.getIsSchemaSetForTesting());
        assertFalse(
                chromeSharedPreferences.readBoolean(
                        ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET, false));

        // Verifies that ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET is set to true after
        // the schema is set successful.
        mAuxiliarySearchDonor.onSetSchemaResponseAvailable(setSchemaResponse);
        assertTrue(mAuxiliarySearchDonor.getIsSchemaSetForTesting());
        assertTrue(
                chromeSharedPreferences.readBoolean(
                        ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET, false));

        chromeSharedPreferences.removeKey(ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET);
    }

    @Test
    @SmallTest
    public void testDoNotSetSchemaAgain() {
        mAuxiliarySearchDonor.resetSchemaSetForTesting();
        SharedPreferencesManager chromeSharedPreferences = ChromeSharedPreferences.getInstance();
        chromeSharedPreferences.writeBoolean(
                ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET, true);
        assertFalse(mAuxiliarySearchDonor.getIsSchemaSetForTesting());

        // Verifies that #onConsumerSchemaSearchedImpl() returns false, i.e., not to set the schema
        // again if it has been set.
        assertFalse(mAuxiliarySearchDonor.onConsumerSchemaSearchedImpl(/* success= */ true));
        assertTrue(mAuxiliarySearchDonor.getIsSchemaSetForTesting());

        chromeSharedPreferences.removeKey(ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET);
    }

    @Test
    @SmallTest
    public void testOnConfigChanged() {
        Callback<Boolean> callback = Mockito.mock(Callback.class);
        assertTrue(mAuxiliarySearchDonor.getSharedTabsWithOsStateForTesting());
        assertTrue(AuxiliarySearchUtils.isShareTabsWithOsEnabled());

        mAuxiliarySearchDonor.onConfigChanged(false, callback);
        assertFalse(mAuxiliarySearchDonor.getSharedTabsWithOsStateForTesting());
        assertFalse(AuxiliarySearchUtils.isShareTabsWithOsEnabled());

        mAuxiliarySearchDonor.onConfigChanged(true, callback);
        assertTrue(mAuxiliarySearchDonor.getSharedTabsWithOsStateForTesting());
        assertTrue(AuxiliarySearchUtils.isShareTabsWithOsEnabled());
    }

    @Test
    @SmallTest
    public void testCanDonate() {
        mAuxiliarySearchDonor.setSharedTabsWithOsStateForTesting(
                /* sharedTabsWithOsState= */ false);
        assertFalse(mAuxiliarySearchDonor.canDonate());

        mAuxiliarySearchDonor.setSharedTabsWithOsStateForTesting(/* sharedTabsWithOsState= */ true);
        mAuxiliarySearchDonor.onConsumerSchemaSearchedImpl(/* success= */ false);
        assertFalse(mAuxiliarySearchDonor.canDonate());

        mAuxiliarySearchDonor.onConsumerSchemaSearchedImpl(/* success= */ true);
        assertTrue(mAuxiliarySearchDonor.canDonate());
    }

    @Test
    @SmallTest
    public void testOnConsumerSchemaSearchedImpl() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET, true);
        Callback<Boolean> callback = Mockito.mock(Callback.class);
        mAuxiliarySearchDonor.setPendingCallbackForTesting(callback);

        // Verifies that closeSession() which calls the pending callback is executed when device
        // isn't capable for Tabs donation while previously donation was allowed (schema is set).
        assertFalse(mAuxiliarySearchDonor.onConsumerSchemaSearchedImpl(/* success= */ false));
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND,
                                false));
        verify(callback).onResult(eq(false));

        // Verifies that onConsumerSchemaSearchedImpl() doesn't reset the schema thus returns
        // false, while AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND is set to be true.
        assertFalse(mAuxiliarySearchDonor.onConsumerSchemaSearchedImpl(/* success= */ true));
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND,
                                false));

        // Verifies that onConsumerSchemaSearchedImpl() returns true to set the schema, and
        // AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND is set to be true.
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.AUXILIARY_SEARCH_IS_SCHEMA_SET, false);
        assertTrue(mAuxiliarySearchDonor.onConsumerSchemaSearchedImpl(/* success= */ true));
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.AUXILIARY_SEARCH_CONSUMER_SCHEMA_FOUND,
                                false));
    }

    @Test
    @SmallTest
    public void testIterateSearchResults() {
        SearchResults searchresults = Mockito.mock(SearchResults.class);
        List<SearchResult> page = new ArrayList<>();
        assertTrue(page.isEmpty());

        mAuxiliarySearchDonor.iterateSearchResults(searchresults, page, mCallback);
        verify(mCallback).onResult(eq(false));

        SearchResult searchResult1 =
                createSearchResult(
                        GlobalSearchApplicationInfo.APPLICATION_TYPE_PRODUCER,
                        AuxiliarySearchDonor.SCHEMA_WEBPAGE);
        SearchResult searchResult2 =
                createSearchResult(GlobalSearchApplicationInfo.APPLICATION_TYPE_CONSUMER, "Schema");
        SearchResult searchResult3 =
                createSearchResult(
                        GlobalSearchApplicationInfo.APPLICATION_TYPE_CONSUMER,
                        AuxiliarySearchDonor.SCHEMA_WEBPAGE);

        page.add(searchResult1);
        mAuxiliarySearchDonor.iterateSearchResults(searchresults, page, mCallback);
        verify(mCallback, times(2)).onResult(eq(false));

        page.add(searchResult2);
        mAuxiliarySearchDonor.iterateSearchResults(searchresults, page, mCallback);
        verify(mCallback, times(3)).onResult(eq(false));

        page.add(searchResult3);
        mAuxiliarySearchDonor.iterateSearchResults(searchresults, page, mCallback);
        verify(mCallback).onResult(eq(true));
    }

    @Test
    @SmallTest
    public void testIsShareTabsWithOsEnabledKeyExist() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.SHARING_TABS_WITH_OS);

        assertFalse(mAuxiliarySearchDonor.isShareTabsWithOsEnabledKeyExist());

        prefsManager.writeBoolean(ChromePreferenceKeys.SHARING_TABS_WITH_OS, true);
        assertTrue(mAuxiliarySearchDonor.isShareTabsWithOsEnabledKeyExist());

        prefsManager.writeBoolean(ChromePreferenceKeys.SHARING_TABS_WITH_OS, false);
        assertTrue(mAuxiliarySearchDonor.isShareTabsWithOsEnabledKeyExist());
    }

    private SearchResult createSearchResult(int applicationType, @NonNull String schemaType) {
        GlobalSearchApplicationInfo appInfo =
                new GlobalSearchApplicationInfo.Builder("namespace", "id", applicationType)
                        .setSchemaTypes(Arrays.asList(schemaType))
                        .build();
        try {
            return new SearchResult.Builder("package", "database").setDocument(appInfo).build();
        } catch (AppSearchException e) {
            return null;
        }
    }
}
