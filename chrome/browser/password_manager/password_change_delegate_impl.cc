// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change_delegate_impl.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_change/change_form_submission_verifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/generation/password_generator.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace {

using password_manager::BrowserSavePasswordProgressLogger;
using password_manager::PasswordForm;
using password_manager::PasswordFormCache;
using password_manager::PasswordFormManager;

PasswordFormCache& GetFormCache(content::WebContents* web_contents) {
  auto* client = static_cast<password_manager::PasswordManagerClient*>(
      ChromePasswordManagerClient::FromWebContents(web_contents));
  CHECK(client);
  CHECK(client->GetPasswordManager());

  auto* cache = client->GetPasswordManager()->GetPasswordFormCache();
  CHECK(cache);
  return *cache;
}

void LogPasswordFormDetectedMetric(bool form_detected,
                                   base::TimeDelta time_delta) {
  base::UmaHistogramBoolean("PasswordManager.ChangePasswordFormDetected",
                            form_detected);
  if (form_detected) {
    base::UmaHistogramMediumTimes(
        "PasswordManager.ChangePasswordFormDetectionTime", time_delta);
  }
}

// Helper object which waits for change password parsing, invokes callback on
// completion. If form isn't found withing
// `PasswordChangeDelegateImpl::kChangePasswordFormWaitingTimeout` callback is
// invoked with null.
class ParsedPasswordFormWaiter
    : public password_manager::PasswordFormManagerObserver {
 public:
  using PasswordFormFoundCallback =
      base::OnceCallback<void(password_manager::PasswordFormManager*)>;

  ParsedPasswordFormWaiter(content::WebContents* web_contents,
                           PasswordFormFoundCallback callback)
      : web_contents_(web_contents->GetWeakPtr()),
        callback_(std::move(callback)) {
    GetFormCache(web_contents).SetObserver(weak_ptr_factory_.GetWeakPtr());

    timeout_timer_.Start(
        FROM_HERE,
        PasswordChangeDelegateImpl::kChangePasswordFormWaitingTimeout, this,
        &ParsedPasswordFormWaiter::OnTimeout);
  }

  ~ParsedPasswordFormWaiter() override {
    if (web_contents_) {
      GetFormCache(web_contents_.get()).ResetObserver();
    }
  }

 private:
  // password_manager::PasswordFormManagerObserver Impl
  void OnPasswordFormParsed(
      password_manager::PasswordFormManager* form_manager) override {
    CHECK(callback_);
    CHECK(form_manager);

    const PasswordForm* parsed_form = form_manager->GetParsedObservedForm();
    CHECK(parsed_form);

    // New password field and password confirmation fields are indicators of
    // a change password form.
    if (!parsed_form->new_password_element_renderer_id ||
        !parsed_form->confirmation_password_element_renderer_id) {
      return;
    }

    // Do not invoke anything after calling the `callback_` as object might be
    // destroyed immediately after.
    std::move(callback_).Run(form_manager);
  }

  void OnTimeout() {
    CHECK(callback_);
    std::move(callback_).Run(nullptr);
  }

  base::OneShotTimer timeout_timer_;
  base::WeakPtr<content::WebContents> web_contents_;
  PasswordFormFoundCallback callback_;

  base::WeakPtrFactory<ParsedPasswordFormWaiter> weak_ptr_factory_{this};
};

std::u16string GeneratePassword(
    const PasswordForm& form,
    password_manager::PasswordGenerationFrameHelper* generation_helper) {
  auto iter = std::ranges::find(form.form_data.fields(),
                                form.new_password_element_renderer_id,
                                &autofill::FormFieldData::renderer_id);
  CHECK(iter != form.form_data.fields().end());

  return generation_helper->GeneratePassword(
      form.url,
      autofill::password_generation::PasswordGenerationType::kAutomatic,
      autofill::CalculateFormSignature(form.form_data),
      autofill::CalculateFieldSignatureForField(*iter), iter->max_length());
}

bool IsActive(base::WeakPtr<content::WebContents> web_contents) {
  if (!web_contents) {
    return false;
  }
#if !BUILDFLAG(IS_ANDROID)
  // Can be null in unit tests.
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents.get());
  return tab ? tab->IsActivated() : false;
#else
  return false;
#endif
}

void DisplayChangePasswordBubbleAutomatically(
    base::WeakPtr<content::WebContents> original_tab,
    base::WeakPtr<content::WebContents> tab_with_password_change) {
  content::WebContents* contents = IsActive(original_tab) ? original_tab.get()
                                   : IsActive(tab_with_password_change)
                                       ? tab_with_password_change.get()
                                       : nullptr;
  if (contents) {
    ManagePasswordsUIController::FromWebContents(contents)
        ->ShowChangePasswordBubble();
  }
}

