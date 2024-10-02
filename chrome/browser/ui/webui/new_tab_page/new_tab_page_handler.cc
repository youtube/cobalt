// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/new_tab_page/customize_chrome/customize_chrome_feature_promo_helper.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"
#include "chrome/browser/new_tab_page/promos/promo_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_background_service.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_tab_helper.h"
#include "chrome/browser/ui/side_panel/customize_chrome/customize_chrome_utils.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_pref_names.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/omnibox/browser/omnibox.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_provider_logos/logo_service.h"
#include "components/search_provider_logos/switches.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

namespace {

const int64_t kMaxDownloadBytes = 1024 * 1024;
const int64_t kMaxModuleFreImpressions = 8;

// Returns a list of module IDs that are eligible for HATS.
std::vector<std::string> GetSurveyEligibleModuleIds() {
  return base::SplitString(
      base::GetFieldTrialParamValueByFeature(
          features::kHappinessTrackingSurveysForDesktopNtpModules,
          ntp_features::kNtpModulesEligibleForHappinessTrackingSurveyParam),
      ",:;", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
}

// Returns true if the scrim (dark gradient overlay) should be hidden for the
// NTP's background image. This is done to fix specific GWS themes where the
// visual impact of the scrim does not align with the artistic intent of
// partnered artists (see crbug.com/1329556).
// TODO(crbug.com/1320107): Remove the scrim for all GWS and custom background
// image configurations on the NTP.
bool ShouldHideScrim(const ThemeService* theme_service) {
  DCHECK(theme_service);
  const auto* theme_supplier = theme_service->GetThemeSupplier();
  if (!theme_supplier ||
      theme_supplier->get_theme_type() !=
          ui::ColorProviderManager::ThemeInitializerSupplier::ThemeType::
              kExtension) {
    return false;
  }

  static constexpr auto kPrideThemeExtensionIdsNoScrim = base::MakeFixedFlatSet<
      base::StringPiece>({
      "kkdpcclippggiadgghfmkggpemadbfcj", "jogkmkalhlbppkpmjdpncmpdcinbkekh",
      "gfkcjfbbpmldkajnebkophpelmcimglf", "mchijkgkaabamaokgcnbmjpfoagkpjfc",
      "cdabkdaechplopdfoahhjgkbjgillcme", "depfhkphmnoonikdokgpejilanmcdonk",
      "efiamifmcbajfehbkjemggiafognbljk", "iclkbhippclhfamkdoigedgnnfbhefpl",
      "ckfehdejjppobbllbkjgcpaockgdigen", "figmdifbokklifinmmjcjdkkopjflhnj",
      "nkgiaofmleojhehacfognclpmoolihko", "npkdokffjmnleabnfihminmikibdhmfa",
      "klnkeldihpjnjoopojllmnpepbpljico", "iffdmpenldeofnlfjmbjcdmafhoekmka",
      "mckialangcdpcdcflekinnpamfkmkobo", "gpgkmnadnanefkpfkmdeijfiobhjagfk",
      "hhdddgombcggoeedkgelollagijjgnmo", "inneonpkbfaipkmpldnhnpefjkacjlcl",
      "inmnnmkfonobaklbnnfgekapnhnhlnnk",
  });

  const std::string& extension_id = theme_supplier->extension_id();
  return base::Contains(kPrideThemeExtensionIdsNoScrim, extension_id);
}

// Returns true if we should force dark foreground colors for the Google logo
// and the One Google Bar. This is done to fix specific GWS themes where the
// always-light logo and OGB colors do not sufficiently contrast with lighter
// image backgrounds (see crbug.com/1329552).
// TODO(crbug.com/1328918): Address this in a general way and extend support to
// custom background images, not just CWS themes.
bool ShouldForceDarkForegroundColorsForLogoAndOGB(
    const ThemeService* theme_service) {
  const auto* theme_supplier = theme_service->GetThemeSupplier();
  if (!theme_supplier ||
      theme_supplier->get_theme_type() !=
          ui::ColorProviderManager::ThemeInitializerSupplier::ThemeType::
              kExtension) {
    return false;
  }
  static constexpr auto kPrideThemeExtensionIdsDarkForeground =
      base::MakeFixedFlatSet<base::StringPiece>({
          "klnkeldihpjnjoopojllmnpepbpljico",
          "iffdmpenldeofnlfjmbjcdmafhoekmka",
          "mckialangcdpcdcflekinnpamfkmkobo",
      });

  const std::string& extension_id = theme_supplier->extension_id();
  return base::Contains(kPrideThemeExtensionIdsDarkForeground, extension_id);
}

new_tab_page::mojom::ThemePtr MakeTheme(
    const ui::ColorProvider& color_provider,
    const ui::ThemeProvider* theme_provider,
    ThemeService* theme_service,
    NtpCustomBackgroundService* ntp_custom_background_service,
    content::WebContents* web_contents) {
  if (ntp_custom_background_service) {
    ntp_custom_background_service->RefreshBackgroundIfNeeded();
  }
  auto theme = new_tab_page::mojom::Theme::New();
  auto most_visited = most_visited::mojom::MostVisitedTheme::New();
  auto custom_background =
      ntp_custom_background_service
          ? ntp_custom_background_service->GetCustomBackground()
          : absl::nullopt;
  theme->background_color = color_provider.GetColor(kColorNewTabPageBackground);
  const bool remove_scrim =
      base::FeatureList::IsEnabled(ntp_features::kNtpRemoveScrim);
  const bool theme_has_custom_image =
      theme_provider->HasCustomImage(IDR_THEME_NTP_BACKGROUND);
  SkColor text_color;
  if (custom_background.has_value()) {
    text_color = color_provider.GetColor(kColorNewTabPageTextUnthemed);
    theme->logo_color =
        color_provider.GetColor(kColorNewTabPageLogoUnthemedLight);
    most_visited->background_color = color_provider.GetColor(
        base::FeatureList::IsEnabled(ntp_features::kNtpComprehensiveTheming)
            ? kColorNewTabPageMostVisitedTileBackground
            : kColorNewTabPageMostVisitedTileBackgroundUnthemed);
  } else if (theme_provider->HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    text_color = color_provider.GetColor(
        remove_scrim ? kColorNewTabPageTextUnthemed : kColorNewTabPageText);
    if (remove_scrim) {
      theme->logo_color =
          color_provider.GetColor(kColorNewTabPageLogoUnthemedLight);
    } else if (theme_provider->GetDisplayProperty(
                   ThemeProperties::NTP_LOGO_ALTERNATE) == 1) {
      theme->logo_color = color_provider.GetColor(kColorNewTabPageLogo);
    }
    most_visited->background_color = color_provider.GetColor(
        kColorNewTabPageMostVisitedTileBackgroundUnthemed);
  } else {
    text_color = color_provider.GetColor(kColorNewTabPageText);
    if (theme_provider->GetDisplayProperty(
            ThemeProperties::NTP_LOGO_ALTERNATE) == 1) {
      theme->logo_color = color_provider.GetColor(kColorNewTabPageLogo);
    }
    most_visited->background_color =
        color_provider.GetColor(kColorNewTabPageMostVisitedTileBackground);
  }

  most_visited->use_white_tile_icon =
      color_utils::IsDark(most_visited->background_color);
  most_visited->is_dark = !color_utils::IsDark(text_color);
  most_visited->use_title_pill = false;
  theme->text_color = text_color;
  theme->is_dark = !color_utils::IsDark(text_color);
  theme->theme_realbox_icons =
      base::FeatureList::IsEnabled(ntp_features::kNtpComprehensiveTheming) &&
      base::FeatureList::IsEnabled(
          ntp_features::kNtpComprehensiveThemeRealbox) &&
      theme_service->UsingAutogeneratedTheme() &&
      theme->background_color != SK_ColorWHITE;
  auto background_image = new_tab_page::mojom::BackgroundImage::New();
  const bool side_panel_enabled = customize_chrome::IsSidePanelEnabled();
  if (theme_has_custom_image &&
      (!custom_background.has_value() || side_panel_enabled)) {
    if (theme_service->UsingExtensionTheme()) {
      background_image->image_source =
          new_tab_page::mojom::NtpBackgroundImageSource::kThirdPartyTheme;
    }
    theme->is_custom_background = false;
    most_visited->use_title_pill = !remove_scrim;
    auto theme_id = theme_service->GetThemeID();
    background_image->url = GURL(base::StrCat(
        {"chrome-untrusted://theme/IDR_THEME_NTP_BACKGROUND?", theme_id}));
    background_image->url_2x = GURL(base::StrCat(
        {"chrome-untrusted://theme/IDR_THEME_NTP_BACKGROUND@2x?", theme_id}));
    if (theme_provider->HasCustomImage(IDR_THEME_NTP_ATTRIBUTION)) {
      background_image->attribution_url = GURL(base::StrCat(
          {"chrome://theme/IDR_THEME_NTP_ATTRIBUTION?", theme_id}));
    }
    background_image->size = "initial";
    switch (theme_provider->GetDisplayProperty(
        ThemeProperties::NTP_BACKGROUND_TILING)) {
      case ThemeProperties::NO_REPEAT:
        background_image->repeat_x = "no-repeat";
        background_image->repeat_y = "no-repeat";
        break;
      case ThemeProperties::REPEAT_X:
        background_image->repeat_x = "repeat";
        background_image->repeat_y = "no-repeat";
        break;
      case ThemeProperties::REPEAT_Y:
        background_image->repeat_x = "no-repeat";
        background_image->repeat_y = "repeat";
        break;
      case ThemeProperties::REPEAT:
        background_image->repeat_x = "repeat";
        background_image->repeat_y = "repeat";
        break;
    }
    int alignment = theme_provider->GetDisplayProperty(
        ThemeProperties::NTP_BACKGROUND_ALIGNMENT);
    if (alignment & ThemeProperties::ALIGN_LEFT) {
      background_image->position_x = "left";
    } else if (alignment & ThemeProperties::ALIGN_RIGHT) {
      background_image->position_x = "right";
    } else {
      background_image->position_x = "center";
    }
    if (alignment & ThemeProperties::ALIGN_TOP) {
      background_image->position_y = "top";
    } else if (alignment & ThemeProperties::ALIGN_BOTTOM) {
      background_image->position_y = "bottom";
    } else {
      background_image->position_y = "center";
    }
  } else if (custom_background.has_value()) {
    theme->is_custom_background = true;
    background_image->url = custom_background->custom_background_url;
    new_tab_page::mojom::NtpBackgroundImageSource image_source = new_tab_page::
        mojom::NtpBackgroundImageSource::kFirstPartyThemeWithoutDailyRefresh;
    if (custom_background->daily_refresh_enabled) {
      image_source = new_tab_page::mojom::NtpBackgroundImageSource::
          kFirstPartyThemeWithDailyRefresh;
    } else if (custom_background->is_uploaded_image) {
      image_source =
          new_tab_page::mojom::NtpBackgroundImageSource::kUploadedImage;
    }
    background_image->image_source = image_source;
  } else {
    background_image = nullptr;
  }

  if (remove_scrim && background_image) {
    // If a background image is defined and the scrim removal flag is active
    // disable the scrim.
    background_image->scrim_display = "none";
  }

  // The special case handling that removes the scrim and forces dark foreground
  // colors should only be applied when the user does not have a custom
  // background selected and has installed a GWS theme with a bundled background
  // image. The first condition is necessary as a custom background image can be
  // set while a GWS theme with a bundled image is concurrently enabled (see
  // crbug.com/1329556 and crbug.com/1329552).
  if (base::FeatureList::IsEnabled(ntp_features::kCwsScrimRemoval) &&
      !custom_background.has_value() && theme_has_custom_image) {
    if (ShouldForceDarkForegroundColorsForLogoAndOGB(theme_service)) {
      theme->is_dark = false;
      theme->logo_color =
          color_provider.GetColor(kColorNewTabPageLogoUnthemedDark);
    }
    if (ShouldHideScrim(theme_service))
      background_image->scrim_display = "none";
  }

  theme->background_image = std::move(background_image);
  if (custom_background.has_value() &&
      (!side_panel_enabled || !theme_has_custom_image)) {
    theme->background_image_attribution_1 =
        custom_background->custom_background_attribution_line_1;
    theme->background_image_attribution_2 =
        custom_background->custom_background_attribution_line_2;
    theme->background_image_attribution_url =
        custom_background->custom_background_attribution_action_url;
    theme->background_image_collection_id = custom_background->collection_id;
    theme->daily_refresh_enabled = custom_background->daily_refresh_enabled;
  }

  theme->most_visited = std::move(most_visited);

  return theme;
}

SkColor ParseHexColor(const std::string& color) {
  SkColor result;
  if (color.size() == 7 && color[0] == '#' &&
      base::HexStringToUInt(color.substr(1), &result)) {
    return SkColorSetA(result, SK_AlphaOPAQUE);
  }
  return SK_ColorTRANSPARENT;
}

new_tab_page::mojom::ImageDoodlePtr MakeImageDoodle(
    search_provider_logos::LogoType type,
    const std::string& data,
    const std::string& mime_type,
    const GURL& animated_url,
    int width_px,
    int height_px,
    const std::string& background_color,
    int share_button_x,
    int share_button_y,
    const std::string& share_button_icon,
    const std::string& share_button_bg,
    double share_button_opacity,
    GURL log_url,
    GURL cta_log_url) {
  auto doodle = new_tab_page::mojom::ImageDoodle::New();
  std::string base64;
  base::Base64Encode(data, &base64);
  doodle->image_url = GURL(base::StringPrintf(
      "data:%s;base64,%s", mime_type.c_str(), base64.c_str()));
  if (type == search_provider_logos::LogoType::ANIMATED) {
    doodle->animation_url = animated_url;
  }
  doodle->width = width_px;
  doodle->height = height_px;
  doodle->background_color = ParseHexColor(background_color);
  if (!share_button_icon.empty()) {
    doodle->share_button = new_tab_page::mojom::DoodleShareButton::New();
    doodle->share_button->x = share_button_x;
    doodle->share_button->y = share_button_y;
    doodle->share_button->icon_url = GURL(base::StringPrintf(
        "data:image/png;base64,%s", share_button_icon.c_str()));
    doodle->share_button->background_color = SkColorSetA(
        ParseHexColor(share_button_bg),
        std::clamp(share_button_opacity, 0.0, 1.0) * SK_AlphaOPAQUE);
  }
  if (type == search_provider_logos::LogoType::ANIMATED) {
    doodle->image_impression_log_url = cta_log_url;
    doodle->animation_impression_log_url = log_url;
  } else {
    doodle->image_impression_log_url = log_url;
  }
  return doodle;
}

new_tab_page::mojom::PromoPtr MakePromo(const PromoData& data) {
  // |data.middle_slot_json| is safe to be decoded here. The JSON string is part
  // of a larger JSON initially decoded using the data decoder utility in the
  // PromoService to base::Value. The middle-slot promo part is then reencoded
  // from base::Value to a JSON string stored in |data.middle_slot_json|.
  auto middle_slot = base::JSONReader::Read(data.middle_slot_json);
  if (!middle_slot.has_value())
    return nullptr;

  base::Value::Dict& middle_slot_dict = middle_slot->GetDict();
  if (middle_slot_dict.FindBoolByDottedPath("hidden").value_or(false))
    return nullptr;

  auto promo = new_tab_page::mojom::Promo::New();
  promo->id = data.promo_id;
  auto* parts = middle_slot_dict.FindList("part");
  if (parts) {
    std::vector<new_tab_page::mojom::PromoPartPtr> mojom_parts;
    for (const base::Value& part : *parts) {
      const base::Value::Dict& part_dict = part.GetDict();
      if (part_dict.Find("image")) {
        auto mojom_image = new_tab_page::mojom::PromoImagePart::New();
        auto* image_url = part_dict.FindStringByDottedPath("image.image_url");
        if (!image_url || image_url->empty()) {
          continue;
        }
        mojom_image->image_url = GURL(*image_url);
        auto* target = part_dict.FindStringByDottedPath("image.target");
        if (target && !target->empty()) {
          mojom_image->target = GURL(*target);
        }
        mojom_parts.push_back(
            new_tab_page::mojom::PromoPart::NewImage(std::move(mojom_image)));
      } else if (part_dict.Find("link")) {
        auto mojom_link = new_tab_page::mojom::PromoLinkPart::New();
        auto* url = part_dict.FindStringByDottedPath("link.url");
        if (!url || url->empty()) {
          continue;
        }
        mojom_link->url = GURL(*url);
        auto* text = part_dict.FindStringByDottedPath("link.text");
        if (!text || text->empty()) {
          continue;
        }
        mojom_link->text = *text;
        mojom_parts.push_back(
            new_tab_page::mojom::PromoPart::NewLink(std::move(mojom_link)));
      } else if (part_dict.Find("text")) {
        auto mojom_text = new_tab_page::mojom::PromoTextPart::New();
        auto* text = part_dict.FindStringByDottedPath("text.text");
        if (!text || text->empty()) {
          continue;
        }
        mojom_text->text = *text;
        mojom_parts.push_back(
            new_tab_page::mojom::PromoPart::NewText(std::move(mojom_text)));
      }
    }
    promo->middle_slot_parts = std::move(mojom_parts);
  }
  promo->log_url = data.promo_log_url;
  return promo;
}

}  // namespace

// static
const char NewTabPageHandler::kModuleDismissedHistogram[] =
    "NewTabPage.Modules.Dismissed";
const char NewTabPageHandler::kModuleRestoredHistogram[] =
    "NewTabPage.Modules.Restored";

NewTabPageHandler::NewTabPageHandler(
    mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<new_tab_page::mojom::Page> pending_page,
    Profile* profile,
    NtpCustomBackgroundService* ntp_custom_background_service,
    ThemeService* theme_service,
    search_provider_logos::LogoService* logo_service,
    content::WebContents* web_contents,
    std::unique_ptr<CustomizeChromeFeaturePromoHelper>
        customize_chrome_feature_promo_helper,
    const base::Time& ntp_navigation_start_time,
    const std::vector<std::pair<const std::string, int>> module_id_names)
    : ntp_background_service_(
          NtpBackgroundServiceFactory::GetForProfile(profile)),
      ntp_custom_background_service_(ntp_custom_background_service),
      logo_service_(logo_service),
      theme_provider_(webui::GetThemeProvider(web_contents)),
      theme_service_(theme_service),
      profile_(profile),
      web_contents_(web_contents),
      customize_chrome_feature_promo_helper_(
          std::move(customize_chrome_feature_promo_helper)),
      ntp_navigation_start_time_(ntp_navigation_start_time),
      module_id_names_(module_id_names),
      logger_(profile,
              GURL(chrome::kChromeUINewTabPageURL),
              ntp_navigation_start_time),
      promo_service_(PromoServiceFactory::GetForProfile(profile)),
      page_{std::move(pending_page)},
      receiver_{this, std::move(pending_page_handler)} {
  CHECK(ntp_background_service_);
  CHECK(ntp_custom_background_service_);
  CHECK(logo_service_);
  CHECK(theme_service_);
  CHECK(promo_service_);
  CHECK(web_contents_);
  CHECK(customize_chrome_feature_promo_helper_);
  ntp_background_service_->AddObserver(this);
  native_theme_observation_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
  theme_service_observation_.Observe(theme_service_.get());
  ntp_custom_background_service_observation_.Observe(
      ntp_custom_background_service_.get());
  promo_service_observation_.Observe(promo_service_.get());
  OnThemeChanged();

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kNtpModulesVisible,
      base::BindRepeating(&NewTabPageHandler::UpdateDisabledModules,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kNtpDisabledModules,
      base::BindRepeating(&NewTabPageHandler::UpdateDisabledModules,
                          base::Unretained(this)));

  if (customize_chrome::IsSidePanelEnabled()) {
    auto* customize_chrome_tab_helper =
        CustomizeChromeTabHelper::FromWebContents(web_contents_);
    // Lifetime is tied to NewTabPageUI which owns the NewTabPageHandler.
    DCHECK(customize_chrome_tab_helper);
    page_->SetCustomizeChromeSidePanelVisibility(
        customize_chrome_tab_helper->IsCustomizeChromeEntryShowing());
    customize_chrome_tab_helper->SetCallback(base::BindRepeating(
        &NewTabPageHandler::NotifyCustomizeChromeSidePanelVisibilityChanged,
        weak_ptr_factory_.GetWeakPtr()));
  }
}

NewTabPageHandler::~NewTabPageHandler() {
  ntp_background_service_->RemoveObserver(this);
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

// static
void NewTabPageHandler::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kNtpDisabledModules);
  registry->RegisterListPref(prefs::kNtpModulesOrder);
  registry->RegisterBooleanPref(prefs::kNtpModulesVisible, true);
  registry->RegisterIntegerPref(prefs::kNtpModulesShownCount, 0);
  registry->RegisterTimePref(prefs::kNtpModulesFirstShownTime, base::Time());
  registry->RegisterBooleanPref(prefs::kNtpModulesFreVisible, true);
  registry->RegisterIntegerPref(prefs::kNtpCustomizeChromeButtonOpenCount, 0);
}

