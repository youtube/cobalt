// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/ui/autofill_image_fetcher_impl.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"

namespace autofill {

class AutofillImageFetcherImplTest : public testing::Test {
 public:
  AutofillImageFetcherImplTest()
      : autofill_image_fetcher_(
            std::make_unique<AutofillImageFetcherImpl>(nullptr)) {}

  gfx::Image& GetTestImage(int resource_id) {
    return ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
        resource_id);
  }

  AutofillImageFetcherImpl* autofill_image_fetcher() {
    return autofill_image_fetcher_.get();
  }

 private:
  std::unique_ptr<AutofillImageFetcherImpl> autofill_image_fetcher_;
};

// TODO(crbug.com/1313616): Write tests for
// kAutofillEnableNewCardArtAndNetworkImages code paths
TEST_F(AutofillImageFetcherImplTest, ResolveCardArtImage) {
  GURL card_art_url = GURL("https://www.example.com/fake_image1");
  // The credit card network images cannot be found in the tests, but it should
  // be okay since we don't care what the images are.
  gfx::Image card_art_image = GetTestImage(IDR_DEFAULT_FAVICON);
  gfx::Image resolved_image = autofill_image_fetcher()->ResolveCardArtImage(
      card_art_url, card_art_image);
  EXPECT_FALSE(gfx::test::AreImagesEqual(card_art_image, resolved_image));
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      AutofillImageFetcherImpl::ApplyGreyOverlay(card_art_image),
      resolved_image));

  // Empty images should not have greyscale applied.
  EXPECT_TRUE(gfx::test::AreImagesEqual(
      gfx::Image(), autofill_image_fetcher()->ResolveCardArtImage(
                        card_art_url, gfx::Image())));
}

class AutofillImageFetcherImplNewCardArtTest
    : public AutofillImageFetcherImplTest {
 private:
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillEnableNewCardArtAndNetworkImages};
};

TEST_F(AutofillImageFetcherImplNewCardArtTest, ResolveCardArtURL) {
  // With kAutofillEnableNewCardArtAndNetworkImages enabled, we fetch the image
  // at height of 48.
  EXPECT_EQ(GURL("https://www.example.com/fake_image1=h48-pa"),
            autofill_image_fetcher()->ResolveCardArtURL(
                GURL("https://www.example.com/fake_image1")));

  // The capitalone image is 'special' however, and we swap it out for the
  // larger variant.
  GURL capital_one_url = GURL(kCapitalOneCardArtUrl);
  EXPECT_EQ(GURL(kCapitalOneLargeCardArtUrl),
            autofill_image_fetcher()->ResolveCardArtURL(capital_one_url));
}

}  // namespace autofill
