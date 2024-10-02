// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import com.google.protobuf.ByteString;

import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge.OptimizationGuideCallback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.commerce.PriceTracking.BuyableProduct;
import org.chromium.components.commerce.PriceTracking.PriceTrackingData;
import org.chromium.components.commerce.PriceTracking.ProductPrice;
import org.chromium.components.commerce.PriceTracking.ProductPriceUpdate;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;
import org.chromium.components.optimization_guide.proto.HintsProto;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Helper class for {@link ShoppingPersistedTabDataTest} & {@link
 * ShoppingPersistedTabDataLegacyTest}.
 */
public abstract class ShoppingPersistedTabDataTestUtils {
    @IntDef({MockPriceTrackingResponse.BUYABLE_PRODUCT_INITIAL,
            MockPriceTrackingResponse.BUYABLE_PRODUCT_PRICE_UPDATED,
            MockPriceTrackingResponse.BUYABLE_PRODUCT_AND_PRODUCT_UPDATE,
            MockPriceTrackingResponse.PRODUCT_PRICE_UPDATE,
            MockPriceTrackingResponse.BUYABLE_PRODUCT_EMPTY, MockPriceTrackingResponse.NONE})
    @Retention(RetentionPolicy.SOURCE)
    @interface MockPriceTrackingResponse {
        int BUYABLE_PRODUCT_INITIAL = 0;
        int BUYABLE_PRODUCT_PRICE_UPDATED = 1;
        int BUYABLE_PRODUCT_AND_PRODUCT_UPDATE = 2;
        int PRODUCT_PRICE_UPDATE = 3;
        int BUYABLE_PRODUCT_EMPTY = 4;
        int NONE = 5;
        int UNPARSEABLE = 6;
    }

    static final GURL DEFAULT_GURL = new GURL("https://www.google.com");
    static final long PRICE_MICROS = 123456789012345L;
    static final long UPDATED_PRICE_MICROS = 287000000L;
    static final long HIGH_PRICE_MICROS = 141000000L;
    static final long LOW_PRICE_MICROS = 100000000L;
    static final String HIGH_PRICE_FORMATTED = "$141";
    static final String LOW_PRICE_FORMATTED = "$100";
    static final String UNITED_STATES_CURRENCY_CODE = "USD";
    static final String GREAT_BRITAIN_CURRENCY_CODE = "GBP";
    static final String JAPAN_CURRENCY_CODE = "JPY";
    static final int TAB_ID = 1;
    static final boolean IS_INCOGNITO = false;
    static final String FAKE_OFFER_ID = "100";

    static final BuyableProduct BUYABLE_PRODUCT_PROTO_INITIAL =
            BuyableProduct.newBuilder()
                    .setCurrentPrice(createProductPrice(PRICE_MICROS, UNITED_STATES_CURRENCY_CODE))
                    .build();
    static final BuyableProduct BUYABLE_PRODUCT_PROTO_PRICE_UPDATED =
            BuyableProduct.newBuilder()
                    .setCurrentPrice(
                            createProductPrice(UPDATED_PRICE_MICROS, UNITED_STATES_CURRENCY_CODE))
                    .build();
    static final ProductPriceUpdate PRODUCT_UPDATE_PROTO =
            ProductPriceUpdate.newBuilder()
                    .setOldPrice(createProductPrice(PRICE_MICROS, UNITED_STATES_CURRENCY_CODE))
                    .setNewPrice(
                            createProductPrice(UPDATED_PRICE_MICROS, UNITED_STATES_CURRENCY_CODE))
                    .build();

    static final PriceTrackingData PRICE_TRACKING_BUYABLE_PRODUCT_INITIAL =
            PriceTrackingData.newBuilder().setBuyableProduct(BUYABLE_PRODUCT_PROTO_INITIAL).build();
    static final Any ANY_BUYABLE_PRODUCT_INITIAL =
            Any.newBuilder()
                    .setValue(ByteString.copyFrom(
                            PRICE_TRACKING_BUYABLE_PRODUCT_INITIAL.toByteArray()))
                    .build();

