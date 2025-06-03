// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_WIZARD_CONTEXT_H_
#define CHROME_BROWSER_ASH_LOGIN_WIZARD_CONTEXT_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class UserContext;

// Structure that defines data that need to be passed between screens during
// WizardController flows.
class WizardContext {
 public:
  WizardContext();
  ~WizardContext();

  WizardContext(const WizardContext&) = delete;
  WizardContext& operator=(const WizardContext&) = delete;

  // Should be tweaked by the tests only in case we need this early in the init
  // process. Otherwise tweak context from `GetWizardContextForTesting`.
  static bool g_is_branded_build;

  enum class EnrollmentPreference {
    kKiosk,
    kEnterprise,
  };

  enum class GaiaPath {
    kDefault,
    kChildSignup,
    kChildSignin,
    kReauth,
  };

  struct GaiaConfig {
    // GAIA path to be loaded the next time GAIA Sign-in screen is shown.
    // This is usually set just before showing the GAIA screen and reset
    // to the default value when hiding the screen.
    GaiaPath gaia_path = GaiaPath::kDefault;

    // The GAIA path shown the last time the GAIA Sign-in screen was shown.
    // This is set by the GAIA screen when hiding the screen.
    GaiaPath last_gaia_path_shown = GaiaPath::kDefault;

    // The account ID to be used in the next loading of GAIA webview.
    // The value is reset to `EmptyAccountId()` when hiding the screen.
    AccountId prefilled_account = EmptyAccountId();
  };

  struct RecoverySetup {
    // Whether the recovery auth factor is supported. Used for metrics.
    bool is_supported = false;

    // Controls if user should be asked about recovery factor setup
    // on the consolidated consent screen.
    bool ask_about_recovery_consent = false;

    // User's choice about using recovery factor. Filled by
    // consolidated consent screen, used by auth_factors_setup screen.
    bool recovery_factor_opted_in = false;
  };

  // This enum helps tell which auth setup flow we're currently going through.
  // This helps screens that modify auth factors such as local password and
  // pin to easily determine if we're adding a new auth factor as part of
  // first user setup or updating an existing auth factor, for instance, as
  // part of recovery flow.
  enum class AuthChangeFlow { kInitialSetup, kRecovery };

  struct KnowledgeFactorSetup {
    // Whether usage of local password is forced.
    bool local_password_forced = false;

    AuthChangeFlow auth_setup_flow = AuthChangeFlow::kInitialSetup;
  };

  // Configuration for automating OOBE screen actions, e.g. during device
  // version rollback.
  // Set by WizardController.
  // Used by multiple screens.
  base::Value::Dict configuration;

  // Indicates that enterprise enrollment was triggered early in the OOBE
  // process, so Update screen should be skipped and Enrollment start right
  // after EULA screen.
  // Set by Welcome, Network and EULA screens.
  // Used by Update screen and WizardController.
  bool enrollment_triggered_early = false;

  // Indicates that user selects to sign in or create a new account for a child.
  bool sign_in_as_child = false;

  // Indicates whether user creates a new gaia account when set up the device
  // for a child.
  bool is_child_gaia_account_new = false;

  // Whether the screens should be skipped so that the normal gaia login is
  // shown. Set by WizardController SkipToLoginForTesting and checked on
  // EnrollmentScreen::MaybeSkip (determines if enrollment screen should be
  // skipped when enrollment isn't mandatory) and UserCreationScreen::MaybeSkip
  // (determines if user creation screen should be skipped).
  bool skip_to_login_for_tests = false;

  // Whether wizard controller should skip to the update screen. Setting this
  // flag will ignore hid detection results.
  bool skip_to_update_for_tests = false;

  // Whether the post login screens should be skipped. Used in MaybeSkip by
  // screens in tests. Is set by WizardController::SkipPostLoginScreensForTests.
  bool skip_post_login_screens_for_tests = false;

  // Whether CHOOBE screen should be skipped. Setting this flag will force skip
  // CHOOBE screen regardless of the number of eligible optional screens.
  // To test an optional screen without selecting the screen from CHOOBE screen,
  // set this flag to true before logging in as a new user.
  bool skip_choobe_for_tests = false;