std::unique_ptr<BrowserSavePasswordProgressLogger> GetLoggerIfAvailable(
    base::WeakPtr<content::WebContents> web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  auto* client = static_cast<password_manager::PasswordManagerClient*>(
      ChromePasswordManagerClient::FromWebContents(web_contents.get()));
  if (!client) {
    return nullptr;
  }

  autofill::LogManager* log_manager = client->GetCurrentLogManager();
  if (log_manager && log_manager->IsLoggingActive()) {
    return std::make_unique<BrowserSavePasswordProgressLogger>(log_manager);
  }

  return nullptr;
}

}  // namespace

PasswordChangeDelegateImpl::PasswordChangeDelegateImpl(
    GURL change_password_url,
    std::u16string username,
    std::u16string password,
    content::WebContents* originator,
    OpenPasswordChangeTabCallback callback)
    : change_password_url_(std::move(change_password_url)),
      username_(std::move(username)),
      original_password_(std::move(password)),
      originator_(originator->GetWeakPtr()),
      open_password_change_tab_callback_(std::move(callback)) {
  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogMessage(
        BrowserSavePasswordProgressLogger::STRING_PASSWORD_CHANGE_STARTED);
  }
}

PasswordChangeDelegateImpl::~PasswordChangeDelegateImpl() {
  base::UmaHistogramEnumeration(kFinalPasswordChangeStatusHistogram,
                                current_state_);
  base::UmaHistogramBoolean(kWasPasswordChangeNewTabFocused,
                            was_password_change_tab_focused_);
  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogBoolean(
        BrowserSavePasswordProgressLogger::STRING_PASSWORD_CHANGE_FINISHED,
        current_state_ == State::kPasswordSuccessfullyChanged);
  }
}

void PasswordChangeDelegateImpl::OfferPasswordChangeUi() {
  UpdateState(PasswordChangeDelegate::State::kOfferingPasswordChange);
}

void PasswordChangeDelegateImpl::StartPasswordChangeFlow() {
  if (IsPrivacyNoticeAcknowledged()) {
    StartPasswordChange();
    return;
  }
  UpdateState(State::kWaitingForAgreement);
}

void PasswordChangeDelegateImpl::StartPasswordChange() {
  CHECK(originator_);
  flow_start_time_ = base::Time::Now();
  UpdateState(State::kWaitingForChangePasswordForm);
  if (executor_) {
    executor_->OpenURL(
        content::OpenURLParams(change_password_url_, content::Referrer(),
                               WindowOpenDisposition::CURRENT_TAB,
                               ui::PAGE_TRANSITION_LINK,
                               /*is_renderer_initiated=*/false),
        base::DoNothing());
  } else {
    content::WebContents* new_tab = open_password_change_tab_callback_.Run(
        change_password_url_, originator_.get());
    if (!new_tab) {
      UpdateState(State::kPasswordChangeFailed);
      return;
    }
    executor_ = new_tab->GetWeakPtr();
  }

  form_waiter_ = std::make_unique<ParsedPasswordFormWaiter>(
      executor_.get(),
      base::BindOnce(&PasswordChangeDelegateImpl::OnPasswordChangeFormParsed,
                     weak_ptr_factory_.GetWeakPtr()));
  Observe(executor_.get());
}

void PasswordChangeDelegateImpl::OnPasswordChangeFormParsed(
    password_manager::PasswordFormManager* form_manager) {
  form_waiter_.reset();

  LogPasswordFormDetectedMetric(/*form_detected=*/form_manager,
                                base::Time::Now() - flow_start_time_);
  if (!form_manager) {
    UpdateState(State::kChangePasswordFormNotFound);
    return;
  }

  CHECK(!submission_verifier_);
  CHECK(executor_);
  generated_password_ = GeneratePassword(
      *form_manager->GetParsedObservedForm(),
      form_manager->GetDriver()->GetPasswordGenerationHelper());

  submission_verifier_ = std::make_unique<ChangeFormSubmissionVerifier>(
      executor_.get(),
      base::BindOnce(
          &PasswordChangeDelegateImpl::OnChangeFormSubmissionVerified,
          weak_ptr_factory_.GetWeakPtr()));
  submission_verifier_->FillChangePasswordForm(form_manager, original_password_,
                                               generated_password_);
  UpdateState(PasswordChangeDelegate::State::kChangingPassword);
}

void PasswordChangeDelegateImpl::WebContentsDestroyed() {
  // PasswordFormManager keeps raw pointers to PasswordManagerClient reset it
  // immediately to avoid keeping dangling pointer.
  submission_verifier_.reset();
}

void PasswordChangeDelegateImpl::OnVisibilityChanged(
    content::Visibility visibility) {
  if (!was_password_change_tab_focused_ &&
      visibility == content::Visibility::VISIBLE) {
    was_password_change_tab_focused_ = true;
  }
}

