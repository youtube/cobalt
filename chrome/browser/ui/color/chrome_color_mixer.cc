// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/chrome_color_mixer.h"

#include <string>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_provider_utils.h"
#include "chrome_color_id.h"
#include "components/omnibox/common/omnibox_features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

namespace {

// This differs from ui::SelectColorBasedOnInput in that we're checking if the
// input transform is *not* dark under the assumption that the background color
// *is* dark from a potential custom theme. Additionally, if the mode is
// explicitly dark just select the correct color for that mode.
ui::ColorTransform SelectColorBasedOnDarkInputOrMode(
    bool dark_mode,
    ui::ColorTransform input_transform,
    ui::ColorTransform dark_mode_color_transform,
    ui::ColorTransform light_mode_color_transform) {
  const auto generator = [](bool dark_mode, ui::ColorTransform input_transform,
                            ui::ColorTransform dark_mode_color_transform,
                            ui::ColorTransform light_mode_color_transform,
                            SkColor input_color, const ui::ColorMixer& mixer) {
    const SkColor transform_color = input_transform.Run(input_color, mixer);
    const SkColor dark_mode_color =
        dark_mode_color_transform.Run(input_color, mixer);
    const SkColor light_mode_color =
        light_mode_color_transform.Run(input_color, mixer);
    const SkColor result_color =
        dark_mode || !color_utils::IsDark(transform_color) ? dark_mode_color
                                                           : light_mode_color;
    DVLOG(2) << "ColorTransform SelectColorBasedOnDarkColorOrMode:"
             << " Dark Mode: " << dark_mode
             << " Transform Color: " << ui::SkColorName(transform_color)
             << " Dark Mode Color: " << ui::SkColorName(dark_mode_color)
             << " Light Mode Color: " << ui::SkColorName(light_mode_color)
             << " Result Color: " << ui::SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, dark_mode, std::move(input_transform),
                             std::move(dark_mode_color_transform),
                             std::move(light_mode_color_transform));
}

ui::ColorTransform GetToolbarTopSeparatorColorTransform(
    ui::ColorTransform toolbar_color_transform,
    ui::ColorTransform frame_color_transform) {
  const auto generator = [](ui::ColorTransform toolbar_color_transform,
                            ui::ColorTransform frame_color_transform,
                            SkColor input_color, const ui::ColorMixer& mixer) {
    const SkColor toolbar_color =
        toolbar_color_transform.Run(input_color, mixer);
    const SkColor frame_color = frame_color_transform.Run(input_color, mixer);
    const SkColor result_color =
        GetToolbarTopSeparatorColor(toolbar_color, frame_color);
    DVLOG(2) << "ColorTransform GetToolbarTopSeparatorColor:"
             << " Input Color: " << ui::SkColorName(input_color)
             << " Toolbar Transform Color: " << ui::SkColorName(toolbar_color)
             << " Frame Transform Color: " << ui::SkColorName(frame_color)
             << " Result Color: " << ui::SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(toolbar_color_transform),
                             std::move(frame_color_transform));
}

// Alpha of 61 = 24% opacity. Opacity of tab group chips in the bookmarks bar.
constexpr SkAlpha kTabGroupChipAlpha = 61;

// Apply updates to the Omnibox background color tokens per GM3 spec.
void ApplyGM3OmniboxBackgroundColor(ui::ColorMixer& mixer,
                                    const ui::ColorProviderManager::Key& key) {
  const bool gm3_background_color_enabled =
      features::GetChromeRefresh2023Level() ==
          features::ChromeRefresh2023Level::kLevel2 ||
      base::FeatureList::IsEnabled(omnibox::kOmniboxSteadyStateBackgroundColor);

  // Apply omnibox background color updates only to non-themed clients.
  if (gm3_background_color_enabled && !key.custom_theme) {
    // Retrieve GM3 omnibox background color params (Dark Mode).
    const std::string dark_background_color_param =
        omnibox::kOmniboxDarkBackgroundColor.Get();
    const std::string dark_background_color_hovered_param =
        omnibox::kOmniboxDarkBackgroundColorHovered.Get();

    // Retrieve GM3 omnibox background color params (Light Mode).
    const std::string light_background_color_param =
        omnibox::kOmniboxLightBackgroundColor.Get();
    const std::string light_background_color_hovered_param =
        omnibox::kOmniboxLightBackgroundColorHovered.Get();

    const auto string_to_skcolor = [](const std::string& rgb_str,
                                      SkColor* result) {
      // Valid color strings are of the form 0xRRGGBB or 0xAARRGGBB.
      const bool valid =
          result && (rgb_str.size() == 8 || rgb_str.size() == 10);
      if (!valid) {
        return false;
      }

      uint32_t parsed = 0;
      const bool success = base::HexStringToUInt(rgb_str, &parsed);
      if (success) {
        *result = SkColorSetA(static_cast<SkColor>(parsed), SK_AlphaOPAQUE);
      }
      return success;
    };

    SkColor dark_background_color = 0;
    SkColor dark_background_color_hovered = 0;

    SkColor light_background_color = 0;
    SkColor light_background_color_hovered = 0;

    const bool success = string_to_skcolor(dark_background_color_param,
                                           &dark_background_color) &&
                         string_to_skcolor(dark_background_color_hovered_param,
                                           &dark_background_color_hovered) &&
                         string_to_skcolor(light_background_color_param,
                                           &light_background_color) &&
                         string_to_skcolor(light_background_color_hovered_param,
                                           &light_background_color_hovered);

    if (!success) {
      return;
    }

    const auto selected_background_color = ui::SelectBasedOnDarkInput(
        kColorToolbar, dark_background_color, light_background_color);

    mixer[kColorLocationBarBackground] = {selected_background_color};

    const auto selected_background_color_hovered =
        ui::SelectBasedOnDarkInput(kColorToolbar, dark_background_color_hovered,
                                   light_background_color_hovered);

    mixer[kColorLocationBarBackgroundHovered] = {
        selected_background_color_hovered};
  }
}

}  // namespace

