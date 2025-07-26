// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_context_menu_manager.h"

#include <algorithm>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/webauthn/context_menu_helper.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_feedback_data.h"
#include "components/autofill/core/browser/foundations/autofill_driver.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/integrators/autofill_ai_delegate.h"
#include "components/autofill/core/common/aliases.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_manual_fallback_metrics_recorder.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/prefs/pref_service.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"
#include "components/variations/service/variations_service.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "components/plus_addresses/resources/vector_icons.h"
#endif

namespace autofill {

namespace {

using ::password_manager::ContentPasswordManagerDriver;

constexpr char kFeedbackPlaceholder[] =
    "What steps did you just take?\n"
    "(1)\n"
    "(2)\n"
    "(3)\n"
    "\n"
    "What was the expected result?\n"
    "\n"
    "What happened instead? (Please include the screenshot below)";

// Constant determining the icon size in the context menu.
constexpr int kContextMenuIconSize = 16;

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const gfx::VectorIcon& kPlusAddressLogoIcon =
    plus_addresses::kPlusAddressLogoSmallIcon;
#else
const gfx::VectorIcon& kPlusAddressLogoIcon = vector_icons::kEmailIcon;
#endif

bool ShouldShowAutofillContextMenu(const content::ContextMenuParams& params) {
  if (!params.form_control_type) {
    return false;
  }
  // Return true (only) on text fields.
  //
  // Note that this switch is over `blink::mojom::FormControlType`, not
  // `autofill::FormControlType`. Therefore, it does not handle
  // `autofill::FormControlType::kContentEditable`, which is covered by the
  // above if-condition `!params.form_control_type`.
  //
  // TODO(crbug.com/40285492): Unify with functions from form_autofill_util.cc.
  switch (*params.form_control_type) {
    case blink::mojom::FormControlType::kInputEmail:
    case blink::mojom::FormControlType::kInputMonth:
    case blink::mojom::FormControlType::kInputNumber:
    case blink::mojom::FormControlType::kInputPassword:
    case blink::mojom::FormControlType::kInputSearch:
    case blink::mojom::FormControlType::kInputTelephone:
    case blink::mojom::FormControlType::kInputText:
    case blink::mojom::FormControlType::kInputUrl:
    case blink::mojom::FormControlType::kTextArea:
      return true;
    case blink::mojom::FormControlType::kButtonButton:
    case blink::mojom::FormControlType::kButtonSubmit:
    case blink::mojom::FormControlType::kButtonReset:
    case blink::mojom::FormControlType::kButtonPopover:
    case blink::mojom::FormControlType::kFieldset:
    case blink::mojom::FormControlType::kInputButton:
    case blink::mojom::FormControlType::kInputCheckbox:
    case blink::mojom::FormControlType::kInputColor:
    case blink::mojom::FormControlType::kInputDate:
    case blink::mojom::FormControlType::kInputDatetimeLocal:
    case blink::mojom::FormControlType::kInputFile:
    case blink::mojom::FormControlType::kInputHidden:
    case blink::mojom::FormControlType::kInputImage:
    case blink::mojom::FormControlType::kInputRadio:
    case blink::mojom::FormControlType::kInputRange:
    case blink::mojom::FormControlType::kInputReset:
    case blink::mojom::FormControlType::kInputSubmit:
    case blink::mojom::FormControlType::kInputTime:
    case blink::mojom::FormControlType::kInputWeek:
    case blink::mojom::FormControlType::kOutput:
    case blink::mojom::FormControlType::kSelectOne:
    case blink::mojom::FormControlType::kSelectMultiple:
      return false;
  }
  NOTREACHED();
}

// Returns true if the given id is one generated for autofill context menu.
bool IsAutofillCustomCommandId(
    AutofillContextMenuManager::CommandId command_id) {
  static constexpr auto kAutofillCommands = base::MakeFixedFlatSet<int>({
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PLUS_ADDRESS,
      IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK,
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD,
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS,
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD,
      IDC_CONTENT_CONTEXT_AUTOFILL_PREDICTION_IMPROVEMENTS,
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE,
  });
  return kAutofillCommands.contains(command_id.value());
}

bool IsLikelyDogfoodClient() {
  auto* variations_service = g_browser_process->variations_service();
  if (!variations_service) {
    return false;
  }
  return variations_service->IsLikelyDogfoodClient();
}

// Returns true if the field is a username or password field.
bool IsPasswordFormField(ContentPasswordManagerDriver& password_manager_driver,
                         const content::ContextMenuParams& params) {
  const autofill::FieldRendererId current_field_renderer_id(
      params.field_renderer_id);
  return password_manager_driver.GetPasswordManager()
      ->GetPasswordFormCache()
      ->GetPasswordForm(&password_manager_driver, current_field_renderer_id);
}

// Returns true if the user has autofillable passwords saved.
bool UserHasPasswordsSaved(
    ContentPasswordManagerDriver& password_manager_driver) {
  password_manager::PasswordManagerClient* client =
      password_manager_driver.GetPasswordManager()->GetClient();
  return client->GetPrefs()->GetBoolean(
             password_manager::prefs::
                 kAutofillableCredentialsProfileStoreLoginDatabase) ||
         client->GetPrefs()->GetBoolean(
             password_manager::prefs::
                 kAutofillableCredentialsAccountStoreLoginDatabase);
}

base::Value::Dict LoadTriggerFormAndFieldLogs(
    AutofillManager& manager,
    const LocalFrameToken& frame_token,
    const content::ContextMenuParams& params) {
  if (!ShouldShowAutofillContextMenu(params)) {
    return base::Value::Dict();
  }

  FormGlobalId form_global_id = {frame_token,
                                 FormRendererId(params.form_renderer_id)};

  base::Value::Dict trigger_form_logs;
  if (FormStructure* form = manager.FindCachedFormById(form_global_id)) {
    trigger_form_logs.Set("triggerFormSignature", form->FormSignatureAsStr());

    if (params.form_control_type) {
      FieldGlobalId field_global_id = {
          frame_token, FieldRendererId(params.field_renderer_id)};
      auto field =
          std::ranges::find_if(*form, [&field_global_id](const auto& field) {
            return field->global_id() == field_global_id;
          });
      if (field != form->end()) {
        trigger_form_logs.Set("triggerFieldSignature",
                              (*field)->FieldSignatureAsStr());
      }
    }
  }
  return trigger_form_logs;
}

}  // namespace

AutofillContextMenuManager::AutofillContextMenuManager(
    RenderViewContextMenuBase* delegate,
    ui::SimpleMenuModel* menu_model)
    : menu_model_(menu_model), delegate_(delegate) {
  DCHECK(delegate_);
  params_ = delegate_->params();
}

AutofillContextMenuManager::~AutofillContextMenuManager() = default;

void AutofillContextMenuManager::AppendItems() {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordManualFallbackAvailable)) {
    MaybeAddAutofillManualFallbackItems();
    MaybeAddAutofillFeedbackItem();
  } else {
    MaybeAddAutofillFeedbackItem();
    MaybeAddAutofillManualFallbackItems();
  }
}