  // Whether user creation screen is enabled (could be disabled due to disabled
  // feature or on managed device). It determines the behavior of back button
  // for GaiaScreen and OfflineLoginScreen. Value is set to true in
  // UserCreationScreen::MaybeSkip when screen is shown and will be set to false
  // when screen is skipped or when cancel action is called.
  bool is_user_creation_enabled = false;

  // When --tpm-is-dynamic switch is set taking TPM ownership is happening right
  // before enrollment. If TakeOwnership returns STATUS_DEVICE_ERROR this
  // flag helps to set the right error message.
  bool tpm_owned_error = false;

  // When --tpm-is-dynamic switch is set taking TPM ownership is happening right
  // before enrollment. If TakeOwnership returns STATUS_DBUS_ERROR this
  // flag helps to set the right error message.
  bool tpm_dbus_error = false;

  // True if this is a branded build (i.e. Google Chrome).
  bool is_branded_build = g_is_branded_build;

  // Force that OOBE Login display isn't destroyed right after login due to all
  // screens being skipped.
  bool defer_oobe_flow_finished_for_tests = false;

  // Indicates which type of licenses should be used for primary button on
  // enrollment screen.
  EnrollmentPreference enrollment_preference_ =
      WizardContext::EnrollmentPreference::kEnterprise;

  // The data for recovery setup flow.
  RecoverySetup recovery_setup;

  KnowledgeFactorSetup knowledge_factor_setup;

  // Authorization data that is required by PinSetup screen to add PIN as
  // another possible auth factor. Can be empty (if PIN is not supported).
  // In future will be replaced by AuthSession.
  std::unique_ptr<UserContext> extra_factors_auth_session;

  // Same as above, but the actual context is stored in AuthSessionStorage,
  // and the token can be used to retrieve it.
  absl::optional<AuthProofToken> extra_factors_token;

  // If the onboarding flow wasn't completed by the user we will try to show
  // TermsOfServiceScreen to them first and then continue the flow with this
  // screen. If the user has already completed onboarding, but
  // TermsOfServiceScreen should be shown on login this will be set to
  // ash::OOBE_SCREEN_UNKNOWN.
  OobeScreenId screen_after_managed_tos;

  // This is set to true when the user hits the keyboard shortcut triggering the
  // associated LoginAccelerator. This is used in place of a feature flag to
  // determine whether to display the Quick Start calls to action.
  bool quick_start_enabled = false;

  // This ID maps onto the instance_id used in
  // ash::multidevice::RemoteDevice. If a user connects their phone during Quick
  // Start, Quick Start saves this ID. After Quick Start, the multidevice screen
  // will show UI enhancements if this quick_start_phone_instance_id is present.
  std::string quick_start_phone_instance_id;

  // Whether the user is currently setting up OOBE using QuickStart.
  // TODO(b/283724988) - Combine QuickStart fields into a class.
  bool quick_start_setup_ongoing = false;

  // WiFi credentials that a received by a Chromebook from an Android device
  // during Quick Start flow. They are set on the QuickStartScreen during the
  // initial connection between the devices.
  // TODO(b/283724988) - Combine QuickStart fields into a class.
  absl::optional<ash::quick_start::mojom::WifiCredentials>
      quick_start_wifi_credentials;

  // If this is a first login after update from CloudReady to a new version.
  // During such an update show users license agreement and data collection
  // consent.
  bool is_cloud_ready_update_flow = false;

  // Determining ownership can take some time. Instead of finding out if the
  // current user is an owner of the device we reuse this value. It is set
  // during ConsolidatedConsentScreen.
  absl::optional<bool> is_owner_flow;

  // True when gesture navigation screen was shown during the OOBE.
  bool is_gesture_navigation_screen_was_shown = false;

  // True when user is inside the "Add Person" flow.
  bool is_add_person_flow = false;

  // True if user clicked "Select more fatures" button on the last CHOOBE
  // selected screen.
  bool return_to_choobe_screen = false;

  // Information that is used during Cryptohome recovery or password changed
  // flow.
  std::unique_ptr<UserContext> user_context;

  // Configuration for GAIA screen. If the configs needs to be updated, it
  // should be updated before showing the GAIA screen. If the GAIA screen is
  // already shown, a call to reload GAIA webview may be necessary.
  GaiaConfig gaia_config;
};

// Returns |true| if this is an OOBE flow after enterprise enrollment.
bool IsRollbackFlow(const WizardContext& context);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_WIZARD_CONTEXT_H_