void NewTabPageHandler::SetMostVisitedSettings(bool custom_links_enabled,
                                               bool visible) {
  bool old_visible = IsShortcutsVisible();
  if (old_visible != visible) {
    profile_->GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, visible);
    logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_TOGGLE_VISIBILITY,
                     base::TimeDelta() /* unused */);
  }

  bool old_custom_links_enabled = IsCustomLinksEnabled();
  if (old_custom_links_enabled != custom_links_enabled) {
    profile_->GetPrefs()->SetBoolean(ntp_prefs::kNtpUseMostVisitedTiles,
                                     !custom_links_enabled);
    logger_.LogEvent(NTP_CUSTOMIZE_SHORTCUT_TOGGLE_TYPE,
                     base::TimeDelta() /* unused */);
  }
}

void NewTabPageHandler::GetMostVisitedSettings(
    GetMostVisitedSettingsCallback callback) {
  bool custom_links_enabled = IsCustomLinksEnabled();
  bool visible = IsShortcutsVisible();
  std::move(callback).Run(custom_links_enabled, visible);
}

void NewTabPageHandler::SetBackgroundImage(const std::string& attribution_1,
                                           const std::string& attribution_2,
                                           const GURL& attribution_url,
                                           const GURL& image_url,
                                           const GURL& thumbnail_url,
                                           const std::string& collection_id) {
  ntp_custom_background_service_->SetCustomBackgroundInfo(
      image_url, thumbnail_url, attribution_1, attribution_2, attribution_url,
      collection_id);
  LogEvent(NTP_BACKGROUND_IMAGE_SET);
}

