// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/promo_card.h"

#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/service/sync_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace password_manager {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PromoCardType {
  // Password Checkup promo bubble.
  kCheckup = 0,
  // Password on the web promo bubble.
  kWebPasswordManager = 1,
  // Add shortcut promo bubble.
  kAddShortcut = 2,
  // Access passwords on iOS/Android promo bubble.
  kAccessOnAnyDevice = 3,
  kMaxValue = kAccessOnAnyDevice,
};

constexpr base::TimeDelta kPasswordCheckupPromoPeriod = base::Days(7);
constexpr int kPromoDisplayLimit = 3;

constexpr char kIdKey[] = "id";
constexpr char kLastTimeShownKey[] = "last_time_shown";
constexpr char kNumberOfTimesShownKey[] = "number_of_times_shown";
constexpr char kWasDismissedKey[] = "was_dismissed";

constexpr char kCheckupPromoId[] = "password_checkup_promo";
constexpr char kWebPasswordManagerPromoId[] = "passwords_on_web_promo";
constexpr char kShortcutPromoId[] = "password_shortcut_promo";
constexpr char kAccessOnAnyDevicePromoId[] = "access_on_any_device_promo";

extern const char16_t kGetStartedOnAndroid[] =
    u"https://support.google.com/chrome/?p=gpm_desktop_promo_android";
extern const char16_t kGetStartedOnIOS[] =
    u"https://support.google.com/chrome/?p=gpm_desktop_promo_ios";

// Creates new pref entry for the promo card with a given id.
base::Value::Dict CreatePromoCardPrefEntry(const std::string& id) {
  base::Value::Dict promo_card_pref_entry;
  promo_card_pref_entry.Set(kIdKey, id);
  promo_card_pref_entry.Set(kLastTimeShownKey, base::TimeToValue(base::Time()));
  promo_card_pref_entry.Set(kNumberOfTimesShownKey, 0);
  promo_card_pref_entry.Set(kWasDismissedKey, false);
  return promo_card_pref_entry;
}

PromoCardType ConvertIdToType(const std::string& promo_id) {
  if (promo_id == kCheckupPromoId) {
    return PromoCardType::kCheckup;
  } else if (promo_id == kWebPasswordManagerPromoId) {
    return PromoCardType::kWebPasswordManager;
  } else if (promo_id == kShortcutPromoId) {
    return PromoCardType::kAddShortcut;
  } else if (promo_id == kAccessOnAnyDevicePromoId) {
    return PromoCardType::kAccessOnAnyDevice;
  }
  NOTREACHED_NORETURN();
}

}  // namespace

// static
std::vector<std::unique_ptr<PromoCardInterface>>
PromoCardInterface::GetAllPromoCardsForProfile(Profile* profile) {
  std::vector<std::unique_ptr<PromoCardInterface>> promo_cards;
  promo_cards.push_back(std::make_unique<PasswordCheckupPromo>(
      profile->GetPrefs(),
      extensions::PasswordsPrivateDelegateFactory::GetForBrowserContext(profile,
                                                                        false)
          .get()));
  promo_cards.push_back(std::make_unique<WebPasswordManagerPromo>(
      profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile)));
  promo_cards.push_back(
      std::make_unique<PasswordManagerShortcutPromo>(profile));
  promo_cards.push_back(
      std::make_unique<AccessOnAnyDevicePromo>(profile->GetPrefs()));
  return promo_cards;
}

PromoCardInterface::PromoCardInterface(const std::string& id,
                                       PrefService* prefs)
    : prefs_(prefs) {
  const base::Value::List& promo_card_prefs =
      prefs_->GetList(prefs::kPasswordManagerPromoCardsList);
  for (const auto& promo_card_pref : promo_card_prefs) {
    if (*promo_card_pref.GetDict().FindString(kIdKey) == id) {
      number_of_times_shown_ =
          *promo_card_pref.GetDict().FindInt(kNumberOfTimesShownKey);
      last_time_shown_ =
          base::ValueToTime(promo_card_pref.GetDict().Find(kLastTimeShownKey))
              .value();
      was_dismissed_ = *promo_card_pref.GetDict().FindBool(kWasDismissedKey);
      return;
    }
  }
  // If there is no pref with matching ID, create one.
  ScopedListPrefUpdate update(prefs_, prefs::kPasswordManagerPromoCardsList);
  update.Get().Append(CreatePromoCardPrefEntry(id));
}

PromoCardInterface::~PromoCardInterface() = default;

std::u16string PromoCardInterface::GetActionButtonText() const {
  return std::u16string();
}

void PromoCardInterface::OnPromoCardDismissed() {
  was_dismissed_ = true;

  ScopedListPrefUpdate update(prefs_, prefs::kPasswordManagerPromoCardsList);
  for (auto& promo_card_pref : update.Get()) {
    if (*promo_card_pref.GetDict().FindString(kIdKey) == GetPromoID()) {
      promo_card_pref.GetDict().Set(kWasDismissedKey, true);
      break;
    }
  }
  base::UmaHistogramEnumeration("PasswordManager.PromoCard.Dismissed",
                                ConvertIdToType(GetPromoID()));
}

void PromoCardInterface::OnPromoCardShown() {
  number_of_times_shown_++;
  last_time_shown_ = base::Time::Now();

  ScopedListPrefUpdate update(prefs_, prefs::kPasswordManagerPromoCardsList);
  for (auto& promo_card_pref : update.Get()) {
    if (*promo_card_pref.GetDict().FindString(kIdKey) == GetPromoID()) {
      promo_card_pref.GetDict().Set(kNumberOfTimesShownKey,
                                    number_of_times_shown_);
      promo_card_pref.GetDict().Set(kLastTimeShownKey,
                                    base::TimeToValue(last_time_shown_));
      break;
    }
  }
  base::UmaHistogramEnumeration("PasswordManager.PromoCard.Shown",
                                ConvertIdToType(GetPromoID()));
}