    static final PriceTrackingData PRICE_TRACKING_BUYABLE_PRODUCT_UPDATE =
            PriceTrackingData.newBuilder()
                    .setBuyableProduct(BUYABLE_PRODUCT_PROTO_PRICE_UPDATED)
                    .build();
    static final Any ANY_BUYABLE_PRODUCT_UPDATE =
            Any.newBuilder()
                    .setValue(ByteString.copyFrom(
                            PRICE_TRACKING_BUYABLE_PRODUCT_UPDATE.toByteArray()))
                    .build();

    static final PriceTrackingData PRICE_TRACKING_BUYABLE_PRODUCT_AND_PRODUCT_UPDATE =
            PriceTrackingData.newBuilder()
                    .setBuyableProduct(BUYABLE_PRODUCT_PROTO_INITIAL)
                    .setProductUpdate(PRODUCT_UPDATE_PROTO)
                    .build();
    static final Any ANY_PRICE_TRACKING_BUYABLE_PRODUCT_AND_PRODUCT_UPDATE =
            Any.newBuilder()
                    .setValue(ByteString.copyFrom(
                            PRICE_TRACKING_BUYABLE_PRODUCT_AND_PRODUCT_UPDATE.toByteArray()))
                    .build();

    static final PriceTrackingData PRICE_TRACKING_PRODUCT_UPDATE =
            PriceTrackingData.newBuilder().setProductUpdate(PRODUCT_UPDATE_PROTO).build();
    static final Any ANY_PRICE_TRACKING_PRODUCT_UPDATE =
            Any.newBuilder()
                    .setValue(ByteString.copyFrom(PRICE_TRACKING_PRODUCT_UPDATE.toByteArray()))
                    .build();

    static final PriceTrackingData PRICE_TRACKING_EMPTY = PriceTrackingData.newBuilder().build();
    static final Any ANY_PRICE_TRACKING_EMPTY =
            Any.newBuilder()
                    .setValue(ByteString.copyFrom(PRICE_TRACKING_EMPTY.toByteArray()))
                    .build();
    static final Any ANY_EMPTY = Any.newBuilder().build();
    static final Any ANY_UNPARSEABLE =
            Any.newBuilder()
                    .setValue(ByteString.copyFrom(new byte[] {(byte) 0xfa, (byte) 0x4a}))
                    .build();

    static ProductPrice createProductPrice(long amountMicros, String currencyCode) {
        return ProductPrice.newBuilder()
                .setCurrencyCode(currencyCode)
                .setAmountMicros(amountMicros)
                .build();
    }