void AddChromeColorMixer(ui::ColorProvider* provider,
                         const ui::ColorProviderManager::Key& key) {
  const bool dark_mode =
      key.color_mode == ui::ColorProviderManager::ColorMode::kDark;
  ui::ColorMixer& mixer = provider->AddMixer();

  mixer[kColorAppMenuHighlightSeverityLow] = AdjustHighlightColorForContrast(
      ui::kColorAlertLowSeverity, kColorToolbar);
  mixer[kColorAppMenuHighlightSeverityHigh] = {
      kColorAvatarButtonHighlightSyncError};
  mixer[kColorAppMenuHighlightSeverityMedium] = AdjustHighlightColorForContrast(
      ui::kColorAlertMediumSeverityIcon, kColorToolbar);
  mixer[kColorAvatarButtonHighlightNormal] =
      AdjustHighlightColorForContrast(ui::kColorAccent, kColorToolbar);
  mixer[kColorAvatarButtonHighlightSyncError] = AdjustHighlightColorForContrast(
      ui::kColorAlertHighSeverity, kColorToolbar);
  mixer[kColorAvatarButtonHighlightSyncPaused] = {
      kColorAvatarButtonHighlightNormal};
  mixer[kColorAvatarStrokeLight] = {SK_ColorWHITE};
  mixer[kColorBookmarkBarBackground] = {kColorToolbar};
  mixer[kColorBookmarkBarForeground] = {kColorToolbarText};
  mixer[kColorBookmarkButtonIcon] = {kColorToolbarButtonIcon};
  mixer[kColorBookmarkFavicon] = ui::PickGoogleColor(
      gfx::kGoogleGrey500, kColorBookmarkBarBackground, 6.0f);
  mixer[kColorBookmarkFolderIcon] = {ui::kColorIcon};
  mixer[kColorBookmarkBarSeparator] = {kColorToolbarSeparator};
  mixer[kColorBookmarkDragImageBackground] = {ui::kColorAccent};
  mixer[kColorBookmarkDragImageCountBackground] = {ui::kColorAlertHighSeverity};
  mixer[kColorBookmarkDragImageCountForeground] =
      ui::GetColorWithMaxContrast(kColorBookmarkDragImageCountBackground);
  mixer[kColorBookmarkDragImageForeground] =
      ui::GetColorWithMaxContrast(kColorBookmarkDragImageBackground);
  mixer[kColorBookmarkDragImageIconBackground] = {
      kColorBookmarkDragImageForeground};
  mixer[kColorCaptionButtonBackground] = {SK_ColorTRANSPARENT};
  mixer[kColorCapturedTabContentsBorder] = {ui::kColorAccent};
  mixer[kColorDesktopMediaTabListBorder] = {ui::kColorMidground};
  mixer[kColorDesktopMediaTabListPreviewBackground] = {ui::kColorMidground};
  mixer[kColorDownloadBubbleInfoBackground] = {
      ui::kColorSubtleEmphasisBackground};
  mixer[kColorDownloadBubbleInfoIcon] = {ui::kColorIcon};
  mixer[kColorDownloadItemForeground] = {kColorDownloadShelfForeground};
  mixer[kColorDownloadItemForegroundDangerous] = ui::PickGoogleColor(
      ui::kColorAlertHighSeverity, kColorDownloadShelfBackground,
      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorDownloadItemForegroundDisabled] = BlendForMinContrast(
      ui::AlphaBlend(kColorDownloadItemForeground,
                     kColorDownloadShelfBackground, gfx::kGoogleGreyAlpha600),
      kColorDownloadShelfBackground, kColorDownloadItemForeground);
  mixer[kColorDownloadItemForegroundSafe] = ui::PickGoogleColor(
      ui::kColorAlertLowSeverity, kColorDownloadShelfBackground,
      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorDownloadItemProgressRingBackground] = ui::SetAlpha(
      kColorDownloadItemProgressRingForeground, gfx::kGoogleGreyAlpha400);
  mixer[kColorDownloadItemProgressRingForeground] = {ui::kColorThrobber};
  mixer[kColorDownloadShelfBackground] = {kColorToolbar};
  mixer[kColorDownloadShelfButtonBackground] = {kColorDownloadShelfBackground};
  mixer[kColorDownloadShelfButtonIcon] = {kColorToolbarButtonIcon};
  mixer[kColorDownloadShelfButtonIconDisabled] = {
      kColorToolbarButtonIconDisabled};
  mixer[kColorDownloadShelfButtonText] =
      ui::PickGoogleColor(ui::kColorAccent, kColorDownloadShelfBackground,
                          color_utils::kMinimumReadableContrastRatio);
  mixer[kColorDownloadShelfContentAreaSeparator] = ui::AlphaBlend(
      kColorDownloadShelfButtonIcon, kColorDownloadShelfBackground, 0x3A);
  mixer[kColorDownloadShelfForeground] = {kColorToolbarText};
  mixer[kColorDownloadStartedAnimationForeground] =
      PickGoogleColor(ui::kColorAccent, kColorDownloadShelfBackground,
                      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorDownloadToolbarButtonActive] =
      ui::PickGoogleColor(ui::kColorThrobber, kColorToolbar,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorDownloadToolbarButtonAnimationBackground] =
      ui::AlphaBlend(kColorDownloadToolbarButtonAnimationForeground,
                     kColorToolbar, kToolbarInkDropHighlightVisibleAlpha);
  mixer[kColorDownloadToolbarButtonAnimationForeground] =
      AdjustHighlightColorForContrast(ui::kColorAccent, kColorToolbar);
  mixer[kColorDownloadToolbarButtonInactive] = {kColorToolbarButtonIcon};
  mixer[kColorDownloadToolbarButtonRingBackground] = {
      SkColorSetA(kColorDownloadToolbarButtonInactive, 0x33)};
  mixer[kColorExtensionDialogBackground] = {SK_ColorWHITE};
  mixer[kColorExtensionIconBadgeBackgroundDefault] =
      PickGoogleColor(ui::kColorAccent, kColorToolbar,
                      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorExtensionIconDecorationAmbientShadow] =
      ui::SetAlpha(ui::kColorShadowBase, 0x26);
  mixer[kColorExtensionIconDecorationBackground] = {SK_ColorWHITE};
  mixer[kColorExtensionIconDecorationKeyShadow] =
      ui::SetAlpha(ui::kColorShadowBase, 0x4D);
  mixer[kColorExtensionMenuIcon] = {ui::kColorIcon};
  mixer[kColorExtensionMenuIconDisabled] = {ui::kColorIconDisabled};
  mixer[kColorExtensionMenuPinButtonIcon] = PickGoogleColor(
      ui::kColorAccentWithGuaranteedContrastAtopPrimaryBackground,
      ui::kColorMenuBackground, color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorExtensionMenuPinButtonIconDisabled] = ui::SetAlpha(
      kColorExtensionMenuPinButtonIcon, gfx::kDisabledControlAlpha);
  mixer[kColorExtensionsMenuHighlightedBackground] = {
      kColorToolbarBackgroundSubtleEmphasis};
  mixer[kColorExtensionsToolbarControlsBackground] = {
      kColorToolbarBackgroundSubtleEmphasis};
  mixer[kColorEyedropperBoundary] = {SK_ColorDKGRAY};
  mixer[kColorEyedropperCentralPixelInnerRing] = {SK_ColorBLACK};
  mixer[kColorEyedropperCentralPixelOuterRing] = {SK_ColorWHITE};
  mixer[kColorEyedropperGrid] = {SK_ColorGRAY};
  mixer[kColorFeaturePromoBubbleBackground] = {gfx::kGoogleBlue700};
  mixer[kColorFeaturePromoBubbleButtonBorder] = {gfx::kGoogleGrey300};
  mixer[kColorFeaturePromoBubbleCloseButtonInkDrop] = {gfx::kGoogleBlue300};
  mixer[kColorFeaturePromoBubbleDefaultButtonBackground] = {
      kColorFeaturePromoBubbleForeground};
  mixer[kColorFeaturePromoBubbleDefaultButtonForeground] = {
      kColorFeaturePromoBubbleBackground};
  mixer[kColorFeaturePromoBubbleForeground] = {SK_ColorWHITE};
  mixer[kColorFeatureLensPromoBubbleBackground] = {
      kColorFeaturePromoBubbleBackground};
  mixer[kColorFeatureLensPromoBubbleForeground] = {
      kColorFeaturePromoBubbleForeground};
  mixer[kColorFindBarBackground] = {ui::kColorTextfieldBackground};
  mixer[kColorFindBarButtonIcon] =
      ui::DeriveDefaultIconColor(ui::kColorTextfieldForeground);
  mixer[kColorFindBarButtonIconDisabled] =
      ui::DeriveDefaultIconColor(ui::kColorTextfieldForegroundDisabled);
  mixer[kColorFindBarForeground] = {ui::kColorTextfieldForeground};
  mixer[kColorFindBarMatchCount] = {ui::kColorSecondaryForeground};
  mixer[kColorFindBarSeparator] = {ui::kColorSeparator};
  mixer[kColorFlyingIndicatorBackground] = {kColorToolbar};
  mixer[kColorFlyingIndicatorForeground] = {kColorToolbarButtonIcon};
  mixer[kColorFocusHighlightDefault] = {SkColorSetRGB(0x10, 0x10, 0x10)};
  mixer[kColorFrameCaptionActive] =
      ui::GetColorWithMaxContrast({ui::kColorFrameActive});
  mixer[kColorFrameCaptionInactive] =
      ui::GetColorWithMaxContrast({ui::kColorFrameInactive});
  mixer[kColorInfoBarBackground] = {kColorToolbar};
  mixer[kColorInfoBarButtonIcon] = {kColorToolbarButtonIcon};
  mixer[kColorInfoBarButtonIconDisabled] = {kColorToolbarButtonIconDisabled};
  mixer[kColorInfoBarContentAreaSeparator] =
      ui::AlphaBlend(kColorInfoBarButtonIcon, kColorInfoBarBackground, 0x3A);
  mixer[kColorInfoBarForeground] = {kColorToolbarText};
  // kColorInfoBarIcon is referenced in //components/infobars, so
  // we can't use a color id from the chrome namespace. Here we're
  // overriding the default color with something more suitable.
  mixer[ui::kColorInfoBarIcon] =
      ui::PickGoogleColor(ui::kColorAccent, kColorInfoBarBackground,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorIntentPickerItemBackgroundHovered] = ui::SetAlpha(
      ui::GetColorWithMaxContrast(ui::kColorDialogBackground), 0x0F);  // 6%.
  mixer[kColorIntentPickerItemBackgroundSelected] = ui::BlendForMinContrast(
      ui::kColorDialogBackground, ui::kColorDialogBackground,
      ui::kColorAccentWithGuaranteedContrastAtopPrimaryBackground, 1.2);

  // By default, the Omnibox background color will be determined by the toolbar
  // color.
  mixer[kColorLocationBarBackground] = {kColorToolbarBackgroundSubtleEmphasis};
  mixer[kColorLocationBarBackgroundHovered] = {
      kColorToolbarBackgroundSubtleEmphasisHovered};

  // Override Omnibox background color tokens per GM3 spec when appropriate.
  ApplyGM3OmniboxBackgroundColor(mixer, key);

  mixer[kColorLocationBarBorder] = {SkColorSetA(SK_ColorBLACK, 0x4D)};
  mixer[kColorLocationBarBorderOpaque] =
      ui::GetResultingPaintColor(kColorLocationBarBorder, kColorToolbar);
  mixer[kColorMediaRouterIconActive] =
      PickGoogleColor(ui::kColorAccent, kColorToolbar,
                      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorMediaRouterIconWarning] = {ui::kColorAlertMediumSeverityIcon};
  mixer[kColorOmniboxAnswerIconBackground] = {
      ui::kColorButtonBackgroundProminent};
  mixer[kColorOmniboxAnswerIconForeground] = {
      ui::kColorButtonForegroundProminent};
  mixer[kColorOmniboxChipBackground] = {kColorTabBackgroundActiveFrameActive};
  mixer[kColorOmniboxChipForegroundLowVisibility] = {kColorToolbarButtonIcon};
  mixer[kColorOmniboxChipForegroundNormalVisibility] = {
      ui::kColorButtonForeground};
  mixer[kColorPageInfoChosenObjectDeleteButtonIcon] = {ui::kColorIcon};
  mixer[kColorPageInfoChosenObjectDeleteButtonIconDisabled] = {
      ui::kColorIconDisabled};
  mixer[kColorPaymentsFeedbackTipBackground] = {
      ui::kColorSubtleEmphasisBackground};
  mixer[kColorPaymentsFeedbackTipBorder] = {ui::kColorBubbleFooterBorder};
  mixer[kColorPaymentsFeedbackTipForeground] = {
      ui::kColorLabelForegroundSecondary};
  mixer[kColorPaymentsFeedbackTipIcon] = {ui::kColorAlertMediumSeverityIcon};
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  mixer[kColorPaymentsGooglePayLogo] = {dark_mode ? SK_ColorWHITE
                                                  : gfx::kGoogleGrey700};
#endif
  mixer[kColorPaymentsPromoCodeBackground] = {
      dark_mode ? SkColorSetA(gfx::kGoogleGreen300, 0x1F)
                : gfx::kGoogleGreen050};
  mixer[kColorPaymentsPromoCodeForeground] = {dark_mode ? gfx::kGoogleGreen300
                                                        : gfx::kGoogleGreen800};
  mixer[kColorPaymentsPromoCodeForegroundHovered] = {
      dark_mode ? gfx::kGoogleGreen200 : gfx::kGoogleGreen900};
  mixer[kColorPaymentsPromoCodeForegroundPressed] = {
      kColorPaymentsPromoCodeForegroundHovered};
  mixer[kColorPaymentsPromoCodeInkDrop] = {dark_mode ? gfx::kGoogleGreen300
                                                     : gfx::kGoogleGreen600};
  mixer[kColorPaymentsRequestBackArrowButtonIcon] = {ui::kColorIcon};
  mixer[kColorPaymentsRequestBackArrowButtonIconDisabled] = {
      ui::kColorIconDisabled};
  mixer[kColorPaymentsRequestRowBackgroundHighlighted] = {
      SkColorSetA(SK_ColorBLACK, 0x0D)};
  mixer[kColorPipWindowBackToTabButtonBackground] = {
      SkColorSetA(SK_ColorBLACK, 0x60)};
  mixer[kColorPipWindowBackground] = {SK_ColorBLACK};
  mixer[kColorPipWindowControlsBackground] = {
      SkColorSetA(gfx::kGoogleGrey900, 0xC1)};
  mixer[kColorPipWindowTopBarBackground] = {gfx::kGoogleGrey900};
  mixer[kColorPipWindowForeground] =
      ui::GetColorWithMaxContrast(kColorPipWindowBackground);
  mixer[kColorPipWindowForegroundInactive] = {gfx::kGoogleGrey500};
  mixer[kColorPipWindowHangUpButtonForeground] = {gfx::kGoogleRed300};
  mixer[kColorPipWindowSkipAdButtonBackground] = {gfx::kGoogleGrey700};
  mixer[kColorPipWindowSkipAdButtonBorder] = {kColorPipWindowForeground};
  // TODO(https://crbug.com/1315194): stop forcing the light theme once the
  // reauth dialog supports the dark mode.
  mixer[kColorProfilesReauthDialogBorder] = {SK_ColorWHITE};
  mixer[kColorQrCodeBackground] = {SK_ColorWHITE};
  mixer[kColorQrCodeBorder] = {ui::kColorMidground};
  mixer[kColorQuickAnswersReportQueryButtonBackground] = ui::SetAlpha(
      ui::kColorAccentWithGuaranteedContrastAtopPrimaryBackground, 0x0A);
  mixer[kColorQuickAnswersReportQueryButtonForeground] = PickGoogleColor(
      ui::kColorAccentWithGuaranteedContrastAtopPrimaryBackground,
      ui::GetResultingPaintColor(kColorQuickAnswersReportQueryButtonBackground,
                                 ui::kColorPrimaryBackground),
      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorScreenshotCapturedImageBackground] = {ui::kColorBubbleBackground};
  mixer[kColorScreenshotCapturedImageBorder] = {ui::kColorMidground};
  mixer[kColorSidePanelBackground] = {kColorToolbar};
  mixer[kColorSidePanelContentAreaSeparator] = {ui::kColorSeparator};
  mixer[kColorStatusBubbleBackgroundFrameActive] = {
      kColorTabBackgroundInactiveFrameActive};
  mixer[kColorStatusBubbleBackgroundFrameInactive] = {
      kColorTabBackgroundInactiveFrameInactive};
  mixer[kColorStatusBubbleForegroundFrameActive] = {
      kColorTabForegroundInactiveFrameActive};
  mixer[kColorStatusBubbleForegroundFrameInactive] = {
      kColorTabForegroundInactiveFrameInactive};
  mixer[kColorStatusBubbleShadow] = {SkColorSetA(SK_ColorBLACK, 0x1E)};
  mixer[kColorTabAlertAudioPlayingActiveFrameActive] = {
      kColorTabForegroundActiveFrameActive};
  mixer[kColorTabAlertAudioPlayingActiveFrameInactive] = {
      kColorTabForegroundActiveFrameInactive};
  mixer[kColorTabAlertAudioPlayingInactiveFrameActive] = {
      kColorTabForegroundInactiveFrameActive};
  mixer[kColorTabAlertAudioPlayingInactiveFrameInactive] = {
      kColorTabForegroundInactiveFrameInactive};
  mixer[kColorTabAlertMediaRecordingActiveFrameActive] =
      ui::SelectBasedOnDarkInput(kColorTabForegroundActiveFrameActive,
                                 gfx::kGoogleRed600, gfx::kGoogleRed300);
  mixer[kColorTabAlertMediaRecordingActiveFrameInactive] =
      ui::SelectBasedOnDarkInput(kColorTabForegroundActiveFrameInactive,
                                 gfx::kGoogleRed600, gfx::kGoogleRed300);
  mixer[kColorTabAlertMediaRecordingInactiveFrameActive] =
      ui::SelectBasedOnDarkInput(kColorTabForegroundInactiveFrameActive,
                                 gfx::kGoogleRed600, gfx::kGoogleRed300);
  mixer[kColorTabAlertMediaRecordingInactiveFrameInactive] =
      ui::SelectBasedOnDarkInput(kColorTabForegroundInactiveFrameInactive,
                                 gfx::kGoogleRed600, gfx::kGoogleRed300);
  mixer[kColorTabAlertPipPlayingActiveFrameActive] = ui::PickGoogleColor(
      ui::kColorAccent, kColorTabBackgroundActiveFrameActive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabAlertPipPlayingActiveFrameInactive] = ui::PickGoogleColor(
      ui::kColorAccent, kColorTabBackgroundActiveFrameInactive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabAlertPipPlayingInactiveFrameActive] = ui::PickGoogleColor(
      ui::kColorAccent, kColorTabBackgroundInactiveFrameActive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabAlertPipPlayingInactiveFrameInactive] = ui::PickGoogleColor(
      ui::kColorAccent, kColorTabBackgroundInactiveFrameInactive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabCloseButtonFocusRingActive] = ui::PickGoogleColor(
      ui::kColorFocusableBorderFocused, kColorTabBackgroundActiveFrameActive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabCloseButtonFocusRingInactive] = ui::PickGoogleColor(
      ui::kColorFocusableBorderFocused, kColorTabBackgroundInactiveFrameActive,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabFocusRingActive] = ui::PickGoogleColorTwoBackgrounds(
      ui::kColorFocusableBorderFocused, kColorTabBackgroundActiveFrameActive,
      ui::kColorFrameActive, color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorTabFocusRingInactive] = ui::PickGoogleColorTwoBackgrounds(
      ui::kColorFocusableBorderFocused, kColorTabBackgroundInactiveFrameActive,
      ui::kColorFrameActive, color_utils::kMinimumVisibleContrastRatio);

  mixer[kColorTabGroupTabStripFrameActiveBlue] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleBlue300, gfx::kGoogleBlue600);
  mixer[kColorTabGroupTabStripFrameActiveCyan] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleCyan300, gfx::kGoogleCyan900);
  mixer[kColorTabGroupTabStripFrameActiveGreen] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleGreen300, gfx::kGoogleGreen700);
  mixer[kColorTabGroupTabStripFrameActiveGrey] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleGrey300, gfx::kGoogleGrey700);
  mixer[kColorTabGroupTabStripFrameActiveOrange] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleOrange300, gfx::kGoogleOrange400);
  mixer[kColorTabGroupTabStripFrameActivePink] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGooglePink300, gfx::kGooglePink700);
  mixer[kColorTabGroupTabStripFrameActivePurple] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGooglePurple300, gfx::kGooglePurple500);
  mixer[kColorTabGroupTabStripFrameActiveRed] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleRed300, gfx::kGoogleRed600);
  mixer[kColorTabGroupTabStripFrameActiveYellow] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameActive,
                                 gfx::kGoogleYellow300, gfx::kGoogleYellow600);

  mixer[kColorTabGroupTabStripFrameInactiveBlue] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleBlue300, gfx::kGoogleBlue600);
  mixer[kColorTabGroupTabStripFrameInactiveCyan] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleCyan300, gfx::kGoogleCyan900);
  mixer[kColorTabGroupTabStripFrameInactiveGreen] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleGreen300, gfx::kGoogleGreen700);
  mixer[kColorTabGroupTabStripFrameInactiveGrey] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleGrey300, gfx::kGoogleGrey700);
  mixer[kColorTabGroupTabStripFrameInactiveOrange] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleOrange300, gfx::kGoogleOrange400);
  mixer[kColorTabGroupTabStripFrameInactivePink] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGooglePink300, gfx::kGooglePink700);
  mixer[kColorTabGroupTabStripFrameInactivePurple] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGooglePurple300, gfx::kGooglePurple500);
  mixer[kColorTabGroupTabStripFrameInactiveRed] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleRed300, gfx::kGoogleRed600);
  mixer[kColorTabGroupTabStripFrameInactiveYellow] =
      ui::SelectBasedOnDarkInput(kColorTabBackgroundInactiveFrameInactive,
                                 gfx::kGoogleYellow300, gfx::kGoogleYellow600);

  mixer[kColorTabGroupDialogBlue] = {kColorTabGroupContextMenuBlue};
  mixer[kColorTabGroupDialogCyan] = {kColorTabGroupContextMenuCyan};
  mixer[kColorTabGroupDialogGreen] = {kColorTabGroupContextMenuGreen};
  mixer[kColorTabGroupDialogGrey] = {kColorTabGroupContextMenuGrey};
  mixer[kColorTabGroupDialogOrange] = {kColorTabGroupContextMenuOrange};
  mixer[kColorTabGroupDialogPink] = {kColorTabGroupContextMenuPink};
  mixer[kColorTabGroupDialogPurple] = {kColorTabGroupContextMenuPurple};
  mixer[kColorTabGroupDialogRed] = {kColorTabGroupContextMenuRed};
  mixer[kColorTabGroupDialogYellow] = {kColorTabGroupContextMenuYellow};

  mixer[kColorTabGroupContextMenuBlue] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGoogleBlue300,
      gfx::kGoogleBlue600);
  mixer[kColorTabGroupContextMenuCyan] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGoogleCyan300,
      gfx::kGoogleCyan900);
  mixer[kColorTabGroupContextMenuGreen] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGoogleGreen300,
      gfx::kGoogleGreen700);
  mixer[kColorTabGroupContextMenuGrey] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGoogleGrey300,
      gfx::kGoogleGrey700);
  mixer[kColorTabGroupContextMenuOrange] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGoogleOrange300,
      gfx::kGoogleOrange400);
  mixer[kColorTabGroupContextMenuPink] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGooglePink300,
      gfx::kGooglePink700);
  mixer[kColorTabGroupContextMenuPurple] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGooglePurple300,
      gfx::kGooglePurple500);
  mixer[kColorTabGroupContextMenuRed] =
      SelectColorBasedOnDarkInputOrMode(dark_mode, kColorBookmarkBarForeground,
                                        gfx::kGoogleRed300, gfx::kGoogleRed600);
  mixer[kColorTabGroupContextMenuYellow] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground, gfx::kGoogleYellow300,
      gfx::kGoogleYellow600);

  mixer[kColorTabGroupBookmarkBarBlue] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground,
      ui::SetAlpha(gfx::kGoogleBlue300, kTabGroupChipAlpha),
      gfx::kGoogleBlue050);
  mixer[kColorTabGroupBookmarkBarCyan] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground,
      ui::SetAlpha(gfx::kGoogleCyan300, kTabGroupChipAlpha),
      gfx::kGoogleCyan050);
  mixer[kColorTabGroupBookmarkBarGreen] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground,
      ui::SetAlpha(gfx::kGoogleGreen300, kTabGroupChipAlpha),
      gfx::kGoogleGreen050);
  mixer[kColorTabGroupBookmarkBarGrey] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground,
      ui::SetAlpha(gfx::kGoogleGrey300, kTabGroupChipAlpha),
      gfx::kGoogleGrey100);
  mixer[kColorTabGroupBookmarkBarPink] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground,
      ui::SetAlpha(gfx::kGooglePink300, kTabGroupChipAlpha),
      gfx::kGooglePink050);
  mixer[kColorTabGroupBookmarkBarPurple] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground,
      ui::SetAlpha(gfx::kGooglePurple300, kTabGroupChipAlpha),
      gfx::kGooglePurple050);
  mixer[kColorTabGroupBookmarkBarRed] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground,
      ui::SetAlpha(gfx::kGoogleRed300, kTabGroupChipAlpha), gfx::kGoogleRed050);
  mixer[kColorTabGroupBookmarkBarYellow] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground,
      ui::SetAlpha(gfx::kGoogleYellow300, kTabGroupChipAlpha),
      gfx::kGoogleYellow050);
  mixer[kColorTabGroupBookmarkBarOrange] = SelectColorBasedOnDarkInputOrMode(
      dark_mode, kColorBookmarkBarForeground,
      ui::SetAlpha(gfx::kGoogleOrange300, kTabGroupChipAlpha),
      gfx::kGoogleOrange050);

  mixer[kColorTabHoverCardBackground] = {dark_mode ? gfx::kGoogleGrey900
                                                   : gfx::kGoogleGrey050};
  mixer[kColorTabHoverCardForeground] = {dark_mode ? gfx::kGoogleGrey700
                                                   : gfx::kGoogleGrey300};
  mixer[kColorTabStrokeFrameActive] = {kColorToolbarTopSeparatorFrameActive};
  mixer[kColorTabStrokeFrameInactive] = {
      kColorToolbarTopSeparatorFrameInactive};
  mixer[kColorTabstripLoadingProgressBackground] = ui::AlphaBlend(
      kColorTabstripLoadingProgressForeground, kColorToolbar, 0x32);
  // 4.5 and 6.0 approximate the default light and dark theme contrasts of
  // accent-against-toolbar.
  mixer[kColorTabstripLoadingProgressForeground] =
      PickGoogleColor(ui::kColorAccent, kColorToolbar, 4.5f, 6.0f);
  mixer[kColorTabstripScrollContainerShadow] =
      ui::SetAlpha(ui::kColorShadowBase, 0x4D);
  mixer[kColorTabThrobber] = {ui::kColorThrobber};
  mixer[kColorTabThrobberPreconnect] = {ui::kColorThrobberPreconnect};
  mixer[kColorThumbnailTabBackground] =
      ui::PickGoogleColor(ui::kColorAccent, ui::kColorFrameActive,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorThumbnailTabForeground] =
      ui::GetColorWithMaxContrast(kColorThumbnailTabBackground);
  mixer[kColorThumbnailTabStripBackgroundActive] = {ui::kColorFrameActive};
  mixer[kColorThumbnailTabStripBackgroundInactive] = {ui::kColorFrameInactive};
  mixer[kColorThumbnailTabStripTabGroupFrameActiveBlue] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleBlue300, gfx::kGoogleBlue600);
  mixer[kColorThumbnailTabStripTabGroupFrameActiveCyan] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleCyan300, gfx::kGoogleCyan900);
  mixer[kColorThumbnailTabStripTabGroupFrameActiveGreen] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleGreen300, gfx::kGoogleGreen700);
  mixer[kColorThumbnailTabStripTabGroupFrameActiveGrey] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleGrey300, gfx::kGoogleGrey700);
  mixer[kColorThumbnailTabStripTabGroupFrameActiveOrange] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleOrange300, gfx::kGoogleOrange400);
  mixer[kColorThumbnailTabStripTabGroupFrameActivePink] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGooglePink300, gfx::kGooglePink700);
  mixer[kColorThumbnailTabStripTabGroupFrameActivePurple] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGooglePurple300, gfx::kGooglePurple500);
  mixer[kColorThumbnailTabStripTabGroupFrameActiveRed] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleRed300, gfx::kGoogleRed600);
  mixer[kColorThumbnailTabStripTabGroupFrameActiveYellow] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundActive,
                                 gfx::kGoogleYellow300, gfx::kGoogleYellow600);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveBlue] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleBlue300, gfx::kGoogleBlue600);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveCyan] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleCyan300, gfx::kGoogleCyan900);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveGreen] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleGreen300, gfx::kGoogleGreen700);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveGrey] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleGrey300, gfx::kGoogleGrey700);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveOrange] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleOrange300, gfx::kGoogleOrange400);
  mixer[kColorThumbnailTabStripTabGroupFrameInactivePink] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGooglePink300, gfx::kGooglePink700);
  mixer[kColorThumbnailTabStripTabGroupFrameInactivePurple] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGooglePurple300, gfx::kGooglePurple500);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveRed] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleRed300, gfx::kGoogleRed600);
  mixer[kColorThumbnailTabStripTabGroupFrameInactiveYellow] =
      ui::SelectBasedOnDarkInput(kColorThumbnailTabStripBackgroundInactive,
                                 gfx::kGoogleYellow300, gfx::kGoogleYellow600);

  mixer[kColorToolbar] = {dark_mode ? SkColorSetRGB(0x35, 0x36, 0x3A)
                                    : SK_ColorWHITE};
  mixer[kColorToolbarButtonBackgroundHighlightedDefault] =
      ui::SetAlpha(ui::GetColorWithMaxContrast(kColorToolbarButtonText), 0xCC);
  mixer[kColorToolbarButtonBorder] = ui::SetAlpha(kColorToolbarInkDrop, 0x20);
  mixer[kColorToolbarButtonIcon] = {kColorToolbarButtonIconDefault};
  mixer[kColorToolbarButtonIconDefault] = ui::HSLShift(
      gfx::kGoogleGrey700, GetThemeTint(ThemeProperties::TINT_BUTTONS, key));
  mixer[kColorToolbarButtonIconDisabled] =
      ui::SetAlpha(kColorToolbarButtonIcon, gfx::kDisabledControlAlpha);
  mixer[kColorToolbarButtonIconHovered] = {kColorToolbarButtonIcon};
  mixer[kColorToolbarButtonIconInactive] = {
      ui::SetAlpha(kColorToolbarButtonIcon, gfx::kGoogleGreyAlpha500)};
  mixer[kColorToolbarButtonIconPressed] = {kColorToolbarButtonIconHovered};
  mixer[kColorToolbarButtonText] = ui::GetColorWithMaxContrast(kColorToolbar);
  mixer[kColorToolbarContentAreaSeparator] =
      ui::AlphaBlend(kColorToolbarButtonIcon, kColorToolbar, 0x3A);
  mixer[kColorToolbarFeaturePromoHighlight] =
      ui::PickGoogleColor(ui::kColorAccent, kColorToolbar,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorToolbarInkDrop] = ui::GetColorWithMaxContrast(kColorToolbar);
  mixer[kColorToolbarInkDropHover] =
      ui::SetAlpha(kColorToolbarInkDrop, kToolbarInkDropHighlightVisibleAlpha);
  mixer[kColorToolbarInkDropRipple] =
      ui::SetAlpha(kColorToolbarInkDrop, std::ceil(0.06f * 255.0f));
  mixer[kColorToolbarExtensionSeparatorEnabled] = {
      kColorTabBackgroundInactiveFrameActive};
  mixer[kColorToolbarExtensionSeparatorDisabled] = {
      kColorToolbarButtonIconInactive};
  mixer[kColorToolbarSeparator] = {kColorToolbarSeparatorDefault};
  mixer[kColorToolbarSeparatorDefault] =
      ui::SetAlpha(kColorToolbarButtonIcon, 0x4D);
  mixer[kColorToolbarText] = {kColorToolbarTextDefault};
  mixer[kColorToolbarTextDefault] = {dark_mode ? SK_ColorWHITE
                                               : gfx::kGoogleGrey800};
  mixer[kColorToolbarTextDisabled] = {kColorToolbarTextDisabledDefault};
  mixer[kColorToolbarTextDisabledDefault] =
      ui::SetAlpha(kColorToolbarText, gfx::kDisabledControlAlpha);
  mixer[kColorToolbarTopSeparatorFrameActive] =
      GetToolbarTopSeparatorColorTransform(kColorToolbar,
                                           ui::kColorFrameActive);
  mixer[kColorToolbarTopSeparatorFrameInactive] =
      GetToolbarTopSeparatorColorTransform(kColorToolbar,
                                           ui::kColorFrameInactive);
  mixer[kColorWebAuthnBackArrowButtonIcon] = {ui::kColorIcon};
  mixer[kColorWebAuthnBackArrowButtonIconDisabled] = {ui::kColorIconDisabled};
  mixer[kColorWebAuthnPinTextfieldBottomBorder] =
      PickGoogleColor(ui::kColorAccent, ui::kColorDialogBackground,
                      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorWebAuthnProgressRingBackground] = ui::SetAlpha(
      kColorWebAuthnProgressRingForeground, gfx::kGoogleGreyAlpha400);
  mixer[kColorWebAuthnProgressRingForeground] = {ui::kColorThrobber};
  mixer[kColorWebContentsBackground] = {kColorNewTabPageBackground};
  mixer[kColorWebContentsBackgroundLetterboxing] =
      ui::AlphaBlend(kColorWebContentsBackground, SK_ColorBLACK, 0x33);
  mixer[kColorWindowControlButtonBackgroundActive] = {ui::kColorFrameActive};
  mixer[kColorWindowControlButtonBackgroundInactive] = {
      ui::kColorFrameInactive};

  mixer[kColorReadAnythingBackground] = {
      dark_mode ? kColorReadAnythingBackgroundDark
                : kColorReadAnythingBackgroundLight};
  mixer[kColorReadAnythingBackgroundBlue] = {gfx::kGoogleBlue200};
  mixer[kColorReadAnythingBackgroundDark] = {gfx::kGoogleGrey900};
  mixer[kColorReadAnythingBackgroundLight] = {gfx::kGoogleGrey100};
  mixer[kColorReadAnythingBackgroundYellow] = {gfx::kGoogleYellow200};
  mixer[kColorReadAnythingForeground] = {
      dark_mode ? kColorReadAnythingForegroundDark
                : kColorReadAnythingForegroundLight};
  mixer[kColorReadAnythingForegroundBlue] = ui::PickGoogleColorTwoBackgrounds(
      kColorReadAnythingForegroundLight,
      kColorReadAnythingDropdownBackgroundBlue,
      kColorReadAnythingDropdownSelectedBlue,
      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorReadAnythingForegroundDark] = ui::PickGoogleColorTwoBackgrounds(
      gfx::kGoogleGrey200, kColorReadAnythingDropdownBackgroundDark,
      kColorReadAnythingDropdownSelectedDark,
      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorReadAnythingForegroundLight] = ui::PickGoogleColorTwoBackgrounds(
      gfx::kGoogleGrey800, kColorReadAnythingBackgroundLight,
      kColorReadAnythingDropdownSelectedLight,
      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorReadAnythingForegroundYellow] = ui::PickGoogleColorTwoBackgrounds(
      kColorReadAnythingForegroundLight, kColorReadAnythingBackgroundYellow,
      kColorReadAnythingDropdownSelectedYellow,
      color_utils::kMinimumReadableContrastRatio);
  mixer[kColorReadAnythingSeparator] = {dark_mode
                                            ? kColorReadAnythingSeparatorDark
                                            : kColorReadAnythingSeparatorLight};
  mixer[kColorReadAnythingSeparatorBlue] =
      ui::PickGoogleColor(gfx::kGoogleGrey500, kColorReadAnythingBackgroundBlue,
                          color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorReadAnythingSeparatorDark] = {gfx::kGoogleGrey800};
  mixer[kColorReadAnythingSeparatorLight] = {gfx::kGoogleGrey300};
  mixer[kColorReadAnythingSeparatorYellow] = ui::PickGoogleColor(
      gfx::kGoogleGrey500, kColorReadAnythingBackgroundYellow,
      color_utils::kMinimumVisibleContrastRatio);
  mixer[kColorReadAnythingDropdownBackground] = {
      dark_mode ? kColorReadAnythingDropdownBackgroundDark
                : kColorReadAnythingDropdownBackgroundLight};
  mixer[kColorReadAnythingDropdownBackgroundBlue] = {gfx::kGoogleBlue100};
  mixer[kColorReadAnythingDropdownBackgroundDark] = {gfx::kGoogleGrey900};
  mixer[kColorReadAnythingDropdownBackgroundLight] = {SK_ColorWHITE};
  mixer[kColorReadAnythingDropdownBackgroundYellow] = {gfx::kGoogleYellow050};
  mixer[kColorReadAnythingDropdownSelected] = {
      dark_mode ? kColorReadAnythingDropdownSelectedDark
                : kColorReadAnythingDropdownSelectedLight};
  mixer[kColorReadAnythingDropdownSelectedBlue] = {gfx::kGoogleBlue200};
  mixer[kColorReadAnythingDropdownSelectedDark] = {gfx::kGoogleGrey800};
  mixer[kColorReadAnythingDropdownSelectedLight] = {gfx::kGoogleGrey200};
  mixer[kColorReadAnythingDropdownSelectedYellow] = {gfx::kGoogleYellow200};
  // Apply high contrast recipes if necessary.
  if (!ShouldApplyHighContrastColors(key)) {
    return;
  }
  mixer[kColorDownloadShelfContentAreaSeparator] = {
      kColorToolbarContentAreaSeparator};
  mixer[kColorInfoBarContentAreaSeparator] = {
      kColorToolbarContentAreaSeparator};
  mixer[kColorLocationBarBorder] = {kColorToolbarText};
  mixer[kColorToolbar] = {dark_mode ? SK_ColorBLACK : SK_ColorWHITE};
  mixer[kColorToolbarContentAreaSeparator] = {kColorToolbarText};
  mixer[kColorToolbarText] = {dark_mode ? SK_ColorWHITE : SK_ColorBLACK};
  mixer[kColorToolbarTopSeparatorFrameActive] = {dark_mode ? SK_ColorDKGRAY
                                                           : SK_ColorLTGRAY};
  mixer[ui::kColorFrameActive] = {SK_ColorDKGRAY};
  mixer[ui::kColorFrameInactive] = {SK_ColorGRAY};
}