void NewTabPageHandler::SetDailyRefreshCollectionId(
    const std::string& collection_id) {
  // Only populating the |collection_id| turns on refresh daily which overrides
  // the the selected image.
  ntp_custom_background_service_->SetCustomBackgroundInfo(
      /* image_url */ GURL(), /* thumbnail_url */ GURL(),
      /* attribution_line_1= */ "", /* attribution_line_2= */ "",
      /* action_url= */ GURL(), collection_id);
  LogEvent(NTP_BACKGROUND_DAILY_REFRESH_ENABLED);
}

void NewTabPageHandler::SetNoBackgroundImage() {
  ntp_custom_background_service_->SetCustomBackgroundInfo(
      /* image_url */ GURL(), /* thumbnail_url */ GURL(),
      /* attribution_line_1= */ "", /* attribution_line_2= */ "",
      /* action_url= */ GURL(), /* collection_id= */ "");
  LogEvent(NTP_BACKGROUND_IMAGE_RESET);
}

void NewTabPageHandler::RevertBackgroundChanges() {
  ntp_custom_background_service_->RevertBackgroundChanges();
}

void NewTabPageHandler::ConfirmBackgroundChanges() {
  ntp_custom_background_service_->ConfirmBackgroundChanges();
}

void NewTabPageHandler::GetBackgroundCollections(
    GetBackgroundCollectionsCallback callback) {
  if (!ntp_background_service_ || background_collections_callback_) {
    std::move(callback).Run(
        std::vector<new_tab_page::mojom::BackgroundCollectionPtr>());
    return;
  }
  background_collections_request_start_time_ = base::TimeTicks::Now();
  background_collections_callback_ = std::move(callback);
  ntp_background_service_->FetchCollectionInfo();
}