bool PasswordChangeDelegateImpl::IsPasswordChangeOngoing(
    content::WebContents* web_contents) {
  return (originator_ && originator_.get() == web_contents) ||
         (executor_ && executor_.get() == web_contents);
}

PasswordChangeDelegate::State PasswordChangeDelegateImpl::GetCurrentState()
    const {
  return current_state_;
}

void PasswordChangeDelegateImpl::Stop() {
  observers_.Notify(&PasswordChangeDelegate::Observer::OnPasswordChangeStopped,
                    this);
}

void PasswordChangeDelegateImpl::Restart() {
  CHECK_EQ(State::kChangePasswordFormNotFound, current_state_);
  CHECK(!submission_verifier_);

  StartPasswordChange();
}

void PasswordChangeDelegateImpl::OnPasswordFormSubmission(
    content::WebContents* web_contents) {
  if (submission_verifier_) {
    submission_verifier_->OnPasswordFormSubmission(web_contents);
  }
}

#if !BUILDFLAG(IS_ANDROID)
void PasswordChangeDelegateImpl::OpenPasswordChangeTab() {
  if (executor_) {
    auto* tab_interface = tabs::TabInterface::GetFromContents(executor_.get());
    CHECK(tab_interface);

    auto* tabs_strip =
        tab_interface->GetBrowserWindowInterface()->GetTabStripModel();
    tabs_strip->ActivateTabAt(
        tabs_strip->GetIndexOfWebContents(executor_.get()));
  }
}
#endif

void PasswordChangeDelegateImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PasswordChangeDelegateImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::u16string PasswordChangeDelegateImpl::GetDisplayOrigin() const {
  GURL url = submission_verifier_ ? submission_verifier_->GetURL()
                                  : change_password_url_;
  return url_formatter::FormatUrlForSecurityDisplay(
      url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
}

const std::u16string& PasswordChangeDelegateImpl::GetUsername() const {
  return username_;
}

const std::u16string& PasswordChangeDelegateImpl::GetGeneratedPassword() const {
  return generated_password_;
}

void PasswordChangeDelegateImpl::OnPrivacyNoticeAccepted() {
  Profile* profile =
      Profile::FromBrowserContext(originator_->GetBrowserContext());
  profile->GetPrefs()->SetBoolean(
      password_manager::prefs::kPasswordChangeFlowNoticeAgreement, true);
  StartPasswordChange();
}

void PasswordChangeDelegateImpl::UpdateState(
    PasswordChangeDelegate::State new_state) {
  if (new_state == current_state_) {
    return;
  }
  current_state_ = new_state;
  observers_.Notify(&PasswordChangeDelegate::Observer::OnStateChanged,
                    new_state);

  switch (current_state_) {
    case State::kWaitingForChangePasswordForm:
    case State::kChangingPassword:
      return;
    case State::kChangePasswordFormNotFound:
    case State::kPasswordSuccessfullyChanged:
      if (executor_ && !IsActive(executor_)) {
        executor_->ClosePage();
      }
      // Fallthrough to trigger bubble display.
      [[fallthrough]];
    case State::kOfferingPasswordChange:
    case State::kWaitingForAgreement:
    case State::kPasswordChangeFailed:
      DisplayChangePasswordBubbleAutomatically(originator_, executor_);
      break;
  }

  if (auto logger = GetLoggerIfAvailable(originator_)) {
    logger->LogNumber(
        BrowserSavePasswordProgressLogger::STRING_PASSWORD_CHANGE_STATE_CHANGED,
        static_cast<int>(new_state));
  }
}

void PasswordChangeDelegateImpl::OnChangeFormSubmissionVerified(bool result) {
  base::UmaHistogramMediumTimes("PasswordManager.PasswordChangeTimeOverall",
                                base::Time::Now() - flow_start_time_);
  if (!result) {
    UpdateState(State::kPasswordChangeFailed);
  } else {
    // Password change was successful. Save new password with an original
    // username.
    submission_verifier_->SavePassword(username_);
    UpdateState(State::kPasswordSuccessfullyChanged);
  }

  submission_verifier_.reset();
}

bool PasswordChangeDelegateImpl::IsPrivacyNoticeAcknowledged() const {
  // TODO(391147412): Use OptimizationGuideKeyedService
  // ShouldFeatureAllowModelExecutionForSignedInUser() instead.
  Profile* profile =
      Profile::FromBrowserContext(originator_->GetBrowserContext());
  return profile->GetPrefs()->GetBoolean(
      password_manager::prefs::kPasswordChangeFlowNoticeAgreement);
}

base::WeakPtr<PasswordChangeDelegate> PasswordChangeDelegateImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
