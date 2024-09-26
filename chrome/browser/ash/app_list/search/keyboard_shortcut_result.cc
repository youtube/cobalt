// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/keyboard_shortcut_result.h"

#include <string>

#include "ash/accelerators/keyboard_code_util.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/shortcut_viewer/keyboard_shortcut_viewer_metadata.h"
#include "ash/shortcut_viewer/strings/grit/shortcut_viewer_strings.h"
#include "base/i18n/rtl.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/common/search_result_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "chromeos/ash/components/string_matching/tokenized_string_match.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/paint_vector_icon.h"

namespace app_list {

namespace {

using ::ash::string_matching::TokenizedString;
using ::ash::string_matching::TokenizedStringMatch;
using TextVector = ChromeSearchResult::TextVector;
using IconCode = ::ash::SearchResultTextItem::IconCode;
using ::ui::KeyboardCode;

constexpr char kKeyboardShortcutScheme[] = "keyboard_shortcut://";

}  // namespace

absl::optional<IconCode> KeyboardShortcutResult::GetIconCodeFromKeyboardCode(
    KeyboardCode keyboard_code) {
  switch (keyboard_code) {
    case (KeyboardCode::VKEY_BROWSER_BACK):
      return IconCode::kKeyboardShortcutBrowserBack;
    case (KeyboardCode::VKEY_BROWSER_FORWARD):
      return IconCode::kKeyboardShortcutBrowserForward;
    case (KeyboardCode::VKEY_BROWSER_REFRESH):
      return IconCode::kKeyboardShortcutBrowserRefresh;
    case (KeyboardCode::VKEY_ZOOM):
      return IconCode::kKeyboardShortcutZoom;
    case (KeyboardCode::VKEY_MEDIA_LAUNCH_APP1):
      return IconCode::kKeyboardShortcutMediaLaunchApp1;
    case (KeyboardCode::VKEY_BRIGHTNESS_DOWN):
      return IconCode::kKeyboardShortcutBrightnessDown;
    case (KeyboardCode::VKEY_BRIGHTNESS_UP):
      return IconCode::kKeyboardShortcutBrightnessUp;
    case (KeyboardCode::VKEY_VOLUME_MUTE):
      return IconCode::kKeyboardShortcutVolumeMute;
    case (KeyboardCode::VKEY_VOLUME_DOWN):
      return IconCode::kKeyboardShortcutVolumeDown;
    case (KeyboardCode::VKEY_VOLUME_UP):
      return IconCode::kKeyboardShortcutVolumeUp;
    case (KeyboardCode::VKEY_UP):
      return IconCode::kKeyboardShortcutUp;
    case (KeyboardCode::VKEY_DOWN):
      return IconCode::kKeyboardShortcutDown;
    case (KeyboardCode::VKEY_LEFT):
      return IconCode::kKeyboardShortcutLeft;
    case (KeyboardCode::VKEY_RIGHT):
      return IconCode::kKeyboardShortcutRight;
    case (KeyboardCode::VKEY_PRIVACY_SCREEN_TOGGLE):
      return IconCode::kKeyboardShortcutPrivacyScreenToggle;
    case (KeyboardCode::VKEY_SNAPSHOT):
      return IconCode::kKeyboardShortcutSnapshot;
    default:
      return absl::nullopt;
  }
}

TextVector KeyboardShortcutResult::CreateTextVectorFromTemplateString(
    const std::u16string& template_string,
    const std::vector<std::u16string>& replacement_strings,
    const std::vector<KeyboardCode>& shortcut_key_codes) {
  // Placeholders ($i) in the template string should have values in [1, 9].
  DCHECK_LE(replacement_strings.size(), 9U);
  DCHECK_EQ(replacement_strings.size(), shortcut_key_codes.size());

  auto pieces = base::SplitString(template_string, u"$", base::KEEP_WHITESPACE,
                                  base::SPLIT_WANT_NONEMPTY);
  DCHECK_GT(pieces.size(), 0u);
  TextVector text_vector;

  const bool starts_with_placeholder = template_string[0] == '$';
  if (!starts_with_placeholder) {
    text_vector.push_back(CreateStringTextItem(pieces[0]));
  }

  for (size_t i = starts_with_placeholder ? 0 : 1; i < pieces.size(); ++i) {
    const std::u16string piece = pieces[i];
    DCHECK(!piece.empty());
    if (piece[0] < '1' || piece[0] > '9') {
      DLOG(ERROR) << "Invalid placeholder: $" << piece
                  << " in template string: " << template_string;
      continue;
    }

    const size_t index = static_cast<size_t>(piece[0] - '1');
    if (index >= replacement_strings.size()) {
      DLOG(ERROR) << "Placeholder $" << index
                  << " number exceeds number of replacement components.";
      continue;
    }

    // Handle placeholder content.
    if (!replacement_strings[index].compare(u"+ ")) {
      // Placeholder special case:
      // The delimiter "+ " is neither an icon nor iconified text.
      text_vector.push_back(CreateStringTextItem(u" + "));
    } else {
      const absl::optional<IconCode> icon_code =
          GetIconCodeFromKeyboardCode(shortcut_key_codes[index]);
      if (icon_code) {
        // Placeholder general case 1:
        // The KeyboardCode has a corresponding IconCode, and therefore an
        // icon image is supported by the front-end.
        text_vector.push_back(CreateIconCodeTextItem(icon_code.value()));
      } else {
        // Placeholder general case 2:
        // KeyboardCode does not have a corresponding IconCode. The
        // replacement text will be styled to look like an icon ("iconified
        // text").
        text_vector.push_back(
            CreateIconifiedTextTextItem(replacement_strings[index]));
      }
    }

    // Handle any plain-text content following the placeholder.
    if (piece.size() > 1) {
      text_vector.push_back(CreateStringTextItem(piece.substr(1)));
    }
  }
  return text_vector;
}

KeyboardShortcutResult::KeyboardShortcutResult(Profile* profile,
                                               const KeyboardShortcutData& data,
                                               double relevance)
    : profile_(profile) {
  set_id(base::StrCat({kKeyboardShortcutScheme,
                       base::NumberToString(data.description_message_id)}));
  set_relevance(relevance);
  SetTitle(data.description);
  SetResultType(ResultType::kKeyboardShortcut);
  SetMetricsType(ash::KEYBOARD_SHORTCUT);
  SetDisplayType(DisplayType::kList);
  SetCategory(Category::kHelp);
  UpdateIcon();

  // Set the details to the display name of the Keyboard Shortcut Viewer app.
  std::u16string sanitized_name = base::CollapseWhitespace(
      l10n_util::GetStringUTF16(IDS_INTERNAL_APP_KEYBOARD_SHORTCUT_VIEWER),
      true);
  base::i18n::SanitizeUserSuppliedString(&sanitized_name);
  SetDetails(sanitized_name);

  // Process |data.keyboard_shortcut_codes| to create:
  //   1. A vector of information for the KSV text.
  //   2. The accessible name.

  std::vector<std::u16string> replacement_strings;
  std::vector<std::u16string> accessible_names;
  const size_t shortcut_key_codes_size = data.shortcut_key_codes.size();
  replacement_strings.reserve(shortcut_key_codes_size);
  accessible_names.reserve(shortcut_key_codes_size);
  bool has_invalid_dom_key = false;

  for (ui::KeyboardCode key_code : data.shortcut_key_codes) {
    // Get the string for the |DomKey|.
    std::u16string dom_key_string = ash::GetStringForKeyboardCode(key_code);

    // See ash/shortcut_viewer/views/keyboard_shortcut_item_view.cc for details
    // on why this is necessary.
    const bool dont_remap_position =
        data.description_message_id == IDS_KSV_DESCRIPTION_IDC_ZOOM_PLUS ||
        data.description_message_id == IDS_KSV_DESCRIPTION_IDC_ZOOM_MINUS;
    if (dont_remap_position) {
      dom_key_string = ash::GetStringForKeyboardCode(
          key_code, /*remap_positional_key=*/false);
    }

    // If the |key_code| has no mapped |dom_key_string|, we use an alternative
    // string to indicate that the shortcut is not supported by current keyboard
    // layout.
    if (dom_key_string.empty()) {
      replacement_strings.clear();
      accessible_names.clear();
      has_invalid_dom_key = true;
      break;
    }

    std::u16string accessible_name =
        keyboard_shortcut_viewer::GetAccessibleNameForKeyboardCode(key_code);
    accessible_names.push_back(accessible_name.empty() ? dom_key_string
                                                       : accessible_name);
    replacement_strings.push_back(std::move(dom_key_string));
  }

  int shortcut_message_id;
  if (has_invalid_dom_key) {
    // |shortcut_message_id| should never be used if the shortcut is not
    // supported on the current keyboard layout.
    shortcut_message_id = -1;
  } else if (data.shortcut_message_id) {
    shortcut_message_id = *data.shortcut_message_id;
  } else {
    // Automatically determine the shortcut message based on the number of
    // replacement strings.
    // As there are separators inserted between the modifiers, a shortcut with
    // N modifiers has 2*N + 1 replacement strings.
    switch (replacement_strings.size()) {
      case 1:
        shortcut_message_id = IDS_KSV_SHORTCUT_ONE_KEY;
        break;
      case 3:
        shortcut_message_id = IDS_KSV_SHORTCUT_ONE_MODIFIER_ONE_KEY;
        break;
      case 5:
        shortcut_message_id = IDS_KSV_SHORTCUT_TWO_MODIFIERS_ONE_KEY;
        break;
      case 7:
        shortcut_message_id = IDS_KSV_SHORTCUT_THREE_MODIFIERS_ONE_KEY;
        break;
      default:
        NOTREACHED() << "Automatically determined shortcut has "
                     << replacement_strings.size() << " replacement strings.";
    }
  }

  std::u16string template_string;
  template_string = l10n_util::GetStringUTF16(
      has_invalid_dom_key ? IDS_KSV_KEY_NO_MAPPING : shortcut_message_id);

  std::u16string accessible_string;
  TextVector text_vector;

  if (replacement_strings.empty()) {
    accessible_string = template_string;
    text_vector.push_back(CreateStringTextItem(template_string));
  } else {
    accessible_string = l10n_util::GetStringFUTF16(
        shortcut_message_id, accessible_names, /*offsets=*/nullptr);
    text_vector = CreateTextVectorFromTemplateString(
        template_string, replacement_strings, data.shortcut_key_codes);
  }

  SetAccessibleName(data.description + u", " + details() + u", " +
                    accessible_string);
  SetKeyboardShortcutTextVector(text_vector);
}

KeyboardShortcutResult::~KeyboardShortcutResult() = default;

void KeyboardShortcutResult::Open(int event_flags) {
  if (ash::features::ShouldOnlyShowNewShortcutApp()) {
    chrome::ShowShortcutCustomizationApp(profile_);
    return;
  }
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);
  proxy->Launch(ash::kInternalAppIdKeyboardShortcutViewer, event_flags,
                apps::LaunchSource::kFromAppListQuery, nullptr);
}

double KeyboardShortcutResult::CalculateRelevance(
    const TokenizedString& query_tokenized,
    const std::u16string& target) {
  const TokenizedString target_tokenized(target, TokenizedString::Mode::kWords);

  const bool use_default_relevance =
      query_tokenized.text().empty() || target_tokenized.text().empty();

  if (use_default_relevance) {
    static constexpr double kDefaultRelevance = 0.0;
    return kDefaultRelevance;
  }

  TokenizedStringMatch match;
  return match.Calculate(query_tokenized, target_tokenized);
}

void KeyboardShortcutResult::UpdateIcon() {
  gfx::ImageSkia icon = gfx::CreateVectorIcon(
      chromeos::kKeyboardShortcutsIcon, kAppIconDimension, SK_ColorTRANSPARENT);
  SetIcon(IconInfo(icon, kAppIconDimension));
}

}  // namespace app_list