void NewTabPageHandler::GetBackgroundImages(
    const std::string& collection_id,
    GetBackgroundImagesCallback callback) {
  if (background_images_callback_) {
    std::move(background_images_callback_)
        .Run(std::vector<new_tab_page::mojom::CollectionImagePtr>());
  }
  if (!ntp_background_service_) {
    std::move(callback).Run(
        std::vector<new_tab_page::mojom::CollectionImagePtr>());
    return;
  }
  images_request_collection_id_ = collection_id;
  background_images_request_start_time_ = base::TimeTicks::Now();
  background_images_callback_ = std::move(callback);
  ntp_background_service_->FetchCollectionImageInfo(collection_id);
}

void NewTabPageHandler::GetDoodle(GetDoodleCallback callback) {
  search_provider_logos::LogoCallbacks callbacks;
  callbacks.on_cached_encoded_logo_available =
      base::BindOnce(&NewTabPageHandler::OnLogoAvailable,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  // This will trigger re-downloading the doodle and caching it. This means a
  // new doodle will be returned on subsequent NTP loads.
  logo_service_->GetLogo(std::move(callbacks), /*for_webui_ntp=*/true);
}

void NewTabPageHandler::ChooseLocalCustomBackground(
    ChooseLocalCustomBackgroundCallback callback) {
  // Early return if the select file dialog is already active.
  if (select_file_dialog_)
    return;

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents_));
  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.allowed_paths = ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH;
  file_types.extensions.resize(1);
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("jpg"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("jpeg"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("png"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("gif"));
  file_types.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_UPLOAD_IMAGE_FORMAT));
  choose_local_custom_background_callback_ = std::move(callback);
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, std::u16string(),
      profile_->last_selected_directory(), &file_types, 0,
      base::FilePath::StringType(), web_contents_->GetTopLevelNativeWindow(),
      nullptr);
}

void NewTabPageHandler::UpdatePromoData() {
  if (promo_service_->promo_data().has_value()) {
    OnPromoDataUpdated();
  }
  promo_load_start_time_ = base::TimeTicks::Now();
  promo_service_->Refresh();
}

void NewTabPageHandler::BlocklistPromo(const std::string& promo_id) {
  promo_service_->BlocklistPromo(promo_id);
}

void NewTabPageHandler::UndoBlocklistPromo(const std::string& promo_id) {
  promo_service_->UndoBlocklistPromo(promo_id);
}

void NewTabPageHandler::OnDismissModule(const std::string& module_id) {
  const std::string histogram_prefix(kModuleDismissedHistogram);
  base::UmaHistogramExactLinear(histogram_prefix, 1, 1);
  base::UmaHistogramExactLinear(histogram_prefix + "." + module_id, 1, 1);
}

void NewTabPageHandler::OnRestoreModule(const std::string& module_id) {
  const std::string histogram_prefix(kModuleRestoredHistogram);
  base::UmaHistogramExactLinear(histogram_prefix, 1, 1);
  base::UmaHistogramExactLinear(histogram_prefix + "." + module_id, 1, 1);
}