PasswordCheckupPromo::PasswordCheckupPromo(
    PrefService* prefs,
    extensions::PasswordsPrivateDelegate* delegate)
    : PromoCardInterface(kCheckupPromoId, prefs) {
  CHECK(delegate);
  delegate_ = delegate->AsWeakPtr();
}

PasswordCheckupPromo::~PasswordCheckupPromo() = default;

std::string PasswordCheckupPromo::GetPromoID() const {
  return kCheckupPromoId;
}

bool PasswordCheckupPromo::ShouldShowPromo() const {
  // Don't show promo if checkup is disabled by policy.
  if (!prefs_->GetBoolean(
          password_manager::prefs::kPasswordLeakDetectionEnabled)) {
    return false;
  }
  // Don't show promo if there are no saved passwords.
  if (!delegate_ || delegate_->GetCredentialGroups().empty()) {
    return false;
  }
  // If promo card was dismissed or shown already for kPromoDisplayLimit times,
  // show it in a week next time.
  bool should_suppress =
      was_dismissed_ || number_of_times_shown_ >= kPromoDisplayLimit;
  return !should_suppress ||
         base::Time().Now() - last_time_shown_ > kPasswordCheckupPromoPeriod;
}

std::u16string PasswordCheckupPromo::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_CHECKUP_PROMO_CARD_TITLE);
}

std::u16string PasswordCheckupPromo::GetDescription() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_CHECKUP_PROMO_CARD_DESCRIPTION);
}

std::u16string PasswordCheckupPromo::GetActionButtonText() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_CHECKUP_PROMO_CARD_ACTION);
}

WebPasswordManagerPromo::WebPasswordManagerPromo(
    PrefService* prefs,
    const syncer::SyncService* sync_service)
    : PromoCardInterface(kWebPasswordManagerPromoId, prefs) {
  // TODO(crbug.com/1464264): Migrate away from `ConsentLevel::kSync` on desktop
  // platforms and remove #ifdef below.
#if BUILDFLAG(IS_ANDROID)
#error If this code is built on Android, please update TODO above.
#endif  // BUILDFLAG(IS_ANDROID)
  sync_enabled_ =
      sync_util::IsSyncFeatureActiveIncludingPasswords(sync_service);
}

std::string WebPasswordManagerPromo::GetPromoID() const {
  return kWebPasswordManagerPromoId;
}

bool WebPasswordManagerPromo::ShouldShowPromo() const {
  if (!sync_enabled_) {
    return false;
  }

  return !was_dismissed_ && number_of_times_shown_ < kPromoDisplayLimit;
}

std::u16string WebPasswordManagerPromo::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_WEB_PROMO_CARD_TITLE);
}

std::u16string WebPasswordManagerPromo::GetDescription() const {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_UI_WEB_PROMO_CARD_DESCRIPTION,
      base::ASCIIToUTF16(kPasswordManagerAccountDashboardURL));
}

PasswordManagerShortcutPromo::PasswordManagerShortcutPromo(Profile* profile)
    : PromoCardInterface(kShortcutPromoId, profile->GetPrefs()),
      profile_(profile) {
  is_shortcut_installed_ =
      web_app::FindInstalledAppWithUrlInScope(
          profile, GURL(chrome::kChromeUIPasswordManagerURL))
          .has_value();
}

std::string PasswordManagerShortcutPromo::GetPromoID() const {
  return kShortcutPromoId;
}

bool PasswordManagerShortcutPromo::ShouldShowPromo() const {
  if (is_shortcut_installed_) {
    return false;
  }

  auto* service = UserEducationServiceFactory::GetForBrowserContext(profile_);
  if (service) {
    auto* tutorial_service = &service->tutorial_service();
    if (tutorial_service && tutorial_service->IsRunningTutorial()) {
      return false;
    }
  }

  return !was_dismissed_ && number_of_times_shown_ < kPromoDisplayLimit;
}

std::u16string PasswordManagerShortcutPromo::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_SHORTCUT_PROMO_CARD_TITLE);
}

std::u16string PasswordManagerShortcutPromo::GetDescription() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_SHORTCUT_PROMO_CARD_DESCRIPTION);
}

std::u16string PasswordManagerShortcutPromo::GetActionButtonText() const {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UI_ADD_SHORTCUT_TITLE);
}

AccessOnAnyDevicePromo::AccessOnAnyDevicePromo(PrefService* prefs)
    : PromoCardInterface(kAccessOnAnyDevicePromoId, prefs) {}

std::string AccessOnAnyDevicePromo::GetPromoID() const {
  return kAccessOnAnyDevicePromoId;
}

bool AccessOnAnyDevicePromo::ShouldShowPromo() const {
  return !was_dismissed_ && number_of_times_shown_ < kPromoDisplayLimit;
}

std::u16string AccessOnAnyDevicePromo::GetTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_PASSWORD_MANAGER_UI_ANY_DEVICE_PROMO_CARD_TITLE);
}

std::u16string AccessOnAnyDevicePromo::GetDescription() const {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_MANAGER_UI_ANY_DEVICE_PROMO_CARD_DESCRIPTION,
      kGetStartedOnAndroid, kGetStartedOnIOS);
}

}  // namespace password_manager
