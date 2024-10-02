// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/translate/translate_infobar_banner_overlay_mediator.h"

#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/translate_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/translate/fake_translate_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/banners/test/fake_infobar_banner_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using translate_infobar_overlays::TranslateBannerRequestConfig;

// Test fixture for TranslateInfobarBannerOverlayMediator.
class TranslateInfobarBannerOverlayMediatorTest : public PlatformTest {
 public:
  TranslateInfobarBannerOverlayMediatorTest() {}

 protected:
  FakeTranslateInfoBarDelegateFactory delegate_factory_;
};

// Tests that a TranslateInfobarBannerOverlayMediator correctly sets up its
// consumer.
TEST_F(TranslateInfobarBannerOverlayMediatorTest, SetUpConsumer) {
  InfoBarIOS infobar(
      InfobarType::kInfobarTypeTranslate,
      delegate_factory_.CreateFakeTranslateInfoBarDelegate("fr", "en"));
  // Package the infobar into an OverlayRequest, then create a mediator that
  // uses this request in order to set up a fake consumer.
  std::unique_ptr<OverlayRequest> request =
      OverlayRequest::CreateWithConfig<TranslateBannerRequestConfig>(&infobar);
  TranslateInfobarBannerOverlayMediator* mediator =
      [[TranslateInfobarBannerOverlayMediator alloc]
          initWithRequest:request.get()];
  FakeInfobarBannerConsumer* consumer =
      [[FakeInfobarBannerConsumer alloc] init];
  mediator.consumer = consumer;
  // Verify that the infobar was set up properly.
  NSString* title = l10n_util::GetNSString(
      IDS_IOS_TRANSLATE_INFOBAR_BEFORE_TRANSLATE_BANNER_TITLE);
  NSString* subtitle = l10n_util::GetNSStringF(
      IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_BANNER_SUBTITLE,
      base::SysNSStringToUTF16(@"fr"), base::SysNSStringToUTF16(@"en"));

  EXPECT_NSEQ(title, consumer.titleText);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_TRANSLATE_INFOBAR_TRANSLATE_ACTION),
      consumer.buttonText);
  EXPECT_NSEQ(subtitle, consumer.subtitleText);
  EXPECT_NSEQ(CustomSymbolTemplateWithPointSize(kTranslateSymbol,
                                                kInfobarSymbolPointSize),
              consumer.iconImage);
}