void NewTabPageHandler::SetModulesVisible(bool visible) {
  profile_->GetPrefs()->SetBoolean(prefs::kNtpModulesVisible, visible);
}

void NewTabPageHandler::SetModuleDisabled(const std::string& module_id,
                                          bool disabled) {
  ScopedListPrefUpdate update(profile_->GetPrefs(), prefs::kNtpDisabledModules);
  base::Value::List& list = update.Get();
  base::Value module_id_value(module_id);
  if (disabled) {
    if (!base::Contains(list, module_id_value))
      list.Append(std::move(module_id_value));
  } else {
    list.EraseValue(module_id_value);
  }
}

void NewTabPageHandler::UpdateDisabledModules() {
  std::vector<std::string> module_ids;
  // If the module visibility is managed by policy we either disable all modules
  // (if invisible) or no modules (if visible).
  if (!profile_->GetPrefs()->IsManagedPreference(prefs::kNtpModulesVisible)) {
    const auto& module_ids_value =
        profile_->GetPrefs()->GetList(prefs::kNtpDisabledModules);
    for (const auto& id : module_ids_value) {
      module_ids.push_back(id.GetString());
    }
  }
  page_->SetDisabledModules(
      !profile_->GetPrefs()->GetBoolean(prefs::kNtpModulesVisible),
      std::move(module_ids));
}

void NewTabPageHandler::OnModulesLoadedWithData(
    const std::vector<std::string>& module_ids) {
  std::vector<std::string> survey_eligible_module_ids =
      GetSurveyEligibleModuleIds();
  // If none of the loaded modules are eligible for HATS, return early.
  if (!std::any_of(module_ids.begin(), module_ids.end(),
                   [&survey_eligible_module_ids](std::string id) {
                     return base::Contains(survey_eligible_module_ids, id);
                   })) {
    return;
  }

  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_, /*create_if_necessary=*/true);
  CHECK(hats_service);
  hats_service->LaunchDelayedSurveyForWebContents(kHatsSurveyTriggerNtpModules,
                                                  web_contents_, 0);
}

void NewTabPageHandler::GetModulesIdNames(GetModulesIdNamesCallback callback) {
  std::vector<new_tab_page::mojom::ModuleIdNamePtr> modules_details;
  for (const auto& id_name_pair : module_id_names_) {
    auto module_id_name = new_tab_page::mojom::ModuleIdName::New();
    module_id_name->id = id_name_pair.first;
    module_id_name->name = l10n_util::GetStringUTF8(id_name_pair.second);
    modules_details.push_back(std::move(module_id_name));
  }

  std::move(callback).Run(std::move(modules_details));
}

void NewTabPageHandler::SetModulesOrder(
    const std::vector<std::string>& module_ids) {
  base::Value::List module_ids_value;
  for (const auto& module_id : module_ids) {
    module_ids_value.Append(module_id);
  }
  profile_->GetPrefs()->SetList(prefs::kNtpModulesOrder,
                                std::move(module_ids_value));
}

void NewTabPageHandler::GetModulesOrder(GetModulesOrderCallback callback) {
  std::vector<std::string> module_ids;

  // First, apply order as set by the last drag&drop interaction.
  if (base::FeatureList::IsEnabled(ntp_features::kNtpModulesDragAndDrop)) {
    const auto& module_ids_value =
        profile_->GetPrefs()->GetList(prefs::kNtpModulesOrder);
    for (const auto& id : module_ids_value) {
      module_ids.push_back(id.GetString());
    }
  }

  // Second, append Finch order for modules _not_ ordered by drag&drop.
  base::ranges::copy_if(ntp_features::GetModulesOrder(),
                        std::back_inserter(module_ids),
                        [&module_ids](const std::string& id) {
                          return !base::Contains(module_ids, id);
                        });

  std::move(callback).Run(std::move(module_ids));
}

void NewTabPageHandler::IncrementModulesShownCount() {
  const auto ntp_modules_shown_count =
      profile_->GetPrefs()->GetInteger(prefs::kNtpModulesShownCount);

  if (ntp_modules_shown_count == 0) {
    profile_->GetPrefs()->SetTime(prefs::kNtpModulesFirstShownTime,
                                  base::Time::Now());
  }
  profile_->GetPrefs()->SetInteger(prefs::kNtpModulesShownCount,
                                   ntp_modules_shown_count + 1);
}

void NewTabPageHandler::SetModulesFreVisible(bool visible) {
  profile_->GetPrefs()->SetBoolean(prefs::kNtpModulesFreVisible, visible);
  page_->SetModulesFreVisibility(visible);
}

void NewTabPageHandler::UpdateModulesFreVisibility() {
  const auto ntp_modules_shown_count =
      profile_->GetPrefs()->GetInteger(prefs::kNtpModulesShownCount);
  const auto ntp_modules_first_shown_time =
      profile_->GetPrefs()->GetTime(prefs::kNtpModulesFirstShownTime);
  auto ntp_modules_fre_visible =
      profile_->GetPrefs()->GetBoolean(prefs::kNtpModulesFreVisible);

  if (ntp_modules_fre_visible &&
      (ntp_modules_shown_count == kMaxModuleFreImpressions ||
       (!ntp_modules_first_shown_time.is_null() &&
        (base::Time::Now() - ntp_modules_first_shown_time) == base::Days(1)))) {
    LogModulesFreOptInStatus(new_tab_page::mojom::OptInStatus::kImplicitOptIn);
  }

  // Hide Modular NTP Desktop v1 First Run Experience after
  // |kMaxModuleFreImpressions| impressions or 1 day, whichever comes first.
  if (ntp_modules_shown_count >= kMaxModuleFreImpressions ||
      (!ntp_modules_first_shown_time.is_null() &&
       (base::Time::Now() - ntp_modules_first_shown_time) > base::Days(1))) {
    ntp_modules_fre_visible = false;
    SetModulesFreVisible(ntp_modules_fre_visible);
  } else {
    page_->SetModulesFreVisibility(ntp_modules_fre_visible);
  }
}

void NewTabPageHandler::LogModulesFreOptInStatus(
    new_tab_page::mojom::OptInStatus opt_in_status) {
  const auto ntp_modules_shown_count =
      profile_->GetPrefs()->GetInteger(prefs::kNtpModulesShownCount);
  switch (opt_in_status) {
    case new_tab_page::mojom::OptInStatus::kExplicitOptIn:
      base::UmaHistogramExactLinear("NewTabPage.Modules.FreExplicitOptIn",
                                    ntp_modules_shown_count,
                                    kMaxModuleFreImpressions);
      break;
    case new_tab_page::mojom::OptInStatus::kImplicitOptIn:
      base::UmaHistogramBoolean("NewTabPage.Modules.FreImplicitOptIn", true);
      break;

    case new_tab_page::mojom::OptInStatus::kOptOut:
      base::UmaHistogramExactLinear("NewTabPage.Modules.FreOptOut",
                                    ntp_modules_shown_count,
                                    kMaxModuleFreImpressions);
  }
}

