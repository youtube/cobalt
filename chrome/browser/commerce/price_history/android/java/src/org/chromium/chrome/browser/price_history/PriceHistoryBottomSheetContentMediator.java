// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_history;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.OPEN_URL_BUTTON_ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.OPEN_URL_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.PRICE_HISTORY_CHART;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.PRICE_HISTORY_CHART_CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.PRICE_HISTORY_DESCRIPTION;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.PRICE_HISTORY_DESCRIPTION_VISIBLE;
import static org.chromium.chrome.browser.price_history.PriceHistoryBottomSheetContentProperties.PRICE_HISTORY_TITLE;

import android.content.Context;

import androidx.annotation.StringRes;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.commerce.core.PriceBucket;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfo;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Mediator for price history bottom sheet responsible for property model update. */
@NullMarked
public class PriceHistoryBottomSheetContentMediator {
    private final Context mContext;
    private final Supplier<Tab> mTabSupplier;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;
    private final PropertyModel mPropertyModel;
    private final PriceInsightsDelegate mPriceInsightsDelegate;

    private @PriceBucket int mPriceBucket;

    public PriceHistoryBottomSheetContentMediator(
            Context context,
            Supplier<Tab> tabSupplier,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            PropertyModel propertyModel,
            PriceInsightsDelegate priceInsightsDelegate) {
        mContext = context;
        mTabSupplier = tabSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mPropertyModel = propertyModel;
        mPriceInsightsDelegate = priceInsightsDelegate;
    }

    public void requestShowContent(Callback<Boolean> contentReadyCallback) {
        ShoppingService shoppingService =
                ShoppingServiceFactory.getForProfile(mTabSupplier.get().getProfile());
        if (shoppingService == null || !shoppingService.isPriceInsightsEligible()) {
            contentReadyCallback.onResult(false);
        }
        shoppingService.getPriceInsightsInfoForUrl(
                mTabSupplier.get().getUrl(),
                (url, info) -> {
                    boolean hasPriceInsightInfo = isValidPriceInsightsInfo(info);
                    if (hasPriceInsightInfo) {
                        updatePriceInsightsInfo(assertNonNull(info));
                    }
                    contentReadyCallback.onResult(hasPriceInsightInfo);
                });
    }

    @Contract("null -> false")
    private boolean isValidPriceInsightsInfo(@Nullable PriceInsightsInfo info) {
        return info != null
                && !info.currencyCode.isEmpty()
                && info.catalogHistoryPrices != null
                && !info.catalogHistoryPrices.isEmpty();
    }

    private void updatePriceInsightsInfo(PriceInsightsInfo info) {
        mPropertyModel.set(PRICE_HISTORY_CHART_CONTENT_DESCRIPTION, mTabSupplier.get().getTitle());
        mPriceBucket = info.priceBucket;
        @StringRes int priceHistoryTitleResId = R.string.price_history_title;
        boolean hasMultipleCatalogs =
                info.hasMultipleCatalogs
                        && info.catalogAttributes != null
                        && !info.catalogAttributes.isEmpty();
        mPropertyModel.set(PRICE_HISTORY_DESCRIPTION_VISIBLE, hasMultipleCatalogs);
        if (hasMultipleCatalogs) {
            priceHistoryTitleResId = R.string.price_history_multiple_catalogs_title;
            mPropertyModel.set(PRICE_HISTORY_DESCRIPTION, info.catalogAttributes.get());
        }
        mPropertyModel.set(PRICE_HISTORY_TITLE, mContext.getString(priceHistoryTitleResId));
        mPropertyModel.set(
                PRICE_HISTORY_CHART,
                mPriceInsightsDelegate.getPriceHistoryChartForPriceInsightsInfo(info));

        boolean hasJackpotUrl = info.jackpotUrl != null && !info.jackpotUrl.isEmpty();
        mPropertyModel.set(OPEN_URL_BUTTON_VISIBLE, hasJackpotUrl);
        if (hasJackpotUrl) {
            mPropertyModel.set(
                    OPEN_URL_BUTTON_ON_CLICK_LISTENER,
                    view -> openJackpotUrl(info.jackpotUrl.get()));
        }
    }

    private void openJackpotUrl(GURL url) {
        RecordHistogram.recordEnumeratedHistogram(
                "Commerce.PriceInsights.BuyingOptionsClicked",
                mPriceBucket,
                PriceBucket.MAX_VALUE + 1);
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        mTabModelSelectorSupplier
                .get()
                .openNewTab(
                        loadUrlParams,
                        TabLaunchType.FROM_LINK,
                        mTabSupplier.get(),
                        /* incognito= */ false);
    }
}
