// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_image_fetcher_impl.h"

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_art_image.h"
#include "components/autofill/core/browser/test_autofill_tick_clock.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "components/image_fetcher/core/mock_image_fetcher.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/resources/grit/ui_resources.h"

using base::Bucket;
using testing::_;

namespace autofill {

class TestAutofillImageFetcher : public AutofillImageFetcherImpl {
 public:
  explicit TestAutofillImageFetcher(
      image_fetcher::MockImageFetcher* image_fetcher)
      : AutofillImageFetcherImpl(nullptr) {
    image_fetcher_ = image_fetcher;
  }

  ~TestAutofillImageFetcher() override = default;
};

class AutofillImageFetcherImplTest : public testing::Test {
 public:
  void SetUp() override {
    image_fetcher_ = std::make_unique<image_fetcher::MockImageFetcher>();
    autofill_image_fetcher_ =
        std::make_unique<TestAutofillImageFetcher>(image_fetcher_.get());
  }

  void SimulateOnImageFetched(
      base::OnceCallback<void(std::unique_ptr<CreditCardArtImage>)>
          barrier_callback,
      const GURL& url,
      const absl::optional<base::TimeTicks>& fetch_image_request_timestamp,
      const gfx::Image& image) {
    autofill_image_fetcher()->OnCardArtImageFetched(
        std::move(barrier_callback), url, fetch_image_request_timestamp, image,
        image_fetcher::RequestMetadata());
  }

  void ValidateResult(const std::map<GURL, gfx::Image>& received_images,
                      const std::map<GURL, gfx::Image>& expected_images) {
    ASSERT_EQ(expected_images.size(), received_images.size());
    for (const auto& [expected_url, expected_image] : expected_images) {
      ASSERT_TRUE(base::Contains(received_images, expected_url));
      const gfx::Image& received_image = received_images.at(expected_url);
      EXPECT_TRUE(gfx::test::AreImagesEqual(expected_image, received_image));
    }
  }

  gfx::Image& GetTestImage(int resource_id) {
    return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
        resource_id);
  }

  image_fetcher::MockImageFetcher* mock_image_fetcher() {
    return image_fetcher_.get();
  }

  TestAutofillImageFetcher* autofill_image_fetcher() {
    return autofill_image_fetcher_.get();
  }

 private:
  std::unique_ptr<TestAutofillImageFetcher> autofill_image_fetcher_;
  std::unique_ptr<image_fetcher::MockImageFetcher> image_fetcher_;
};

TEST_F(AutofillImageFetcherImplTest, FetchImage_Success) {
  base::TimeTicks now = AutofillTickClock::NowTicks();
  TestAutofillTickClock test_clock;
  test_clock.SetNowTicks(now);

  // The credit card network images cannot be found in the tests, but it should
  // be okay since we don't care what the images are.
  gfx::Image fake_image1 = GetTestImage(IDR_DEFAULT_FAVICON);
  gfx::Image fake_image2 = GetTestImage(IDR_DEFAULT_FAVICON);
  GURL fake_url1 = GURL("https://www.example.com/fake_image1");
  GURL fake_url2 =
      GURL("https://www.gstatic.com/autofill/virtualcard/icon/capitalone.png");

  std::map<GURL, gfx::Image> expected_images = {
      {fake_url1, AutofillImageFetcherImpl::ApplyGreyOverlay(fake_image1)},
      {fake_url2, AutofillImageFetcherImpl::ApplyGreyOverlay(fake_image2)}};

  // Expect callback to be called with some received images.
  std::map<GURL, gfx::Image> received_images;
  const auto callback =
      base::BindLambdaForTesting([&](const CardArtImageData& card_art_images) {
        for (auto& entry : card_art_images)
          received_images[entry->card_art_url] = entry->card_art_image;
      });
  const auto barrier_callback =
      base::BarrierCallback<std::unique_ptr<CreditCardArtImage>>(
          2U, std::move(callback));

  base::HistogramTester histogram_tester;
  // Expect to be called twice.
  EXPECT_CALL(
      *mock_image_fetcher(),
      FetchImageAndData_(GURL(fake_url1.spec() + "=w32-h20-n"), _, _, _))
      .Times(1);
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(fake_url2, _, _, _))
      .Times(1);
  std::vector<GURL> urls = {fake_url1, fake_url2};
  autofill_image_fetcher()->FetchImagesForURLs(urls, base::DoNothing());

  // Advance the time to make the latency values more realistic.
  test_clock.Advance(base::Milliseconds(200));
  // Simulate successful image fetching (for image with URL) -> expect the
  // callback to be called.
  SimulateOnImageFetched(barrier_callback, fake_url1, now, fake_image1);
  SimulateOnImageFetched(barrier_callback, fake_url2, now, fake_image2);

  ValidateResult(std::move(received_images), expected_images);
  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.ImageFetcher.Result"),
              BucketsAre(Bucket(false, 0), Bucket(true, 2)));
  histogram_tester.ExpectTotalCount("Autofill.ImageFetcher.RequestLatency", 2);
  histogram_tester.ExpectUniqueSample("Autofill.ImageFetcher.RequestLatency",
                                      200, 2);
}