void NewTabPageHandler::SetCustomizeChromeSidePanelVisible(
    bool visible,
    new_tab_page::mojom::CustomizeChromeSection section) {
  CustomizeChromeSection section_enum;
  switch (section) {
    case new_tab_page::mojom::CustomizeChromeSection::kUnspecified:
      section_enum = CustomizeChromeSection::kUnspecified;
      break;
    case new_tab_page::mojom::CustomizeChromeSection::kAppearance:
      section_enum = CustomizeChromeSection::kAppearance;
      break;
    case new_tab_page::mojom::CustomizeChromeSection::kShortcuts:
      section_enum = CustomizeChromeSection::kShortcuts;
      break;
    case new_tab_page::mojom::CustomizeChromeSection::kModules:
      section_enum = CustomizeChromeSection::kModules;
      break;
  }
  auto* customize_chrome_tab_helper =
      CustomizeChromeTabHelper::FromWebContents(web_contents_);
  customize_chrome_tab_helper->SetCustomizeChromeSidePanelVisible(visible,
                                                                  section_enum);

  if (visible) {
    // Record usage for customize chrome promo.
    auto* tab = web_contents_.get();
    customize_chrome_feature_promo_helper_->RecordCustomizeChromeFeatureUsage(
        tab);
    customize_chrome_feature_promo_helper_->CloseCustomizeChromeFeaturePromo(
        tab);
  }
}

void NewTabPageHandler::IncrementCustomizeChromeButtonOpenCount() {
  CHECK(profile_);
  CHECK(profile_->GetPrefs());
  profile_->GetPrefs()->SetInteger(
      prefs::kNtpCustomizeChromeButtonOpenCount,
      profile_->GetPrefs()->GetInteger(
          prefs::kNtpCustomizeChromeButtonOpenCount) +
          1);
}

void NewTabPageHandler::MaybeShowCustomizeChromeFeaturePromo() {
  CHECK(profile_);
  CHECK(profile_->GetPrefs());
  const auto customize_chrome_button_open_count =
      profile_->GetPrefs()->GetInteger(
          prefs::kNtpCustomizeChromeButtonOpenCount);
  if (customize_chrome_button_open_count == 0) {
    customize_chrome_feature_promo_helper_
        ->MaybeShowCustomizeChromeFeaturePromo(web_contents_.get());
  }
}

void NewTabPageHandler::OnPromoDataUpdated() {
  if (promo_load_start_time_.has_value()) {
    base::TimeDelta duration = base::TimeTicks::Now() - *promo_load_start_time_;
    UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.Promos.RequestLatency2", duration);
    if (promo_service_->promo_status() == PromoService::Status::OK_WITH_PROMO) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "NewTabPage.Promos.RequestLatency2.SuccessWithPromo", duration);
    } else if (promo_service_->promo_status() ==
               PromoService::Status::OK_BUT_BLOCKED) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "NewTabPage.Promos.RequestLatency2.SuccessButBlocked", duration);
    } else if (promo_service_->promo_status() ==
               PromoService::Status::OK_WITHOUT_PROMO) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "NewTabPage.Promos.RequestLatency2.SuccessWithoutPromo", duration);
    } else {
      DCHECK(promo_service_->promo_status() !=
             PromoService::Status::NOT_UPDATED);
      UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.Promos.RequestLatency2.Failure",
                                 duration);
    }
    promo_load_start_time_ = absl::nullopt;
  }

  const auto& data = promo_service_->promo_data();
  if (data.has_value() &&
      promo_service_->promo_status() != PromoService::Status::OK_BUT_BLOCKED) {
    page_->SetPromo(MakePromo(data.value()));
  } else {
    page_->SetPromo(nullptr);
  }
}

void NewTabPageHandler::OnPromoServiceShuttingDown() {
  promo_service_observation_.Reset();
  promo_service_ = nullptr;
}

void NewTabPageHandler::OnAppRendered(double time) {
  logger_.LogEvent(NTP_APP_RENDERED,
                   base::Time::FromJsTime(time) - ntp_navigation_start_time_);
}

void NewTabPageHandler::OnOneGoogleBarRendered(double time) {
  logger_.LogEvent(NTP_ONE_GOOGLE_BAR_SHOWN,
                   base::Time::FromJsTime(time) - ntp_navigation_start_time_);
}

void NewTabPageHandler::OnPromoRendered(double time,
                                        const absl::optional<GURL>& log_url) {
  logger_.LogEvent(NTP_MIDDLE_SLOT_PROMO_SHOWN,
                   base::Time::FromJsTime(time) - ntp_navigation_start_time_);
  if (log_url.has_value() && log_url->is_valid()) {
    Fetch(*log_url, base::BindOnce([](bool, std::unique_ptr<std::string>) {}));
  }
}

void NewTabPageHandler::OnCustomizeDialogAction(
    new_tab_page::mojom::CustomizeDialogAction action) {
  NTPLoggingEventType event;
  switch (action) {
    case new_tab_page::mojom::CustomizeDialogAction::kCancelClicked:
      event = NTP_CUSTOMIZATION_MENU_CANCEL;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::kDoneClicked:
      event = NTP_CUSTOMIZATION_MENU_DONE;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::kOpenClicked:
      event = NTP_CUSTOMIZATION_MENU_OPENED;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::kBackgroundsBackClicked:
      event = NTP_BACKGROUND_BACK_CLICK;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kBackgroundsNoBackgroundSelected:
      event = NTP_BACKGROUND_DEFAULT_SELECTED;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kBackgroundsCollectionOpened:
      event = NTP_BACKGROUND_OPEN_COLLECTION;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kBackgroundsRefreshToggleClicked:
      event = NTP_BACKGROUND_REFRESH_TOGGLE_CLICKED;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::kBackgroundsImageSelected:
      event = NTP_BACKGROUND_SELECT_IMAGE;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kBackgroundsUploadFromDeviceClicked:
      event = NTP_BACKGROUND_UPLOAD_FROM_DEVICE;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kShortcutsCustomLinksClicked:
      event = NTP_CUSTOMIZE_SHORTCUT_CUSTOM_LINKS_CLICKED;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kShortcutsMostVisitedClicked:
      event = NTP_CUSTOMIZE_SHORTCUT_MOST_VISITED_CLICKED;
      break;
    case new_tab_page::mojom::CustomizeDialogAction::
        kShortcutsVisibilityToggleClicked:
      event = NTP_CUSTOMIZE_SHORTCUT_VISIBILITY_TOGGLE_CLICKED;
      break;
    default:
      NOTREACHED();
  }
  LogEvent(event);
}

