// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.graphics.Bitmap;

import androidx.annotation.Px;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.CardIconSpecs;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.image_fetcher.test.TestImageFetcher;
import org.chromium.url.GURL;

import java.util.Map;

/** Unit tests for {@link AutofillImageFetcher}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillImageFetcherTest {
    private static final Bitmap TEST_CARD_ART_IMAGE =
            Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);

    private AutofillImageFetcher mImageFetcher;

    @Before
    public void setUp() {
        mImageFetcher = new AutofillImageFetcher(new TestImageFetcher(TEST_CARD_ART_IMAGE));
    }

    @After
    public void tearDown() {
        mImageFetcher.clearCachedImagesForTesting();
    }

    @Test
    @SmallTest
    public void testPrefetchImages_validUrl_successfulImageFetch() {
        GURL validUrl1 = new GURL("https://www.google.com/valid-image-url-1");
        GURL validUrl2 = new GURL("https://www.google.com/valid-image-url-2");
        CardIconSpecs cardIconSpecsSmall =
                CardIconSpecs.create(ContextUtils.getApplicationContext(), ImageSize.SMALL);
        CardIconSpecs cardIconSpecsLarge =
                CardIconSpecs.create(ContextUtils.getApplicationContext(), ImageSize.LARGE);
        GURL cachedValidUrlSmall1 =
                AutofillUiUtils.getCreditCardIconUrlWithParams(
                        validUrl1, cardIconSpecsSmall.getWidth(), cardIconSpecsSmall.getHeight());
        GURL cachedValidUrlLarge1 =
                AutofillUiUtils.getCreditCardIconUrlWithParams(
                        validUrl1, cardIconSpecsLarge.getWidth(), cardIconSpecsLarge.getHeight());
        GURL cachedValidUrlSmall2 =
                AutofillUiUtils.getCreditCardIconUrlWithParams(
                        validUrl2, cardIconSpecsSmall.getWidth(), cardIconSpecsSmall.getHeight());
        GURL cachedValidUrlLarge2 =
                AutofillUiUtils.getCreditCardIconUrlWithParams(
                        validUrl2, cardIconSpecsLarge.getWidth(), cardIconSpecsLarge.getHeight());
        Bitmap treatedImageSmall =
                AutofillUiUtils.resizeAndAddRoundedCornersAndGreyBorder(
                        TEST_CARD_ART_IMAGE, cardIconSpecsSmall, true);
        Bitmap treatedImageLarge =
                AutofillUiUtils.resizeAndAddRoundedCornersAndGreyBorder(
                        TEST_CARD_ART_IMAGE, cardIconSpecsLarge, true);

        mImageFetcher.prefetchImages(
                new GURL[] {validUrl1, validUrl2}, new int[] {ImageSize.SMALL, ImageSize.LARGE});
        Map<String, Bitmap> cachedImages = mImageFetcher.getCachedImagesForTesting();

        // Each card art image is cached at 2 resolutions: 32x20 for the Keyboard Accessory, and
        // 40x24 on all other surfaces.
        assertEquals(4, cachedImages.size());
        assertTrue(treatedImageSmall.sameAs(cachedImages.get(cachedValidUrlSmall1.getSpec())));
        assertTrue(treatedImageSmall.sameAs(cachedImages.get(cachedValidUrlSmall2.getSpec())));
        assertTrue(treatedImageLarge.sameAs(cachedImages.get(cachedValidUrlLarge1.getSpec())));
        assertTrue(treatedImageLarge.sameAs(cachedImages.get(cachedValidUrlLarge2.getSpec())));
    }

    @Test
    @SmallTest
    public void testPrefetchImages_validUrl_unsuccessfulImageFetch() {
        mImageFetcher = new AutofillImageFetcher(new TestImageFetcher(null));
        GURL validUrl = new GURL("https://www.google.com/valid-image-url");

        mImageFetcher.prefetchImages(new GURL[] {validUrl}, new int[] {ImageSize.SMALL});

        assertTrue(mImageFetcher.getCachedImagesForTesting().isEmpty());
    }

    @Test
    @SmallTest
    public void testPrefetchImages_invalidOrEmptyUrl() {
        GURL invalidUrl = new GURL("invalid-image-url");
        GURL emptyUrl = new GURL("");

        mImageFetcher.prefetchImages(
                new GURL[] {invalidUrl, emptyUrl}, new int[] {ImageSize.SMALL});

        assertTrue(mImageFetcher.getCachedImagesForTesting().isEmpty());
    }

    @Test
    @SmallTest
    public void testPrefetchPixAccountImages_validUrl_successfulImageFetch() {
        GURL validUrl1 = new GURL("https://www.google.com/valid-image-url-1");
        GURL validUrl2 = new GURL("https://www.google.com/valid-image-url-2");
        @Px
        int logoSize = AutofillImageFetcherUtils.getPixelSize(R.dimen.square_card_icon_side_length);
        GURL cachedValidUrl1 =
                AutofillUiUtils.getCreditCardIconUrlWithParams(validUrl1, logoSize, logoSize);
        GURL cachedValidUrl2 =
                AutofillUiUtils.getCreditCardIconUrlWithParams(validUrl2, logoSize, logoSize);
        Bitmap treatedImage = AutofillImageFetcherUtils.treatPixAccountImage(TEST_CARD_ART_IMAGE);
        // Success histograms should be logged for both images.
        HistogramWatcher expectedHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                "Autofill.ImageFetcher.Result", /* value= */ true, /* times= */ 2)
                        .build();

        mImageFetcher.prefetchPixAccountImages(new GURL[] {validUrl1, validUrl2});
        Map<String, Bitmap> cachedImages = mImageFetcher.getCachedImagesForTesting();

        // Verify that the images are successfully fetched and cached.
        assertEquals(2, cachedImages.size());
        assertTrue(treatedImage.sameAs(cachedImages.get(cachedValidUrl1.getSpec())));
        assertTrue(treatedImage.sameAs(cachedImages.get(cachedValidUrl2.getSpec())));

        expectedHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testPrefetchPixAccountImages_imageInCache_imageNotFetched() {
        GURL validUrl = new GURL("https://www.google.com/valid-image-url");
        @Px
        int logoSize = AutofillImageFetcherUtils.getPixelSize(R.dimen.square_card_icon_side_length);
        GURL cachedValidUrl =
                AutofillUiUtils.getCreditCardIconUrlWithParams(validUrl, logoSize, logoSize);
        mImageFetcher.addImageToCacheForTesting(cachedValidUrl, TEST_CARD_ART_IMAGE);
        // No histogram should be logged since no image fetching is done.
        HistogramWatcher expectedHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Autofill.ImageFetcher.Result")
                        .build();

        mImageFetcher.prefetchPixAccountImages(new GURL[] {validUrl});
        Map<String, Bitmap> cachedImages = mImageFetcher.getCachedImagesForTesting();

        // Verify that the cache contains only the already cached image.
        assertEquals(1, cachedImages.size());
        assertTrue(TEST_CARD_ART_IMAGE.sameAs(cachedImages.get(cachedValidUrl.getSpec())));

        expectedHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testPrefetchPixAccountImages_validUrl_unsuccessfulImageFetch() {
        mImageFetcher = new AutofillImageFetcher(new TestImageFetcher(null));
        GURL validUrl = new GURL("https://www.google.com/valid-image-url");
        // Failure histogram should be logged since fetching was attempted but failed.
        HistogramWatcher expectedHistogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                "Autofill.ImageFetcher.Result", /* value= */ false, /* times= */ 1)
                        .build();

        mImageFetcher.prefetchPixAccountImages(new GURL[] {validUrl});

        // Verify that the cache is empty since image fetching failed.
        assertTrue(mImageFetcher.getCachedImagesForTesting().isEmpty());

        expectedHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testPrefetchPixAccountImages_invalidOrEmptyUrl() {
        GURL invalidUrl = new GURL("invalid-image-url");
        GURL emptyUrl = new GURL("");
        // No histogram should be logged since image fetching isn't attempted for invalid URLs.
        HistogramWatcher expectedHistogram =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Autofill.ImageFetcher.Result")
                        .build();

        mImageFetcher.prefetchPixAccountImages(new GURL[] {invalidUrl, emptyUrl});

        // Verify that the cache is empty since the image URLs weren't valid and no images were
        // fetched.
        assertTrue(mImageFetcher.getCachedImagesForTesting().isEmpty());

        expectedHistogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testGetImagesIfAvailable() {
        GURL cardArtUrl = new GURL("https://www.google.com/card-art-url");
        CardIconSpecs cardIconSpecs =
                CardIconSpecs.create(ContextUtils.getApplicationContext(), ImageSize.LARGE);
        Bitmap treatedImage =
                AutofillUiUtils.resizeAndAddRoundedCornersAndGreyBorder(
                        TEST_CARD_ART_IMAGE, cardIconSpecs, true);

        // The card art image is not present in the image cache, so a call is made to fetch the
        // image, and nothing is returned.
        assertFalse(mImageFetcher.getImageIfAvailable(cardArtUrl, cardIconSpecs).isPresent());

        // The card art image is fetched and cached from the previous call, so the cached image is
        // returned.
        assertTrue(
                mImageFetcher
                        .getImageIfAvailable(cardArtUrl, cardIconSpecs)
                        .get()
                        .sameAs(treatedImage));
    }
}