TEST_F(AutofillImageFetcherImplTest, FetchImage_InvalidUrlFailure) {
  base::TimeTicks now = AutofillTickClock::NowTicks();
  TestAutofillTickClock test_clock;
  test_clock.SetNowTicks(now);

  gfx::Image fake_image1 = GetTestImage(IDR_DEFAULT_FAVICON);
  GURL fake_url1 = GURL("http://www.example.com/fake_image1");
  std::map<GURL, gfx::Image> expected_images = {
      {fake_url1, AutofillImageFetcherImpl::ApplyGreyOverlay(fake_image1)}};

  std::map<GURL, gfx::Image> received_images;
  const auto callback =
      base::BindLambdaForTesting([&](const CardArtImageData& card_art_images) {
        for (auto& entry : card_art_images)
          received_images[entry->card_art_url] = entry->card_art_image;
      });
  const auto barrier_callback =
      base::BarrierCallback<std::unique_ptr<CreditCardArtImage>>(
          1U, std::move(callback));

  base::HistogramTester histogram_tester;
  // Expect to be called once with one invalid url.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(_, _, _, _)).Times(1);
  std::vector<GURL> urls = {fake_url1, GURL("")};
  autofill_image_fetcher()->FetchImagesForURLs(urls, base::DoNothing());

  test_clock.Advance(base::Milliseconds(200));
  // Simulate successful image fetching (for image with URL) -> expect the
  // callback to be called.
  SimulateOnImageFetched(barrier_callback, fake_url1, now, fake_image1);

  ValidateResult(std::move(received_images), expected_images);
  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.ImageFetcher.Result"),
              BucketsAre(Bucket(false, 1), Bucket(true, 1)));
  histogram_tester.ExpectTotalCount("Autofill.ImageFetcher.RequestLatency", 1);
  histogram_tester.ExpectUniqueSample("Autofill.ImageFetcher.RequestLatency",
                                      200, 1);
}

TEST_F(AutofillImageFetcherImplTest, FetchImage_ServerFailure) {
  base::TimeTicks now = AutofillTickClock::NowTicks();
  TestAutofillTickClock test_clock;
  test_clock.SetNowTicks(now);

  GURL fake_url1 = GURL("https://www.example.com/fake_image1");
  std::map<GURL, gfx::Image> expected_images = {{fake_url1, gfx::Image()}};

  // Expect callback to be called with some received images.
  std::map<GURL, gfx::Image> received_images;
  const auto callback =
      base::BindLambdaForTesting([&](const CardArtImageData& card_art_images) {
        for (auto& entry : card_art_images)
          received_images[entry->card_art_url] = entry->card_art_image;
      });
  const auto barrier_callback =
      base::BarrierCallback<std::unique_ptr<CreditCardArtImage>>(
          1U, std::move(callback));

  base::HistogramTester histogram_tester;
  // Expect to be called once.
  EXPECT_CALL(*mock_image_fetcher(), FetchImageAndData_(_, _, _, _)).Times(1);
  std::vector<GURL> urls = {fake_url1};
  autofill_image_fetcher()->FetchImagesForURLs(urls, base::DoNothing());

  test_clock.Advance(base::Milliseconds(200));
  // Simulate failed image fetching (for image with URL) -> expect the
  // callback to be called.
  SimulateOnImageFetched(barrier_callback, fake_url1, now, gfx::Image());

  ValidateResult(std::move(received_images), expected_images);
  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.ImageFetcher.Result"),
              BucketsAre(Bucket(false, 1), Bucket(true, 0)));
  histogram_tester.ExpectTotalCount("Autofill.ImageFetcher.RequestLatency", 1);
  histogram_tester.ExpectUniqueSample("Autofill.ImageFetcher.RequestLatency",
                                      200, 1);
}

}  // namespace autofill