void NewTabPageHandler::OnDoodleImageClicked(
    new_tab_page::mojom::DoodleImageType type,
    const absl::optional<::GURL>& log_url) {
  NTPLoggingEventType event;
  switch (type) {
    case new_tab_page::mojom::DoodleImageType::kAnimation:
      event = NTP_ANIMATED_LOGO_CLICKED;
      break;
    case new_tab_page::mojom::DoodleImageType::kCta:
      event = NTP_CTA_LOGO_CLICKED;
      break;
    case new_tab_page::mojom::DoodleImageType::kStatic:
      event = NTP_STATIC_LOGO_CLICKED;
      break;
    default:
      NOTREACHED();
      return;
  }
  LogEvent(event);

  if (type == new_tab_page::mojom::DoodleImageType::kCta &&
      log_url.has_value()) {
    // We just ping the server to indicate a CTA image has been clicked.
    Fetch(*log_url, base::BindOnce([](bool, std::unique_ptr<std::string>) {}));
  }
}

void NewTabPageHandler::OnDoodleImageRendered(
    new_tab_page::mojom::DoodleImageType type,
    double time,
    const GURL& log_url,
    OnDoodleImageRenderedCallback callback) {
  if (type == new_tab_page::mojom::DoodleImageType::kCta ||
      type == new_tab_page::mojom::DoodleImageType::kStatic) {
    logger_.LogEvent(type == new_tab_page::mojom::DoodleImageType::kCta
                         ? NTP_CTA_LOGO_SHOWN_FROM_CACHE
                         : NTP_STATIC_LOGO_SHOWN_FROM_CACHE,
                     base::Time::FromJsTime(time) - ntp_navigation_start_time_);
  }
  Fetch(log_url,
        base::BindOnce(&NewTabPageHandler::OnLogFetchResult,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void NewTabPageHandler::OnDoodleShared(
    new_tab_page::mojom::DoodleShareChannel channel,
    const std::string& doodle_id,
    const absl::optional<std::string>& share_id) {
  int channel_id;
  switch (channel) {
    case new_tab_page::mojom::DoodleShareChannel::kFacebook:
      channel_id = 2;
      break;
    case new_tab_page::mojom::DoodleShareChannel::kTwitter:
      channel_id = 3;
      break;
    case new_tab_page::mojom::DoodleShareChannel::kEmail:
      channel_id = 5;
      break;
    case new_tab_page::mojom::DoodleShareChannel::kLinkCopy:
      channel_id = 6;
      break;
    default:
      NOTREACHED();
      break;
  }
  std::string query =
      base::StringPrintf("gen_204?atype=i&ct=doodle&ntp=2&cad=sh,%d,ct:%s",
                         channel_id, doodle_id.c_str());
  if (share_id.has_value()) {
    query += "&ei=" + *share_id;
  }
  auto url = GURL(TemplateURLServiceFactory::GetForProfile(profile_)
                      ->search_terms_data()
                      .GoogleBaseURLValue())
                 .Resolve(query);
  // We just ping the server to indicate a doodle has been shared.
  Fetch(url, base::BindOnce([](bool s, std::unique_ptr<std::string>) {}));
}

void NewTabPageHandler::OnPromoLinkClicked() {
  LogEvent(NTP_MIDDLE_SLOT_PROMO_LINK_CLICKED);
}

void NewTabPageHandler::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  OnThemeChanged();
}

void NewTabPageHandler::OnThemeChanged() {
  page_->SetTheme(MakeTheme(web_contents_->GetColorProvider(), theme_provider_,
                            theme_service_, ntp_custom_background_service_,
                            web_contents_));
}

void NewTabPageHandler::OnCustomBackgroundImageUpdated() {
  OnThemeChanged();
}

void NewTabPageHandler::OnNtpCustomBackgroundServiceShuttingDown() {
  ntp_custom_background_service_observation_.Reset();
  ntp_custom_background_service_ = nullptr;
}

void NewTabPageHandler::OnCollectionInfoAvailable() {
  if (!background_collections_callback_) {
    return;
  }

  base::TimeDelta duration =
      base::TimeTicks::Now() - background_collections_request_start_time_;
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "NewTabPage.BackgroundService.Collections.RequestLatency", duration);
  // Any response where no collections are returned is considered a failure.
  if (ntp_background_service_->collection_info().empty()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Collections.RequestLatency.Failure",
        duration);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Collections.RequestLatency.Success",
        duration);
  }

  std::vector<new_tab_page::mojom::BackgroundCollectionPtr> collections;
  for (const auto& info : ntp_background_service_->collection_info()) {
    auto collection = new_tab_page::mojom::BackgroundCollection::New();
    collection->id = info.collection_id;
    collection->label = info.collection_name;
    collection->preview_image_url = GURL(info.preview_image_url);
    collections.push_back(std::move(collection));
  }
  std::move(background_collections_callback_).Run(std::move(collections));
}

void NewTabPageHandler::OnCollectionImagesAvailable() {
  if (!background_images_callback_) {
    return;
  }

  base::TimeDelta duration =
      base::TimeTicks::Now() - background_images_request_start_time_;
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "NewTabPage.BackgroundService.Images.RequestLatency", duration);
  // Any response where no images are returned is considered a failure.
  if (ntp_background_service_->collection_images().empty()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Images.RequestLatency.Failure", duration);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Images.RequestLatency.Success", duration);
  }

  std::vector<new_tab_page::mojom::CollectionImagePtr> images;
  if (ntp_background_service_->collection_images().empty()) {
    std::move(background_images_callback_).Run(std::move(images));
    return;
  }
  auto collection_id =
      ntp_background_service_->collection_images()[0].collection_id;
  for (const auto& info : ntp_background_service_->collection_images()) {
    DCHECK(info.collection_id == collection_id);
    auto image = new_tab_page::mojom::CollectionImage::New();
    image->attribution_1 = !info.attribution.empty() ? info.attribution[0] : "";
    image->attribution_2 =
        info.attribution.size() > 1 ? info.attribution[1] : "";
    image->attribution_url = info.attribution_action_url;
    image->image_url = info.image_url;
    image->preview_image_url = info.thumbnail_image_url;
    image->collection_id = collection_id;
    images.push_back(std::move(image));
  }
  std::move(background_images_callback_).Run(std::move(images));
}

void NewTabPageHandler::OnNextCollectionImageAvailable() {}

void NewTabPageHandler::OnNtpBackgroundServiceShuttingDown() {
  ntp_background_service_->RemoveObserver(this);
  ntp_background_service_ = nullptr;
}

void NewTabPageHandler::FileSelected(const base::FilePath& path,
                                     int index,
                                     void* params) {
  DCHECK(choose_local_custom_background_callback_);
  if (ntp_custom_background_service_) {
    profile_->set_last_selected_directory(path.DirName());
    ntp_custom_background_service_->SelectLocalBackgroundImage(path);
  }

  select_file_dialog_ = nullptr;
  // File selection can happen at any time after NTP load, and is not logged
  // with the event.
  LogEvent(NTP_CUSTOMIZE_LOCAL_IMAGE_DONE);
  LogEvent(NTP_BACKGROUND_UPLOAD_DONE);

  if (choose_local_custom_background_callback_)
    std::move(choose_local_custom_background_callback_).Run(true);
}