bool AutofillContextMenuManager::IsCommandIdSupported(int command_id) {
  return IsAutofillCustomCommandId(CommandId(command_id));
}

bool AutofillContextMenuManager::IsCommandIdEnabled(int command_id) {
  return true;
}

void AutofillContextMenuManager::ExecuteCommand(int command_id) {
  content::RenderFrameHost* rfh = delegate_->GetRenderFrameHost();
  if (!rfh) {
    return;
  }
  if (command_id ==
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE) {
    webauthn::OnPasskeyFromAnotherDeviceContextMenuItemSelected(rfh);
    return;
  }
  ContentAutofillDriver* autofill_driver =
      ContentAutofillDriver::GetForRenderFrameHost(rfh);
  if (!autofill_driver) {
    return;
  }
  CHECK(IsAutofillCustomCommandId(CommandId(command_id)));

  if (command_id == IDC_CONTENT_CONTEXT_AUTOFILL_PREDICTION_IMPROVEMENTS) {
    ExecuteAutofillAiCommand(autofill_driver->GetFrameToken(),
                             *autofill_driver);
    return;
  }

  if (command_id == IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK) {
    ExecuteAutofillFeedbackCommand(autofill_driver->GetFrameToken(),
                                   autofill_driver->GetAutofillManager());
    return;
  }

  if (command_id == IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PLUS_ADDRESS) {
    ExecuteFallbackForPlusAddressesCommand(*autofill_driver);
    return;
  }

  if (command_id ==
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD) {
    ExecuteFallbackForSelectPasswordCommand(*autofill_driver);
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (command_id ==
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS) {
    // This function also records metrics.
    NavigateToManagePasswordsPage(
        chrome::FindBrowserWithTab(web_contents),
        password_manager::ManagePasswordsReferrer::kPasswordContextMenu);
    return;
  }

  if (command_id ==
      IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD) {
    // This function also records metrics.
    password_manager_util::UserTriggeredManualGenerationFromContextMenu(
        ChromePasswordManagerClient::FromWebContents(web_contents),
        autofill::ContentAutofillClient::FromWebContents(web_contents));
    return;
  }
}

void AutofillContextMenuManager::MaybeAddAutofillFeedbackItem() {
  content::RenderFrameHost* rfh = delegate_->GetRenderFrameHost();
  if (!rfh) {
    return;
  }

  ContentAutofillDriver* autofill_driver =
      ContentAutofillDriver::GetForRenderFrameHost(rfh);
  // Do not show autofill context menu options for input fields that cannot be
  // filled by the driver. See crbug.com/1367547.
  if (!autofill_driver || !autofill_driver->CanShowAutofillUi()) {
    return;
  }

  // Includes the option of submitting feedback on Autofill.
  if (autofill_driver->GetAutofillClient().IsAutofillEnabled() &&
      IsLikelyDogfoodClient()) {
    menu_model_->AddItemWithStringIdAndIcon(
        IDC_CONTENT_CONTEXT_AUTOFILL_FEEDBACK,
        IDS_CONTENT_CONTEXT_AUTOFILL_FEEDBACK,
        ui::ImageModel::FromVectorIcon(vector_icons::kDogfoodIcon));

    menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
  }
}

void AutofillContextMenuManager::MaybeAddAutofillManualFallbackItems() {
  if (!ShouldShowAutofillContextMenu(params_)) {
    // Autofill entries are only available in input or text area fields
    return;
  }

  content::RenderFrameHost* rfh = delegate_->GetRenderFrameHost();
  if (!rfh) {
    return;
  }

  ContentAutofillDriver* autofill_driver =
      ContentAutofillDriver::GetForRenderFrameHost(rfh);
  ContentPasswordManagerDriver* password_manager_driver =
      ContentPasswordManagerDriver::GetForRenderFrameHost(rfh);

  bool add_plus_address_fallback = false;
  bool add_passwords_fallback = false;
  bool add_autofill_ai = false;

  // Do not show autofill context menu options for input fields that cannot be
  // filled by the driver. See crbug.com/1367547.
  if (autofill_driver && autofill_driver->CanShowAutofillUi()) {
    auto* web_contents = content::WebContents::FromRenderFrameHost(
        autofill_driver->render_frame_host());
    add_autofill_ai = ShouldAddAutofillAiItem(
        autofill_driver->GetAutofillClient().GetAutofillAiDelegate(),
        web_contents->GetPrimaryMainFrame()->GetLastCommittedURL());
    add_plus_address_fallback =
        ShouldAddPlusAddressManualFallbackItem(*autofill_driver);
  }

  // Do not show password manager context menu options for input fields that
  // cannot be filled by the driver. See crbug.com/1367547.
  if (password_manager_driver && password_manager_driver->CanShowAutofillUi()) {
    add_passwords_fallback =
        ShouldAddPasswordsManualFallbackItem(*password_manager_driver);
  }

  if (!add_plus_address_fallback && !add_passwords_fallback &&
      !add_autofill_ai) {
    return;
  }

  if (add_passwords_fallback) {
    AddPasswordsManualFallbackItems(*password_manager_driver);

    const bool select_passwords_option_shown =
        UserHasPasswordsSaved(*password_manager_driver);
    if (select_passwords_option_shown) {
      LogSelectPasswordManualFallbackContextMenuEntryShown(
          CHECK_DEREF(password_manager_driver));
    }
  }
  if (add_autofill_ai) {
    menu_model_->AddItemWithStringIdAndIcon(
        IDC_CONTENT_CONTEXT_AUTOFILL_PREDICTION_IMPROVEMENTS,
        IDS_CONTENT_CONTEXT_AUTOFILL_PREDICTION_IMPROVEMENTS,
        ui::ImageModel::FromVectorIcon(
            vector_icons::kLocationOnChromeRefreshIcon, ui::kColorIcon,
            kContextMenuIconSize));
  }
  if (add_plus_address_fallback) {
    menu_model_->AddItemWithStringIdAndIcon(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PLUS_ADDRESS,
        IDS_PLUS_ADDRESS_FALLBACK_LABEL_CONTEXT_MENU,
        ui::ImageModel::FromVectorIcon(kPlusAddressLogoIcon, ui::kColorIcon,
                                       kContextMenuIconSize));
    MaybeMarkLastItemAsNewFeature(
        plus_addresses::features::kPlusAddressFallbackFromContextMenu);
    // TODO(crbug.com/327566698): Log metrics for plus address fallbacks, too.
  }
  menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
}

bool AutofillContextMenuManager::ShouldAddPlusAddressManualFallbackItem(
    ContentAutofillDriver& autofill_driver) {
  if (params_.form_control_type &&
      params_.form_control_type.value() ==
          blink::mojom::FormControlType::kInputPassword) {
    return false;
  }

  auto* web_contents = content::WebContents::FromRenderFrameHost(
      autofill_driver.render_frame_host());
  const plus_addresses::PlusAddressService* plus_address_service =
      PlusAddressServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  AutofillClient& client = autofill_driver.GetAutofillManager().client();
  return plus_address_service &&
         plus_address_service->ShouldShowManualFallback(
             client.GetLastCommittedPrimaryMainFrameOrigin(),
             client.IsOffTheRecord()) &&
         base::FeatureList::IsEnabled(
             plus_addresses::features::kPlusAddressFallbackFromContextMenu);
}

bool AutofillContextMenuManager::ShouldAddAutofillAiItem(
    AutofillAiDelegate* delegate,
    const GURL& url) {
  // TODO(crbug.com/372158654): Implement suitable criteria or remove the entry.
  return false;
}

bool AutofillContextMenuManager::ShouldAddPasswordsManualFallbackItem(
    ContentPasswordManagerDriver& password_manager_driver) {
  // Password suggestions should not be triggered on text areas.
  if (params_.form_control_type == blink::mojom::FormControlType::kTextArea) {
    return false;
  }

  return password_manager_driver.GetPasswordManager()
             ->GetClient()
             ->IsFillingEnabled(
                 password_manager_driver.GetLastCommittedURL()) &&
         base::FeatureList::IsEnabled(
             password_manager::features::kPasswordManualFallbackAvailable);
}

void AutofillContextMenuManager::AddPasswordsManualFallbackItems(
    ContentPasswordManagerDriver& password_manager_driver) {
  const bool add_select_password_option =
      UserHasPasswordsSaved(password_manager_driver);
  const bool add_password_generation_option =
      password_manager_util::ManualPasswordGenerationEnabled(
          &password_manager_driver) &&
      password_manager_driver.IsPasswordFieldForPasswordManager(
          autofill::FieldRendererId(params_.field_renderer_id),
          params_.form_control_type);
  const bool add_passkey_from_another_device_option =
      webauthn::IsPasskeyFromAnotherDeviceContextMenuEnabled(
          delegate_->GetRenderFrameHost(), params_.form_renderer_id,
          params_.field_renderer_id) &&
      base::FeatureList::IsEnabled(
          password_manager::features::
              kWebAuthnUsePasskeyFromAnotherDeviceInContextMenu);
  const bool add_import_passwords_option = !add_select_password_option;

  if (add_select_password_option) {
    menu_model_->AddItemWithStringId(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD,
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SELECT_PASSWORD);
    MaybeMarkLastItemAsNewFeature(
        password_manager::features::kPasswordManualFallbackAvailable);
  }
  if (add_password_generation_option) {
    menu_model_->AddItemWithStringId(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD,
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_SUGGEST_PASSWORD);
  }
  if (add_passkey_from_another_device_option) {
    menu_model_->AddItemWithStringId(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE,
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_USE_PASSKEY_FROM_ANOTHER_DEVICE);
  }
  if (add_import_passwords_option) {
    menu_model_->AddItemWithStringId(
        IDC_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS,
        IDS_CONTENT_CONTEXT_AUTOFILL_FALLBACK_PASSWORDS_IMPORT_PASSWORDS);
  }
}

void AutofillContextMenuManager::
    LogSelectPasswordManualFallbackContextMenuEntryShown(
        ContentPasswordManagerDriver& password_manager_driver) {
  password_manager_driver.GetPasswordAutofillManager()
      ->GetPasswordManualFallbackMetricsRecorder()
      .ContextMenuEntryShown(
          /*classified_as_target_filling_password=*/
          IsPasswordFormField(password_manager_driver, params_));
}

void AutofillContextMenuManager::
    LogSelectPasswordManualFallbackContextMenuEntryAccepted() {
  content::RenderFrameHost* rfh = delegate_->GetRenderFrameHost();
  ContentPasswordManagerDriver* password_manager_driver =
      rfh ? ContentPasswordManagerDriver::GetForRenderFrameHost(rfh) : nullptr;

  if (password_manager_driver) {
    password_manager_driver->GetPasswordAutofillManager()
        ->GetPasswordManualFallbackMetricsRecorder()
        .ContextMenuEntryAccepted(/*classified_as_target_filling_password=*/
                                  IsPasswordFormField(*password_manager_driver,
                                                      params_));
  }
}

void AutofillContextMenuManager::ExecuteAutofillAiCommand(
    const LocalFrameToken& frame_token,
    ContentAutofillDriver& autofill_driver) {
  autofill_driver.browser_events().RendererShouldTriggerSuggestions(
      FieldGlobalId(frame_token, FieldRendererId(params_.field_renderer_id)),
      AutofillSuggestionTriggerSource::kAutofillAi);
}

void AutofillContextMenuManager::ExecuteAutofillFeedbackCommand(
    const LocalFrameToken& frame_token,
    AutofillManager& manager) {
  // The cast is safe since the context menu is only available on Desktop.
  auto& client = static_cast<ContentAutofillClient&>(manager.client());
  Browser* browser = chrome::FindBrowserWithTab(&client.GetWebContents());
  chrome::ShowFeedbackPage(
      browser, feedback::kFeedbackSourceAutofillContextMenu,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/kFeedbackPlaceholder,
      /*category_tag=*/"dogfood_autofill_feedback",
      /*extra_diagnostics=*/std::string(),
      /*autofill_metadata=*/
      data_logs::FetchAutofillFeedbackData(
          &manager,
          LoadTriggerFormAndFieldLogs(manager, frame_token, params_)));
}

void AutofillContextMenuManager::ExecuteFallbackForPlusAddressesCommand(
    AutofillDriver& autofill_driver) {
  autofill_driver.RendererShouldTriggerSuggestions(
      /*field_id=*/{autofill_driver.GetFrameToken(),
                    FieldRendererId(params_.field_renderer_id)},
      AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses);

  base::RecordAction(base::UserMetricsAction(
      "PlusAddresses.ManualFallbackDesktopContextManualFallbackSelected"));
  UserEducationService::MaybeNotifyNewBadgeFeatureUsed(
      delegate_->GetBrowserContext(),
      plus_addresses::features::kPlusAddressFallbackFromContextMenu);
}

void AutofillContextMenuManager::ExecuteFallbackForSelectPasswordCommand(
    AutofillDriver& autofill_driver) {
  autofill_driver.RendererShouldTriggerSuggestions(
      /*field_id=*/{autofill_driver.GetFrameToken(),
                    FieldRendererId(params_.field_renderer_id)},
      AutofillSuggestionTriggerSource::kManualFallbackPasswords);

  LogSelectPasswordManualFallbackContextMenuEntryAccepted();
  UserEducationService::MaybeNotifyNewBadgeFeatureUsed(
      delegate_->GetBrowserContext(),
      password_manager::features::kPasswordManualFallbackAvailable);
}

void AutofillContextMenuManager::MaybeMarkLastItemAsNewFeature(
    const base::Feature& feature) {
  menu_model_->SetIsNewFeatureAt(menu_model_->GetItemCount() - 1,
                                 UserEducationService::MaybeShowNewBadge(
                                     delegate_->GetBrowserContext(), feature));
}

}  // namespace autofill