    static ShoppingPersistedTabData createShoppingPersistedTabDataWithDefaults() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(TAB_ID, IS_INCOGNITO));
        shoppingPersistedTabData.setCurrencyCode(UNITED_STATES_CURRENCY_CODE);
        shoppingPersistedTabData.setPriceDropGurl(DEFAULT_GURL);
        return shoppingPersistedTabData;
    }

    static ShoppingPersistedTabData createSavedShoppingPersistedTabDataOnUiThread(Tab tab) {
        AtomicReference<ShoppingPersistedTabData> res = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
            ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
            supplier.set(true);
            shoppingPersistedTabData.registerIsTabSaveEnabledSupplier(supplier);
            shoppingPersistedTabData.enableSaving();
            shoppingPersistedTabData.setPriceMicros(PRICE_MICROS);
            shoppingPersistedTabData.setPreviousPriceMicros(UPDATED_PRICE_MICROS);
            shoppingPersistedTabData.setLastUpdatedMs(System.currentTimeMillis());
            shoppingPersistedTabData.setPriceDropGurl(DEFAULT_GURL);
            shoppingPersistedTabData.save();
            res.set(shoppingPersistedTabData);
        });
        return res.get();
    }

    static ShoppingPersistedTabData createShoppingPersistedTabDataWithPriceDropOnUiThread(Tab tab) {
        AtomicReference<ShoppingPersistedTabData> res = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
            ObservableSupplierImpl<Boolean> supplier = new ObservableSupplierImpl<>();
            supplier.set(true);
            shoppingPersistedTabData.registerIsTabSaveEnabledSupplier(supplier);
            shoppingPersistedTabData.setPriceMicros(UPDATED_PRICE_MICROS);
            shoppingPersistedTabData.setPreviousPriceMicros(PRICE_MICROS);
            shoppingPersistedTabData.setLastUpdatedMs(
                    System.currentTimeMillis() - TimeUnit.DAYS.toMillis(2));
            shoppingPersistedTabData.setPriceDropGurl(DEFAULT_GURL);
            shoppingPersistedTabData.setCurrencyCode(UNITED_STATES_CURRENCY_CODE);
            res.set(shoppingPersistedTabData);
        });
        return res.get();
    }

    static ShoppingPersistedTabData createShoppingPersistedTabDataWithCurrencyCode(
            int tabId, boolean isIncognito, String currencyCode) {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(tabId, isIncognito));
        shoppingPersistedTabData.setCurrencyCode(currencyCode);
        shoppingPersistedTabData.setPriceDropGurl(DEFAULT_GURL);
        return shoppingPersistedTabData;
    }

    static Tab createTabOnUiThread(int tabId, boolean isIncognito) {
        AtomicReference<Tab> res = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            MockTab tab = (MockTab) MockTab.createAndInitialize(tabId, isIncognito);
            tab.setIsInitialized(true);
            tab.setGurlOverrideForTesting(DEFAULT_GURL);
            CriticalPersistedTabData.from(tab).setTimestampMillis(System.currentTimeMillis());
            res.set(tab);
        });
        return res.get();
    }

    static long getTimeLastUpdatedOnUiThread(Tab tab) {
        AtomicReference<Long> res = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            res.set(PersistedTabData.from(tab, ShoppingPersistedTabData.class)
                            .getLastPriceChangeTimeMs());
        });
        return res.get();
    }

    static void acquireSemaphore(Semaphore semaphore) {
        try {
            semaphore.acquire();
        } catch (InterruptedException e) {
            // Throw Runtime exception to make catching InterruptedException unnecessary
            throw new RuntimeException(e);
        }
    }

    static void mockOptimizationGuideResponse(OptimizationGuideBridge.Natives optimizationGuideJni,
            int optimizationType, @OptimizationGuideDecision int decision, @Nullable Any metadata) {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                OptimizationGuideCallback callback =
                        (OptimizationGuideCallback) invocation.getArguments()[3];
                callback.onOptimizationGuideDecision(decision, metadata);
                return null;
            }
        })
                .when(optimizationGuideJni)
                .canApplyOptimization(anyLong(), any(GURL.class), eq(optimizationType),
                        any(OptimizationGuideCallback.class));
    }

    static void mockOptimizationGuideResponse(OptimizationGuideBridge.Natives optimizationGuideJni,
            int optimizationType, @MockPriceTrackingResponse int expectedResponse) {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                OptimizationGuideCallback callback =
                        (OptimizationGuideCallback) invocation.getArguments()[3];
                switch (expectedResponse) {
                    case MockPriceTrackingResponse.BUYABLE_PRODUCT_INITIAL:
                        callback.onOptimizationGuideDecision(
                                OptimizationGuideDecision.TRUE, ANY_BUYABLE_PRODUCT_INITIAL);
                        break;
                    case MockPriceTrackingResponse.BUYABLE_PRODUCT_PRICE_UPDATED:
                        callback.onOptimizationGuideDecision(
                                OptimizationGuideDecision.TRUE, ANY_BUYABLE_PRODUCT_UPDATE);
                        break;
                    case MockPriceTrackingResponse.BUYABLE_PRODUCT_AND_PRODUCT_UPDATE:
                        callback.onOptimizationGuideDecision(OptimizationGuideDecision.TRUE,
                                ANY_PRICE_TRACKING_BUYABLE_PRODUCT_AND_PRODUCT_UPDATE);
                        break;
                    case MockPriceTrackingResponse.PRODUCT_PRICE_UPDATE:
                        callback.onOptimizationGuideDecision(
                                OptimizationGuideDecision.TRUE, ANY_PRICE_TRACKING_PRODUCT_UPDATE);
                        break;
                    case MockPriceTrackingResponse.BUYABLE_PRODUCT_EMPTY:
                        callback.onOptimizationGuideDecision(
                                OptimizationGuideDecision.TRUE, ANY_PRICE_TRACKING_EMPTY);
                        break;
                    case MockPriceTrackingResponse.NONE:
                        callback.onOptimizationGuideDecision(
                                OptimizationGuideDecision.FALSE, ANY_EMPTY);
                        break;
                    default:
                        break;
                }
                return null;
            }
        })
                .when(optimizationGuideJni)
                .canApplyOptimization(
                        anyLong(), any(GURL.class), anyInt(), any(OptimizationGuideCallback.class));
    }

    static void mockOptimizationGuideResponseAsync(
            OptimizationGuideBridge.Natives optimizationGuideJni, int optimizationType,
            @MockPriceTrackingResponse int expectedResponse) {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                OptimizationGuideCallback callback =
                        (OptimizationGuideCallback) invocation.getArguments()[3];
                switch (expectedResponse) {
                    case MockPriceTrackingResponse.BUYABLE_PRODUCT_INITIAL:
                        callback.onOptimizationGuideDecision(
                                OptimizationGuideDecision.TRUE, ANY_BUYABLE_PRODUCT_INITIAL);
                        break;
                    case MockPriceTrackingResponse.BUYABLE_PRODUCT_PRICE_UPDATED:
                        callback.onOptimizationGuideDecision(
                                OptimizationGuideDecision.TRUE, ANY_BUYABLE_PRODUCT_UPDATE);
                        break;
                    case MockPriceTrackingResponse.BUYABLE_PRODUCT_AND_PRODUCT_UPDATE:
                        callback.onOptimizationGuideDecision(OptimizationGuideDecision.TRUE,
                                ANY_PRICE_TRACKING_BUYABLE_PRODUCT_AND_PRODUCT_UPDATE);
                        break;
                    case MockPriceTrackingResponse.PRODUCT_PRICE_UPDATE:
                        callback.onOptimizationGuideDecision(
                                OptimizationGuideDecision.TRUE, ANY_PRICE_TRACKING_PRODUCT_UPDATE);
                        break;
                    case MockPriceTrackingResponse.BUYABLE_PRODUCT_EMPTY:
                        callback.onOptimizationGuideDecision(
                                OptimizationGuideDecision.TRUE, ANY_PRICE_TRACKING_EMPTY);
                        break;
                    case MockPriceTrackingResponse.NONE:
                        callback.onOptimizationGuideDecision(
                                OptimizationGuideDecision.FALSE, ANY_EMPTY);
                        break;
                    case MockPriceTrackingResponse.UNPARSEABLE:
                        callback.onOptimizationGuideDecision(
                                OptimizationGuideDecision.TRUE, ANY_UNPARSEABLE);
                        break;
                    default:
                        break;
                }
                return null;
            }
        })
                .when(optimizationGuideJni)
                .canApplyOptimizationAsync(
                        anyLong(), any(GURL.class), anyInt(), any(OptimizationGuideCallback.class));
    }

    static void verifyEndpointFetcherCalled(EndpointFetcher.Natives endpointFetcher, int numTimes) {
        verify(endpointFetcher, times(numTimes))
                .nativeFetchChromeAPIKey(any(Profile.class), anyString(), anyString(), anyString(),
                        anyString(), anyLong(), any(String[].class), anyInt(), any(Callback.class));
    }

    static void verifyPriceTrackingOptimizationTypeCalled(
            OptimizationGuideBridge.Natives optimizationGuideJni, int numTimes) {
        verify(optimizationGuideJni, times(numTimes))
                .canApplyOptimization(anyLong(), any(GURL.class),
                        eq(HintsProto.OptimizationType.PRICE_TRACKING.getNumber()),
                        any(OptimizationGuideCallback.class));
    }

    static void verifyOptimizationGuideCalledWithNavigationHandle(
            OptimizationGuideBridge.Natives optimizationGuideJni, GURL gurl) {
        verify(optimizationGuideJni, times(1))
                .canApplyOptimizationAsync(anyLong(), eq(gurl),
                        eq(HintsProto.OptimizationType.PRICE_TRACKING.getNumber()),
                        any(OptimizationGuideCallback.class));
    }
}