void NewTabPageHandler::FileSelectionCanceled(void* params) {
  DCHECK(choose_local_custom_background_callback_);
  select_file_dialog_ = nullptr;
  // File selection can happen at any time after NTP load, and is not logged
  // with the event.
  LogEvent(NTP_CUSTOMIZE_LOCAL_IMAGE_CANCEL);
  LogEvent(NTP_BACKGROUND_UPLOAD_CANCEL);
  if (choose_local_custom_background_callback_)
    std::move(choose_local_custom_background_callback_).Run(false);
}

void NewTabPageHandler::OnLogoAvailable(
    GetDoodleCallback callback,
    search_provider_logos::LogoCallbackReason type,
    const absl::optional<search_provider_logos::EncodedLogo>& logo) {
  if (!logo) {
    std::move(callback).Run(nullptr);
    return;
  }
  auto doodle = new_tab_page::mojom::Doodle::New();
  if (logo->metadata.type == search_provider_logos::LogoType::SIMPLE ||
      logo->metadata.type == search_provider_logos::LogoType::ANIMATED) {
    if (!logo->encoded_image) {
      std::move(callback).Run(nullptr);
      return;
    }
    auto image_doodle = new_tab_page::mojom::AllModeImageDoodle::New();
    image_doodle->light = MakeImageDoodle(
        logo->metadata.type, logo->encoded_image->data(),
        logo->metadata.mime_type, logo->metadata.animated_url,
        logo->metadata.width_px, logo->metadata.height_px, "#ffffff",
        logo->metadata.share_button_x, logo->metadata.share_button_y,
        logo->metadata.share_button_icon, logo->metadata.share_button_bg,
        logo->metadata.share_button_opacity, logo->metadata.log_url,
        logo->metadata.cta_log_url);
    if (logo->dark_encoded_image) {
      image_doodle->dark = MakeImageDoodle(
          logo->metadata.type, logo->dark_encoded_image->data(),
          logo->metadata.dark_mime_type, logo->metadata.dark_animated_url,
          logo->metadata.dark_width_px, logo->metadata.dark_height_px,
          logo->metadata.dark_background_color,
          logo->metadata.dark_share_button_x,
          logo->metadata.dark_share_button_y,
          logo->metadata.dark_share_button_icon,
          logo->metadata.dark_share_button_bg,
          logo->metadata.dark_share_button_opacity, logo->metadata.dark_log_url,
          logo->metadata.dark_cta_log_url);
    }
    if (logo->metadata.on_click_url.is_valid()) {
      image_doodle->on_click_url = logo->metadata.on_click_url;
    }
    image_doodle->share_url = logo->metadata.short_link;
    doodle->image = std::move(image_doodle);
  } else if (logo->metadata.type ==
             search_provider_logos::LogoType::INTERACTIVE) {
    auto interactive_doodle = new_tab_page::mojom::InteractiveDoodle::New();
    interactive_doodle->url = logo->metadata.full_page_url;
    interactive_doodle->width = logo->metadata.iframe_width_px;
    interactive_doodle->height = logo->metadata.iframe_height_px;
    doodle->interactive = std::move(interactive_doodle);
  } else {
    std::move(callback).Run(nullptr);
    return;
  }
  doodle->description = logo->metadata.alt_text;
  std::move(callback).Run(std::move(doodle));
}

void NewTabPageHandler::LogEvent(NTPLoggingEventType event) {
  logger_.LogEvent(event, base::TimeDelta() /* unused */);
}

void NewTabPageHandler::Fetch(const GURL& url,
                              OnFetchResultCallback on_result) {
  auto traffic_annotation =
      net::DefineNetworkTrafficAnnotation("new_tab_page_handler", R"(
        semantics {
          sender: "New Tab Page"
          description: "Logs impression and interaction with doodle or promo."
          trigger:
            "Showing or clicking on the doodle or promo on the New Tab Page. "
            "Desktop only."
          data:
            "String identifiying todays doodle or promo and token identifying "
            "a single interaction session. Data does not contain PII."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control this feature via selecting a non-Google default "
            "search engine in Chrome settings under 'Search Engine'."
          chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
          }
        })");
  auto url_loader_factory = profile_->GetURLLoaderFactory();
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  auto loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  loader->DownloadToString(url_loader_factory.get(),
                           base::BindOnce(&NewTabPageHandler::OnFetchResult,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          loader.get(), std::move(on_result)),
                           kMaxDownloadBytes);
  loader_map_.insert({loader.get(), std::move(loader)});
}

void NewTabPageHandler::OnFetchResult(const network::SimpleURLLoader* loader,
                                      OnFetchResultCallback on_result,
                                      std::unique_ptr<std::string> body) {
  bool success = loader->NetError() == net::OK && loader->ResponseInfo() &&
                 loader->ResponseInfo()->headers &&
                 loader->ResponseInfo()->headers->response_code() >= 200 &&
                 loader->ResponseInfo()->headers->response_code() <= 299 &&
                 body;
  std::move(on_result).Run(success, std::move(body));
  loader_map_.erase(loader);
}

void NewTabPageHandler::OnLogFetchResult(OnDoodleImageRenderedCallback callback,
                                         bool success,
                                         std::unique_ptr<std::string> body) {
  if (!success || body->size() < 4 || body->substr(0, 4) != ")]}'") {
    std::move(callback).Run("", absl::nullopt, "");
    return;
  }
  auto value = base::JSONReader::Read(body->substr(4));
  if (!value.has_value()) {
    std::move(callback).Run("", absl::nullopt, "");
    return;
  }

  base::Value::Dict& dict = value->GetDict();
  auto* target_url_params_value =
      dict.FindStringByDottedPath("ddllog.target_url_params");
  auto target_url_params =
      target_url_params_value ? *target_url_params_value : "";
  auto* interaction_log_url_value =
      dict.FindStringByDottedPath("ddllog.interaction_log_url");
  auto interaction_log_url =
      interaction_log_url_value
          ? absl::optional<GURL>(
                GURL(TemplateURLServiceFactory::GetForProfile(profile_)
                         ->search_terms_data()
                         .GoogleBaseURLValue())
                    .Resolve(*interaction_log_url_value))
          : absl::nullopt;
  auto* encoded_ei_value = dict.FindStringByDottedPath("ddllog.encoded_ei");
  auto encoded_ei = encoded_ei_value ? *encoded_ei_value : "";
  std::move(callback).Run(target_url_params, interaction_log_url, encoded_ei);
}

bool NewTabPageHandler::IsCustomLinksEnabled() const {
  return !profile_->GetPrefs()->GetBoolean(ntp_prefs::kNtpUseMostVisitedTiles);
}

bool NewTabPageHandler::IsShortcutsVisible() const {
  return profile_->GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible);
}

void NewTabPageHandler::NotifyCustomizeChromeSidePanelVisibilityChanged(
    bool is_open) {
  page_->SetCustomizeChromeSidePanelVisibility(is_open);
}
