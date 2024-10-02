// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcherJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeJni;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.state.PriceDropMetricsLogger.MetricsResult;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.HintsProto;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.nio.ByteBuffer;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Test relating to {@link ShoppingPersistedTabData}
 */
@RunWith(BaseJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.COMMERCE_PRICE_TRACKING + "<Study"})
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "force-fieldtrials=Study/Group"})
public class ShoppingPersistedTabDataTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public JniMocker mMocker = new JniMocker();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Mock
    protected EndpointFetcher.Natives mEndpointFetcherJniMock;

    @Mock
    protected OptimizationGuideBridge.Natives mOptimizationGuideBridgeJniMock;

    @Mock
    protected Profile mProfileMock;

    @Mock
    protected NavigationHandle mNavigationHandle;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(EndpointFetcherJni.TEST_HOOKS, mEndpointFetcherJniMock);
        mMocker.mock(OptimizationGuideBridgeJni.TEST_HOOKS, mOptimizationGuideBridgeJniMock);
        // Ensure native pointer is initialized
        doReturn(1L).when(mOptimizationGuideBridgeJniMock).init();
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(),
                OptimizationGuideDecision.TRUE, null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.onDeferredStartup();
            PersistedTabDataConfiguration.setUseTestConfig(true);
        });
        Profile.setLastUsedProfileForTesting(mProfileMock);
        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);
        doReturn(true).when(mNavigationHandle).isInPrimaryMainFrame();
    }

    @UiThreadTest
    @SmallTest
    @Test
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:check_if_price_drop_is_seen/true"})
    public void testShoppingProto() {
        Tab tab = new MockTab(ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
        supplier.set(true);
        shoppingPersistedTabData.registerIsTabSaveEnabledSupplier(supplier);
        shoppingPersistedTabData.setPriceMicros(
                ShoppingPersistedTabDataTestUtils.PRICE_MICROS, null);
        shoppingPersistedTabData.setCurrencyCode(
                ShoppingPersistedTabDataTestUtils.GREAT_BRITAIN_CURRENCY_CODE);
        shoppingPersistedTabData.setPriceDropGurl(new GURL("https://www.google.com"));
        shoppingPersistedTabData.setIsCurrentPriceDropSeen(true);
        ByteBuffer serialized = shoppingPersistedTabData.getSerializer().get();
        ShoppingPersistedTabData deserialized = new ShoppingPersistedTabData(tab);
        deserialized.deserialize(serialized);
        Assert.assertEquals(
                ShoppingPersistedTabDataTestUtils.PRICE_MICROS, deserialized.getPriceMicros());
        Assert.assertEquals(
                ShoppingPersistedTabData.NO_PRICE_KNOWN, deserialized.getPreviousPriceMicros());
        Assert.assertEquals(ShoppingPersistedTabDataTestUtils.GREAT_BRITAIN_CURRENCY_CODE,
                deserialized.getCurrencyCode());
        Assert.assertTrue(deserialized.getIsCurrentPriceDropSeen());
        Assert.assertEquals(
                new GURL("https://www.google.com"), deserialized.getPriceDropDataForTesting().gurl);
        MetricsResult metricsResult =
                deserialized.getPriceDropMetricsLoggerForTesting().getMetricsResultForTesting();
        Assert.assertFalse(metricsResult.isProductDetailPage);
        Assert.assertTrue(metricsResult.containsPrice);
        Assert.assertFalse(metricsResult.containsPriceDrop);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testMetricDerivations() {
        Tab tab = new MockTab(ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
        supplier.set(true);
        shoppingPersistedTabData.registerIsTabSaveEnabledSupplier(supplier);
        for (boolean isProductDetailPage : new boolean[] {false, true}) {
            for (boolean containsPrice : new boolean[] {false, true}) {
                for (boolean containsPriceDrop : new boolean[] {false, true}) {
                    shoppingPersistedTabData.setMainOfferId(
                            isProductDetailPage ? "non-empty-offer-id" : null);
                    shoppingPersistedTabData.setPriceMicros(containsPrice || containsPriceDrop
                                    ? 42_000_000L
                                    : ShoppingPersistedTabData.NO_PRICE_KNOWN);
                    shoppingPersistedTabData.setPreviousPriceMicros(containsPriceDrop
                                    ? 30_000_000L
                                    : ShoppingPersistedTabData.NO_PRICE_KNOWN);
                    ByteBuffer serialized = shoppingPersistedTabData.getSerializer().get();
                    ShoppingPersistedTabData deserialized = new ShoppingPersistedTabData(tab);
                    deserialized.deserialize(serialized);
                    MetricsResult metricsResult = deserialized.getPriceDropMetricsLoggerForTesting()
                                                          .getMetricsResultForTesting();
                    Assert.assertEquals(isProductDetailPage, metricsResult.isProductDetailPage);
                    Assert.assertEquals(
                            containsPrice || containsPriceDrop, metricsResult.containsPrice);
                    Assert.assertEquals(containsPriceDrop, metricsResult.containsPriceDrop);
                }
            }
        }
    }

    @SmallTest
    @Test
    public void testShoppingBloomFilterNotShoppingWebsite() {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_INITIAL);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(),
                OptimizationGuideDecision.FALSE, null);
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(
                    tab, (shoppingPersistedTabData) -> { semaphore.release(); });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
        ShoppingPersistedTabDataTestUtils.verifyPriceTrackingOptimizationTypeCalled(
                mOptimizationGuideBridgeJniMock, 0);
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testShoppingBloomFilterShoppingWebsite() {
        for (@OptimizationGuideDecision int decision :
                new int[] {OptimizationGuideDecision.TRUE, OptimizationGuideDecision.UNKNOWN}) {
            ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                    mOptimizationGuideBridgeJniMock,
                    HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                    ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                            .BUYABLE_PRODUCT_INITIAL);
            ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                    mOptimizationGuideBridgeJniMock,
                    HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(), decision,
                    null);
            Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                    ShoppingPersistedTabDataTestUtils.TAB_ID,
                    ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
            Semaphore semaphore = new Semaphore(0);
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                ShoppingPersistedTabData.from(
                        tab, (shoppingPersistedTabData) -> { semaphore.release(); });
            });
            ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
            ShoppingPersistedTabDataTestUtils.verifyPriceTrackingOptimizationTypeCalled(
                    mOptimizationGuideBridgeJniMock, 1);
        }
    }

    @SmallTest
    @Test
    public void testStaleTab() {
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CriticalPersistedTabData.from(tab).setTimestampMillis(
                    System.currentTimeMillis() - TimeUnit.DAYS.toMillis(100));
            ShoppingPersistedTabData.from(
                    tab, (shoppingPersistedTabData) -> { semaphore.release(); });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
        ShoppingPersistedTabDataTestUtils.verifyPriceTrackingOptimizationTypeCalled(
                mOptimizationGuideBridgeJniMock, 0);
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_stale_tab_threshold_seconds/86400/"
            + "price_tracking_with_optimization_guide/true"})
    public void
    test2DayTabWithStaleOverride1day() {
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CriticalPersistedTabData.from(tab).setTimestampMillis(
                    System.currentTimeMillis() - TimeUnit.DAYS.toMillis(2));
            ShoppingPersistedTabData.from(
                    tab, (shoppingPersistedTabData) -> { semaphore.release(); });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
        ShoppingPersistedTabDataTestUtils.verifyPriceTrackingOptimizationTypeCalled(
                mOptimizationGuideBridgeJniMock, 0);
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_stale_tab_threshold_seconds/86400/"
            + "price_tracking_with_optimization_guide/true"})
    public void
    testHalfDayTabWithStaleOverride1day() {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_INITIAL);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(),
                OptimizationGuideDecision.TRUE, null);
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CriticalPersistedTabData.from(tab).setTimestampMillis(
                    System.currentTimeMillis() - TimeUnit.HOURS.toMillis(12));
            ShoppingPersistedTabData.from(
                    tab, (shoppingPersistedTabData) -> { semaphore.release(); });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
        ShoppingPersistedTabDataTestUtils.verifyPriceTrackingOptimizationTypeCalled(
                mOptimizationGuideBridgeJniMock, 1);
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testNoRefetch() {
        final Semaphore initialSemaphore = new Semaphore(0);
        final Semaphore updateSemaphore = new Semaphore(0);
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_INITIAL);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertEquals(ShoppingPersistedTabDataTestUtils.PRICE_MICROS,
                        shoppingPersistedTabData.getPriceMicros());
                Assert.assertEquals(ShoppingPersistedTabData.NO_PRICE_KNOWN,
                        shoppingPersistedTabData.getPreviousPriceMicros());
                Assert.assertEquals(ShoppingPersistedTabDataTestUtils.UNITED_STATES_CURRENCY_CODE,
                        shoppingPersistedTabData.getCurrencyCode());
                // By setting time to live to be a negative number, an update
                // will be forced in the subsequent call
                initialSemaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(initialSemaphore);
        ShoppingPersistedTabDataTestUtils.verifyPriceTrackingOptimizationTypeCalled(
                mOptimizationGuideBridgeJniMock, 1);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_PRICE_UPDATED);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertEquals(ShoppingPersistedTabDataTestUtils.PRICE_MICROS,
                        shoppingPersistedTabData.getPriceMicros());
                Assert.assertEquals(ShoppingPersistedTabData.NO_PRICE_KNOWN,
                        shoppingPersistedTabData.getPreviousPriceMicros());

                // By setting time to live to be a negative number, an update
                // will be forced in the subsequent call
                updateSemaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(updateSemaphore);
        // EndpointFetcher should not have been called a second time - because we haven't passed the
        // time to live
        ShoppingPersistedTabDataTestUtils.verifyPriceTrackingOptimizationTypeCalled(
                mOptimizationGuideBridgeJniMock, 1);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDrop() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();
        // Prices unknown is not a price drop
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());

        // Same price is not a price drop
        shoppingPersistedTabData.setPriceMicros(
                ShoppingPersistedTabDataTestUtils.HIGH_PRICE_MICROS, null);
        shoppingPersistedTabData.setPreviousPriceMicros(
                ShoppingPersistedTabDataTestUtils.HIGH_PRICE_MICROS);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());

        // Lower -> Higher price is not a price drop
        shoppingPersistedTabData.setPriceMicros(
                ShoppingPersistedTabDataTestUtils.HIGH_PRICE_MICROS, null);
        shoppingPersistedTabData.setPreviousPriceMicros(
                ShoppingPersistedTabDataTestUtils.LOW_PRICE_MICROS);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());

        // Actual price drop (Higher -> Lower)
        shoppingPersistedTabData.setPriceMicros(
                ShoppingPersistedTabDataTestUtils.LOW_PRICE_MICROS, null);
        shoppingPersistedTabData.setPreviousPriceMicros(
                ShoppingPersistedTabDataTestUtils.HIGH_PRICE_MICROS);
        ShoppingPersistedTabData.PriceDrop priceDrop = shoppingPersistedTabData.getPriceDrop();
        Assert.assertEquals(ShoppingPersistedTabDataTestUtils.LOW_PRICE_FORMATTED, priceDrop.price);
        Assert.assertEquals(
                ShoppingPersistedTabDataTestUtils.HIGH_PRICE_FORMATTED, priceDrop.previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterSamePrice() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();

        // $10 -> $10 is not a price drop (same price)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(10_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(10_000_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterNoFormattedPriceDifference() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();

        // $10.40 -> $10 (which would be displayed $10 -> $10 is not a price drop)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(10_400_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(10_000_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterPriceIncrease() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithDefaults();

        // $9.33 -> $9.66 price increase is not a price drop
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(9_330_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(9_660_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropGBP() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithCurrencyCode(
                        ShoppingPersistedTabDataTestUtils.TAB_ID,
                        ShoppingPersistedTabDataTestUtils.IS_INCOGNITO,
                        ShoppingPersistedTabDataTestUtils.GREAT_BRITAIN_CURRENCY_CODE);
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(15_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(9_560_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("£15", shoppingPersistedTabData.getPriceDrop().previousPrice);
        Assert.assertEquals("£9.56", shoppingPersistedTabData.getPriceDrop().price);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropJPY() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithCurrencyCode(
                        ShoppingPersistedTabDataTestUtils.TAB_ID,
                        ShoppingPersistedTabDataTestUtils.IS_INCOGNITO,
                        ShoppingPersistedTabDataTestUtils.JAPAN_CURRENCY_CODE);
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(3_140_000_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(287_000_000_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("¥3,140,000", shoppingPersistedTabData.getPriceDrop().previousPrice);
        Assert.assertEquals("¥287,000", shoppingPersistedTabData.getPriceDrop().price);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testStalePriceDropUSD() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils.createShoppingPersistedTabDataWithCurrencyCode(
                        ShoppingPersistedTabDataTestUtils.TAB_ID,
                        ShoppingPersistedTabDataTestUtils.IS_INCOGNITO,
                        ShoppingPersistedTabDataTestUtils.UNITED_STATES_CURRENCY_CODE);
        // $10 -> $5 (50% and $5 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(10000000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(5000000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$5.00", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$10", shoppingPersistedTabData.getPriceDrop().previousPrice);

        // Price drops greater than a week old are removed
        shoppingPersistedTabData.setLastPriceChangeTimeMsForTesting(
                System.currentTimeMillis() - TimeUnit.DAYS.toMillis(8));
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    @UiThreadTest
    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testNewUrlResetSPTD() {
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse.NONE);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(),
                OptimizationGuideDecision.TRUE, null);
        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        for (boolean isSameDocument : new boolean[] {false, true}) {
            ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
            shoppingPersistedTabData.setPriceMicros(42_000_000L);
            shoppingPersistedTabData.setPreviousPriceMicros(60_000_000L);
            shoppingPersistedTabData.setCurrencyCode("USD");
            shoppingPersistedTabData.setPriceDropGurl(
                    ShoppingPersistedTabDataTestUtils.DEFAULT_GURL);
            Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
            doReturn(isSameDocument).when(navigationHandle).isSameDocument();
            shoppingPersistedTabData.getUrlUpdatedObserverForTesting()
                    .onDidStartNavigationInPrimaryMainFrame(tab, navigationHandle);
            if (!isSameDocument) {
                Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
            } else {
                Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
            }
        }
    }

    @UiThreadTest
    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testDontResetSPTDOnRefresh() {
        MockTab tab = (MockTab) ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse.NONE);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(),
                OptimizationGuideDecision.TRUE, null);
        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        doReturn(true).when(navigationHandle).isInPrimaryMainFrame();
        doReturn(false).when(navigationHandle).isSameDocument();
        GURL gurl1 = new GURL("https://foo.com");
        GURL gurl2 = new GURL("https://bar.com");
        tab.setGurlOverrideForTesting(gurl1);
        doReturn(gurl1).when(navigationHandle).getUrl();
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        shoppingPersistedTabData.setPriceMicros(42_000_000L);
        shoppingPersistedTabData.setPreviousPriceMicros(60_000_000L);
        shoppingPersistedTabData.setCurrencyCode("USD");
        shoppingPersistedTabData.setPriceDropGurl(gurl1);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        shoppingPersistedTabData.getUrlUpdatedObserverForTesting()
                .onDidStartNavigationInPrimaryMainFrame(tab, navigationHandle);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        doReturn(gurl2).when(navigationHandle).getUrl();
        shoppingPersistedTabData.getUrlUpdatedObserverForTesting()
                .onDidStartNavigationInPrimaryMainFrame(tab, navigationHandle);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    @UiThreadTest
    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testOmniBoxSearchResetSPTD() {
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse.NONE);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(),
                OptimizationGuideDecision.TRUE, null);
        NavigationHandle navigationHandle = mock(NavigationHandle.class);
        doReturn(false).when(navigationHandle).isSameDocument();
        doReturn(false).when(navigationHandle).isValidSearchFormUrl();
        doReturn(true).when(navigationHandle).hasCommitted();
        int reloadFromAddressBar = PageTransition.FROM_ADDRESS_BAR | PageTransition.RELOAD;
        doReturn(reloadFromAddressBar).when(navigationHandle).pageTransition();
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        shoppingPersistedTabData.setPriceMicros(42_000_000L);
        shoppingPersistedTabData.setPreviousPriceMicros(60_000_000L);
        shoppingPersistedTabData.setCurrencyCode("USD");
        shoppingPersistedTabData.setPriceDropGurl(ShoppingPersistedTabDataTestUtils.DEFAULT_GURL);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        shoppingPersistedTabData.getUrlUpdatedObserverForTesting()
                .onDidFinishNavigationInPrimaryMainFrame(tab, navigationHandle);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testSPTDSavingEnabledUponSuccessfulResponse() {
        final Semaphore semaphore = new Semaphore(0);
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_INITIAL);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertTrue(shoppingPersistedTabData.mIsTabSaveEnabledSupplier.get());
                semaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testSPTDSavingEnabledUponSuccessfulProductUpdateResponse() {
        final Semaphore semaphore = new Semaphore(0);
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_AND_PRODUCT_UPDATE);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertTrue(shoppingPersistedTabData.mIsTabSaveEnabledSupplier.get());
                semaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testSPTDNullUponUnsuccessfulResponse() {
        final Semaphore semaphore = new Semaphore(0);
        MockTab tab = (MockTab) ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse.BUYABLE_PRODUCT_EMPTY);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
                semaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }

    @SmallTest
    @Test
    public void testSPTDNullOptimizationGuideFalse() {
        final Semaphore semaphore = new Semaphore(0);
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse.NONE);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
                semaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testSerializationBug() {
        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        shoppingPersistedTabData.setPriceMicros(42_000_000L, null);
        ByteBuffer serialized = shoppingPersistedTabData.getSerializer().get();
        PersistedTabDataConfiguration config = PersistedTabDataConfiguration.get(
                ShoppingPersistedTabData.class, tab.isIncognito());
        ShoppingPersistedTabData deserialized =
                new ShoppingPersistedTabData(tab, config.getStorage(), config.getId());
        deserialized.deserializeAndLog(serialized);
        Assert.assertEquals(42_000_000L, deserialized.getPriceMicros());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testSerializeWithOfferId() {
        Tab tab = new MockTab(ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
        supplier.set(true);
        shoppingPersistedTabData.registerIsTabSaveEnabledSupplier(supplier);
        shoppingPersistedTabData.setMainOfferId(ShoppingPersistedTabDataTestUtils.FAKE_OFFER_ID);
        ByteBuffer serialized = shoppingPersistedTabData.getSerializer().get();
        ShoppingPersistedTabData deserialized = new ShoppingPersistedTabData(tab);
        deserialized.deserialize(serialized);
        Assert.assertEquals(
                ShoppingPersistedTabDataTestUtils.FAKE_OFFER_ID, deserialized.getMainOfferId());
    }

    @UiThreadTest
    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testOptimizationGuideNavigationIntegration() {
        Tab tab = new MockTab(ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        GURL gurl = new GURL("https://www.google.com");
        doReturn(gurl).when(mNavigationHandle).getUrl();
        doReturn(true).when(mNavigationHandle).hasCommitted();
        shoppingPersistedTabData.getUrlUpdatedObserverForTesting()
                .onDidFinishNavigationInPrimaryMainFrame(tab, mNavigationHandle);
        ShoppingPersistedTabDataTestUtils.verifyOptimizationGuideCalledWithNavigationHandle(
                mOptimizationGuideBridgeJniMock, gurl);
    }

    @UiThreadTest
    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testOptGuidePrefetching() {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponseAsync(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_AND_PRODUCT_UPDATE);
        MockTab tab = (MockTab) ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        tab.setIsInitialized(true);
        GURL gurl = new GURL("https://www.google.com");
        tab.setGurlOverrideForTesting(gurl);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        doReturn(gurl).when(mNavigationHandle).getUrl();
        Semaphore semaphore = new Semaphore(0);
        shoppingPersistedTabData.prefetchOnNewNavigation(
                tab, mNavigationHandle, semaphore::release);
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
        Assert.assertEquals(287_000_000L, shoppingPersistedTabData.getPriceMicros());
        Assert.assertEquals(
                123_456_789_012_345L, shoppingPersistedTabData.getPreviousPriceMicros());
    }

    @UiThreadTest
    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testOptGuidePrefetchingNoResponse() {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponseAsync(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse.NONE);
        MockTab tab = (MockTab) ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        tab.setIsInitialized(true);
        GURL gurl = new GURL("https://www.google.com");
        tab.setGurlOverrideForTesting(gurl);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        doReturn(gurl).when(mNavigationHandle).getUrl();
        Semaphore semaphore = new Semaphore(0);
        shoppingPersistedTabData.prefetchOnNewNavigation(
                tab, mNavigationHandle, semaphore::release);
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
        Assert.assertEquals(
                ShoppingPersistedTabData.NO_PRICE_KNOWN, shoppingPersistedTabData.getPriceMicros());
        Assert.assertEquals(ShoppingPersistedTabData.NO_PRICE_KNOWN,
                shoppingPersistedTabData.getPreviousPriceMicros());
    }

    @UiThreadTest
    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testOptGuidePrefetchingUnparseable() {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponseAsync(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse.UNPARSEABLE);
        MockTab tab = (MockTab) ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        tab.setIsInitialized(true);
        GURL gurl = new GURL("https://www.google.com");
        tab.setGurlOverrideForTesting(gurl);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        doReturn(gurl).when(mNavigationHandle).getUrl();
        Semaphore semaphore = new Semaphore(0);
        shoppingPersistedTabData.prefetchOnNewNavigation(
                tab, mNavigationHandle, semaphore::release);
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
        Assert.assertEquals(
                ShoppingPersistedTabData.NO_PRICE_KNOWN, shoppingPersistedTabData.getPriceMicros());
        Assert.assertEquals(ShoppingPersistedTabData.NO_PRICE_KNOWN,
                shoppingPersistedTabData.getPreviousPriceMicros());
    }

    @UiThreadTest
    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testPriceDropURLTabURLMisMatch() {
        MockTab tab = (MockTab) ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        tab.setIsInitialized(true);
        tab.setGurlOverrideForTesting(new GURL("https://www.google.com"));
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        shoppingPersistedTabData.setPriceMicros(42_000_000L);
        shoppingPersistedTabData.setPreviousPriceMicros(60_000_000L);
        shoppingPersistedTabData.setPriceDropGurl(new GURL("https://www.google.com"));
        shoppingPersistedTabData.setLastPriceChangeTimeMsForTesting(System.currentTimeMillis());
        shoppingPersistedTabData.setCurrencyCode("USD");
        shoppingPersistedTabData.setLastUpdatedMs(System.currentTimeMillis());
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertFalse(shoppingPersistedTabData.needsUpdate());
        shoppingPersistedTabData.setPriceDropGurl(new GURL("https://www.yahoo.com"));
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertTrue(shoppingPersistedTabData.needsUpdate());
    }

    @SmallTest
    @Test
    public void testIncognitoTabDisabled() throws TimeoutException {
        TabImpl tab = mock(TabImpl.class);
        doReturn(true).when(tab).isIncognito();
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (res) -> {
                Assert.assertNull(res);
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(0);
    }

    @SmallTest
    @Test
    public void testCustomTabsDisabled() throws TimeoutException {
        TabImpl tab = mock(TabImpl.class);
        doReturn(true).when(tab).isCustomTab();
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (res) -> {
                Assert.assertNull(res);
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(0);
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testTabDestroyedSupplier() {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_INITIAL);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(),
                OptimizationGuideDecision.TRUE, null);
        TabImpl tab = mock(TabImpl.class);
        doReturn(ShoppingPersistedTabDataTestUtils.TAB_ID).when(tab).getId();
        doReturn(ShoppingPersistedTabDataTestUtils.IS_INCOGNITO).when(tab).isIncognito();
        CriticalPersistedTabData criticalPersistedTabData = new CriticalPersistedTabData(tab);
        criticalPersistedTabData.setTimestampMillis(
                System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1));
        for (boolean isInitialized : new boolean[] {false, true}) {
            doReturn(isInitialized).when(tab).isInitialized();
            Semaphore semaphore = new Semaphore(0);
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                UserDataHost userDataHost = new UserDataHost();
                userDataHost.setUserData(CriticalPersistedTabData.class, criticalPersistedTabData);
                doReturn(userDataHost).when(tab).getUserDataHost();
                ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                    if (isInitialized) {
                        Assert.assertNotNull(shoppingPersistedTabData);
                    } else {
                        Assert.assertNull(shoppingPersistedTabData);
                    }
                    semaphore.release();
                });
            });
        }
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testTabDestroyed1() {
        final Semaphore semaphore = new Semaphore(0);
        MockTab tab = getDefaultTab();
        mockOptimizationGuideDefaults();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // There is ShoppingPersistedTabData associated with the Tab, however, it is 1 day old
            // (the threshold for a refetch is 1 hour) so a refetch will be forced.
            ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
            shoppingPersistedTabData.setLastUpdatedMs(
                    System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1));
            tab.getUserDataHost().setUserData(
                    ShoppingPersistedTabData.class, shoppingPersistedTabData);
            // Tab being destroyed should result in the public API from returning null
            tab.destroy();
            ShoppingPersistedTabData.from(tab, (sptdRes) -> {
                Assert.assertNull(sptdRes);
                semaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testTabDestroyed2() {
        final Semaphore semaphore = new Semaphore(0);
        MockTab tab = getDefaultTab();
        mockOptimizationGuideDefaults();
        // There is no ShoppingPersistedTabData associated with the Tab, so it will be
        // acquired from OptimizationGuide.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Tab being destroyed should result in the public API from returning null
            tab.destroy();
            ShoppingPersistedTabData.from(tab, (sptdRes) -> {
                Assert.assertNull(sptdRes);
                semaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testTabDestroyed3() {
        final Semaphore semaphore0 = new Semaphore(0);
        MockTab tab = getDefaultTab();
        mockOptimizationGuideDefaults();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
            // ShoppingPersistedTabData is 1 day old which will trigger a refetch, however, this
            // time it will be acquired from storage, then refetched.
            shoppingPersistedTabData.setLastUpdatedMs(
                    System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1));
            save(shoppingPersistedTabData);
            // Verify ShoppingPersistedTabData is acquired from storage as expected.
            ShoppingPersistedTabData.from(tab, (sptdRes) -> {
                Assert.assertNotNull(sptdRes);
                semaphore0.release();
            });
        });
        final Semaphore semaphore1 = new Semaphore(0);
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Remove UserData to force acquisition from storage.
            tab.getUserDataHost().removeUserData(ShoppingPersistedTabData.class);
            // Tab being destroyed should result in the public API from returning null
            tab.destroy();
            ShoppingPersistedTabData.from(tab, (sptdRes) -> {
                Assert.assertNull(sptdRes);
                semaphore1.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore1);
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testTabDestroyed4() {
        final Semaphore semaphore0 = new Semaphore(0);
        MockTab tab = getDefaultTab();
        tab.setGurlOverrideForTesting(ShoppingPersistedTabDataTestUtils.DEFAULT_GURL);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
            // ShoppingPersistedTabData is 0 seconds old so it can be acquired from storage
            // and returned direcetly, without a refresh.
            shoppingPersistedTabData.setLastUpdatedMs(System.currentTimeMillis());
            shoppingPersistedTabData.setPriceDropGurl(
                    ShoppingPersistedTabDataTestUtils.DEFAULT_GURL);
            save(shoppingPersistedTabData);
            // Verify ShoppingPersistedTabData is acquired from storage as expected.
            ShoppingPersistedTabData.from(tab, (sptdRes) -> {
                Assert.assertNotNull(sptdRes);
                semaphore0.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore0);
        final Semaphore semaphore1 = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Remove UserData to force acquisition from storage.
            tab.getUserDataHost().removeUserData(ShoppingPersistedTabData.class);
            // Tab being destroyed should result in the public API from returning null
            tab.destroy();
            ShoppingPersistedTabData.from(tab, (sptdRes) -> {
                Assert.assertNull(sptdRes);
                semaphore1.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore1);
    }

    @SmallTest
    @Test
    @CommandLineFlags.
    Add({"force-fieldtrial-params=Study.Group:price_tracking_with_optimization_guide/true"})
    public void testCheckPriceDropUrlForUpdateWhenItExists() {
        final Semaphore semaphore = new Semaphore(0);
        MockTab tab = getDefaultTab();
        mockOptimizationGuideEmptyResponse();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (sptdRes) -> {
                Assert.assertNull(sptdRes.getPriceDropDataForTesting().gurl);
                Assert.assertFalse(sptdRes.needsUpdate());
                semaphore.release();
            });
        });
        ShoppingPersistedTabDataTestUtils.acquireSemaphore(semaphore);
    }

    private static MockTab getDefaultTab() {
        return (MockTab) ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
    }

    private void mockOptimizationGuideDefaults() {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_AND_PRODUCT_UPDATE);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(),
                OptimizationGuideDecision.TRUE, null);
    }

    private void mockOptimizationGuideEmptyResponse() {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse.BUYABLE_PRODUCT_EMPTY);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(),
                OptimizationGuideDecision.TRUE, null);
    }

    private static void save(ShoppingPersistedTabData shoppingPersistedTabData) {
        ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
        supplier.set(true);
        shoppingPersistedTabData.registerIsTabSaveEnabledSupplier(supplier);
        shoppingPersistedTabData.enableSaving();
        shoppingPersistedTabData.save();
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testShoppingPersistedTabDataSupportedForMaintenance() {
        TabImpl tab = mock(TabImpl.class);
        doReturn(ShoppingPersistedTabDataTestUtils.TAB_ID).when(tab).getId();
        doReturn(ShoppingPersistedTabDataTestUtils.IS_INCOGNITO).when(tab).isIncognito();
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        Assert.assertTrue(PersistedTabData.getSupportedMaintenanceClassesForTesting().contains(
                ShoppingPersistedTabData.class));
    }

    @SmallTest
    @Test
    public void testVerifyDeserializationBackgroundThread() throws TimeoutException {
        ThreadUtils.setThreadAssertsDisabledForTesting(false);
        CallbackHelper helper = new CallbackHelper();
        int count = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = MockTab.createAndInitialize(1, false);
            tab.setIsTabSaveEnabled(true);
            DeserializeAndLogCheckerShoppingPersistedTabData deserializeChecker =
                    new DeserializeAndLogCheckerShoppingPersistedTabData(tab);
            registerObserverSupplier(deserializeChecker);
            deserializeChecker.save();
            PersistedTabData.from(tab,
                    (data, storage, id, factoryCallback)
                            -> {
                        factoryCallback.onResult(
                                new DeserializeAndLogCheckerShoppingPersistedTabData(
                                        tab, storage, id));
                    },
                    null, ShoppingPersistedTabData.class, (res) -> { helper.notifyCalled(); });
        });
        helper.waitForCallback(count);
    }

    @SmallTest
    @Test
    public void testDestroyedTab() throws TimeoutException {
        TabImpl tab = mock(TabImpl.class);
        doReturn(true).when(tab).isDestroyed();
        CallbackHelper helper = new CallbackHelper();
        int count = helper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (res) -> {
                Assert.assertNull(res);
                helper.notifyCalled();
            });
        });
        helper.waitForCallback(count);
    }

    @SmallTest
    @Test
    public void testNullTab() throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        int count = helper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(null, (res) -> {
                Assert.assertNull(res);
                helper.notifyCalled();
            });
        });
        helper.waitForCallback(count);
    }

    static class DeserializeAndLogCheckerShoppingPersistedTabData extends ShoppingPersistedTabData {
        DeserializeAndLogCheckerShoppingPersistedTabData(Tab tab) {
            super(tab);
        }

        DeserializeAndLogCheckerShoppingPersistedTabData(Tab tab,
                PersistedTabDataStorage persistedTabDataStorage, String persistedTabDataId) {
            super(tab, persistedTabDataStorage, persistedTabDataId);
        }

        @Override
        public void deserializeAndLog(@Nullable ByteBuffer bytes) {
            ThreadUtils.assertOnBackgroundThread();
            super.deserializeAndLog(bytes);
        }

        @Override
        protected boolean needsUpdate() {
            return false;
        }
    }

    @SmallTest
    @Test
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:check_if_price_drop_is_seen/true"
            + "/price_tracking_with_optimization_guide/true"})
    public void
    testIsCurrentPriceDropSeen_PriceChange() throws TimeoutException {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_AND_PRODUCT_UPDATE);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(),
                OptimizationGuideDecision.TRUE, null);

        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils
                        .createShoppingPersistedTabDataWithPriceDropOnUiThread(tab);

        CallbackHelper callbackHelper = new CallbackHelper();
        int count = callbackHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            shoppingPersistedTabData.setIsCurrentPriceDropSeen(true);
            shoppingPersistedTabData.setPriceMicros(
                    ShoppingPersistedTabDataTestUtils.LOW_PRICE_MICROS);
            tab.getUserDataHost().setUserData(
                    ShoppingPersistedTabData.class, shoppingPersistedTabData);
            ShoppingPersistedTabData.from(tab, (sptd) -> {
                Assert.assertFalse(sptd.getIsCurrentPriceDropSeen());
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(count);
    }

    @SmallTest
    @Test
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:check_if_price_drop_is_seen/true"
            + "/price_tracking_with_optimization_guide/true"})
    public void
    testIsCurrentPriceDropSeen_CurrencyChange() throws TimeoutException {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_AND_PRODUCT_UPDATE);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(),
                OptimizationGuideDecision.TRUE, null);

        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils
                        .createShoppingPersistedTabDataWithPriceDropOnUiThread(tab);

        CallbackHelper callbackHelper = new CallbackHelper();
        int count = callbackHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            shoppingPersistedTabData.setIsCurrentPriceDropSeen(true);
            shoppingPersistedTabData.setCurrencyCode(
                    ShoppingPersistedTabDataTestUtils.JAPAN_CURRENCY_CODE);
            tab.getUserDataHost().setUserData(
                    ShoppingPersistedTabData.class, shoppingPersistedTabData);
            ShoppingPersistedTabData.from(tab, (sptd) -> {
                Assert.assertFalse(sptd.getIsCurrentPriceDropSeen());
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(count);
    }

    @SmallTest
    @Test
    @CommandLineFlags.Add({"force-fieldtrial-params=Study.Group:check_if_price_drop_is_seen/true"
            + "/price_tracking_with_optimization_guide/true"})
    public void
    testIsCurrentPriceDropSeen_NoPriceChange() throws TimeoutException {
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.PRICE_TRACKING.getNumber(),
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_AND_PRODUCT_UPDATE);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeJniMock,
                HintsProto.OptimizationType.SHOPPING_PAGE_PREDICTOR.getNumber(),
                OptimizationGuideDecision.TRUE, null);

        Tab tab = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(
                ShoppingPersistedTabDataTestUtils.TAB_ID,
                ShoppingPersistedTabDataTestUtils.IS_INCOGNITO);
        ShoppingPersistedTabData shoppingPersistedTabData =
                ShoppingPersistedTabDataTestUtils
                        .createShoppingPersistedTabDataWithPriceDropOnUiThread(tab);

        CallbackHelper callbackHelper = new CallbackHelper();
        int count = callbackHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            shoppingPersistedTabData.setIsCurrentPriceDropSeen(true);
            tab.getUserDataHost().setUserData(
                    ShoppingPersistedTabData.class, shoppingPersistedTabData);
            ShoppingPersistedTabData.from(tab, (sptd) -> {
                Assert.assertTrue(sptd.getIsCurrentPriceDropSeen());
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForCallback(count);
    }

    private static void registerObserverSupplier(
            ShoppingPersistedTabData shoppingPersistedTabData) {
        ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
        supplier.set(true);
        shoppingPersistedTabData.registerIsTabSaveEnabledSupplier(supplier);
    }
}
