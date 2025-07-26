// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_page_handler.h"

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/auth_controller.h"
#include "chrome/browser/glic/browser_conditions.h"
#include "chrome/browser/glic/glic.mojom.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_tab_data.h"
#include "chrome/browser/glic/glic_web_client_access.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/media/audio_ducker.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/message.h"
#include "ui/gfx/geometry/mojom/geometry.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace glic {

// WARNING: One instance of this class is created per WebUI navigated to
// chrome://glic. The design and implementation of this class, which plumbs
// events through GlicKeyedService to other components, relies on the assumption
// that there is exactly 1 WebUI instance. If this assumption is ever violated
// then many classes will break.
class GlicWebClientHandler : public glic::mojom::WebClientHandler,
                             public GlicWindowController::StateObserver,
                             public GlicWebClientAccess,
                             public BrowserAttachObserver {
 public:
  explicit GlicWebClientHandler(
      GlicPageHandler* page_handler,
      content::BrowserContext* browser_context,
      mojo::PendingReceiver<glic::mojom::WebClientHandler> receiver)
      : profile_(Profile::FromBrowserContext(browser_context)),
        page_handler_(page_handler),
        glic_service_(
            GlicKeyedServiceFactory::GetGlicKeyedService(browser_context)),
        pref_service_(profile_->GetPrefs()),
        receiver_(this, std::move(receiver)) {}

  ~GlicWebClientHandler() override {
    if (web_client_) {
      Uninstall();
    }
  }

  // glic::mojom::WebClientHandler implementation.
  void WebClientCreated(
      ::mojo::PendingRemote<glic::mojom::WebClient> web_client,
      WebClientCreatedCallback callback) override {
    web_client_.Bind(std::move(web_client));
    web_client_.set_disconnect_handler(base::BindOnce(
        &GlicWebClientHandler::WebClientDisconnected, base::Unretained(this)));

    // Listen for changes to prefs.
    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        prefs::kGlicMicrophoneEnabled,
        base::BindRepeating(&GlicWebClientHandler::OnPrefChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kGlicGeolocationEnabled,
        base::BindRepeating(&GlicWebClientHandler::OnPrefChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kGlicTabContextEnabled,
        base::BindRepeating(&GlicWebClientHandler::OnPrefChanged,
                            base::Unretained(this)));
    glic_service_->window_controller().AddStateObserver(this);

    focus_changed_subscription_ = glic_service_->AddFocusedTabChangedCallback(
        base::BindRepeating(&GlicWebClientHandler::OnFocusedTabChanged,
                            base::Unretained(this)));

    browser_attach_observation_ = ObserveBrowserForAttachment(profile_, this);

    auto state = glic::mojom::WebClientInitialState::New();
    state->chrome_version = version_info::GetVersion();
    state->microphone_permission_enabled =
        pref_service_->GetBoolean(prefs::kGlicMicrophoneEnabled);
    state->location_permission_enabled =
        pref_service_->GetBoolean(prefs::kGlicGeolocationEnabled);
    state->tab_context_permission_enabled =
        pref_service_->GetBoolean(prefs::kGlicTabContextEnabled);

    state->panel_state =
        glic_service_->window_controller().GetPanelState().Clone();

    state->focused_tab_data =
        CreateFocusedTabData(glic_service_->GetFocusedTabData());
    state->can_attach = browser_attach_observation_->CanAttachToBrowser();

    std::move(callback).Run(std::move(state));
    glic_service_->WebClientCreated();
  }

  void WebClientInitializeFailed() override {
    glic_service_->window_controller().WebClientInitializeFailed();
  }

  void WebClientInitialized() override {
    glic_service_->window_controller().SetWebClient(this);
    // If chrome://glic is opened in a tab for testing, send a synthetic open
    // signal.
    if (page_handler_->guest_contents() !=
        glic_service_->window_controller().GetWebContents()) {
      const auto& panel_state =
          glic_service_->window_controller().GetPanelState();
      web_client_->NotifyPanelWillOpen(panel_state.Clone(), base::DoNothing());
    }
  }

  void CreateTab(const ::GURL& url,
                 bool open_in_background,
                 const std::optional<int32_t> window_id,
                 CreateTabCallback callback) override {
    glic_service_->CreateTab(url, open_in_background, window_id,
                             std::move(callback));
  }

  void OpenGlicSettingsPage() override {
    glic_service_->OpenGlicSettingsPage();
  }

  void ClosePanel() override { glic_service_->ClosePanel(); }

  void AttachPanel() override { glic_service_->AttachPanel(); }

  void DetachPanel() override { glic_service_->DetachPanel(); }

  void ShowProfilePicker() override { glic_service_->ShowProfilePicker(); }

  void ResizeWidget(const gfx::Size& size,
                    base::TimeDelta duration,
                    ResizeWidgetCallback callback) override {
    glic_service_->ResizePanel(size, duration, std::move(callback));
  }

  void GetContextFromFocusedTab(
      glic::mojom::GetTabContextOptionsPtr options,
      GetContextFromFocusedTabCallback callback) override {
    glic_service_->GetContextFromFocusedTab(*options, std::move(callback));
  }

  void CaptureScreenshot(CaptureScreenshotCallback callback) override {
    glic_service_->CaptureScreenshot(std::move(callback));
  }

  void SetAudioDucking(bool enabled,
                       SetAudioDuckingCallback callback) override {
    content::WebContents* web_contents = page_handler_->guest_contents();
    if (!web_contents || web_contents->IsBeingDestroyed()) {
      std::move(callback).Run(false);
      return;
    }
    AudioDucker* audio_ducker =
        AudioDucker::GetOrCreateForPage(web_contents->GetPrimaryPage());
    std::move(callback).Run(enabled ? audio_ducker->StartDuckingOtherAudio()
                                    : audio_ducker->StopDuckingOtherAudio());
  }

  void SetPanelDraggableAreas(
      const std::vector<gfx::Rect>& draggable_areas,
      SetPanelDraggableAreasCallback callback) override {
    if (!draggable_areas.empty()) {
      glic_service_->SetPanelDraggableAreas(draggable_areas);
    } else {
      // Default to the top bar area of the panel.
      // TODO(cuianthony): Define panel dimensions constants in shared location.
      glic_service_->SetPanelDraggableAreas({{0, 0, 400, 80}});
    }
    std::move(callback).Run();
  }

  void SetMicrophonePermissionState(
      bool enabled,
      SetMicrophonePermissionStateCallback callback) override {
    pref_service_->SetBoolean(prefs::kGlicMicrophoneEnabled, enabled);
    std::move(callback).Run();
  }

  void SetLocationPermissionState(
      bool enabled,
      SetLocationPermissionStateCallback callback) override {
    pref_service_->SetBoolean(prefs::kGlicGeolocationEnabled, enabled);
    std::move(callback).Run();
  }

  void SetTabContextPermissionState(
      bool enabled,
      SetTabContextPermissionStateCallback callback) override {
    pref_service_->SetBoolean(prefs::kGlicTabContextEnabled, enabled);
    std::move(callback).Run();
  }

  void SetContextAccessIndicator(bool enabled) override {
    glic_service_->SetContextAccessIndicator(enabled);
  }

  void GetUserProfileInfo(GetUserProfileInfoCallback callback) override {
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile_->GetPath());
    if (!entry) {
      std::move(callback).Run(nullptr);
      return;
    }

    auto result = glic::mojom::UserProfileInfo::New();
    // TODO(crbug.com/382794680): Determine the correct size.
    gfx::Image icon = entry->GetAvatarIcon(512);
    if (!icon.IsEmpty()) {
      result->avatar_icon = icon.AsBitmap();
    }
    result->display_name = base::UTF16ToUTF8(entry->GetGAIAName());
    result->email = base::UTF16ToUTF8(entry->GetUserName());

    std::move(callback).Run(std::move(result));
  }

  void SyncCookies(SyncCookiesCallback callback) override {
    glic_service_->GetAuthController().ForceSyncCookies(std::move(callback));
  }

  void OnUserInputSubmitted(glic::mojom::WebClientMode mode) override {
    glic_service_->metrics()->OnUserInputSubmitted(mode);
  }

  void OnResponseStarted() override {
    glic_service_->metrics()->OnResponseStarted();
  }

  void OnResponseStopped() override {
    glic_service_->metrics()->OnResponseStopped();
  }

  void OnSessionTerminated() override {
    glic_service_->metrics()->OnSessionTerminated();
  }

  void OnResponseRated(bool positive) override {
    glic_service_->metrics()->OnResponseRated(positive);
  }

  void ScrollTo(mojom::ScrollToParamsPtr params,
                ScrollToCallback callback) override {
    if (!base::FeatureList::IsEnabled(features::kGlicScrollTo)) {
      mojo::ReportBadMessage(
          "Client should not be able to call ScrollTo without the GlicScrollTo "
          "feature enabled.");
      return;
    }
    NOTIMPLEMENTED();
    std::move(callback).Run(mojom::ScrollToErrorReason::kNotSupported);
  }

  // GlicWindowController::StateObserver implementation.
  void PanelStateChanged(const glic::mojom::PanelState& panel_state) override {
    web_client_->NotifyPanelStateChange(panel_state.Clone());
  }

  // GlicWebClientAccess implementation.

  void PanelWillOpen(const glic::mojom::PanelState& panel_state,
                     PanelWillOpenCallback done) override {
    web_client_->NotifyPanelWillOpen(
        panel_state.Clone(),
        base::BindOnce(
            [](PanelWillOpenCallback done, glic::mojom::OpenPanelInfoPtr info) {
              base::UmaHistogramEnumeration("Glic.Api.NotifyPanelWillOpen",
                                            info->web_client_mode);
              std::move(done).Run(std::move(info));
            },
            std::move(done)));
  }

  void PanelWasClosed(base::OnceClosure done) override {
    web_client_->NotifyPanelWasClosed(
        mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(done)));
  }

  // BrowserAttachmentObserver implementation.
  void CanAttachToBrowserChanged(bool can_attach) override {
    web_client_->NotifyPanelCanAttachChange(can_attach);
  }

 private:
  void Uninstall() {
    SetAudioDucking(false, base::DoNothing());
    if (glic_service_->window_controller().web_client() == this) {
      glic_service_->window_controller().SetWebClient(nullptr);
    }
    pref_change_registrar_.Reset();
    glic_service_->window_controller().RemoveStateObserver(this);
    focus_changed_subscription_ = {};
    browser_attach_observation_.reset();
  }

  void WebClientDisconnected() { Uninstall(); }

  void OnPrefChanged(const std::string& pref_name) {
    bool is_enabled = pref_service_->GetBoolean(pref_name);
    if (pref_name == prefs::kGlicMicrophoneEnabled) {
      web_client_->NotifyMicrophonePermissionStateChanged(is_enabled);
    } else if (pref_name == prefs::kGlicGeolocationEnabled) {
      web_client_->NotifyLocationPermissionStateChanged(is_enabled);
    } else if (pref_name == prefs::kGlicTabContextEnabled) {
      web_client_->NotifyTabContextPermissionStateChanged(is_enabled);
    } else {
      DCHECK(false) << "Unknown Glic permission pref changed: " << pref_name;
    }
  }

  void OnFocusedTabChanged(FocusedTabData focused_tab_data) {
    web_client_->NotifyFocusedTabChanged(
        CreateFocusedTabData(focused_tab_data));
  }

  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<Profile> profile_;
  raw_ptr<GlicPageHandler> page_handler_;
  raw_ptr<GlicKeyedService> glic_service_;
  raw_ptr<PrefService> pref_service_;
  base::CallbackListSubscription focus_changed_subscription_;
  mojo::Receiver<glic::mojom::WebClientHandler> receiver_;
  mojo::Remote<glic::mojom::WebClient> web_client_;
  std::unique_ptr<BrowserAttachObservation> browser_attach_observation_;
};

GlicPageHandler::GlicPageHandler(
    content::WebContents* webui_contents,
    mojo::PendingReceiver<glic::mojom::PageHandler> receiver,
    mojo::PendingRemote<mojom::Page> page)
    : webui_contents_(webui_contents),
      browser_context_(webui_contents->GetBrowserContext()),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  GetGlicService()->PageHandlerAdded(this);
}

GlicPageHandler::~GlicPageHandler() {
  WebUiStateChanged(glic::mojom::WebUiState::kUninitialized);
  // `GlicWebClientHandler` holds a pointer back to us, so delete it first.
  web_client_handler_.reset();
  GetGlicService()->PageHandlerRemoved(this);
}

GlicKeyedService* GlicPageHandler::GetGlicService() {
  return GlicKeyedServiceFactory::GetGlicKeyedService(browser_context_);
}

void GlicPageHandler::CreateWebClient(
    ::mojo::PendingReceiver<glic::mojom::WebClientHandler>
        web_client_receiver) {
  web_client_handler_ = std::make_unique<GlicWebClientHandler>(
      this, browser_context_, std::move(web_client_receiver));
}

void GlicPageHandler::PrepareForClient(
    base::OnceCallback<void(bool)> callback) {
  GetGlicService()->GetAuthController().CheckAuthBeforeLoad(
      std::move(callback));
}

void GlicPageHandler::WebviewCommitted(const GURL& url) {
  // TODO(crbug.com/388328847): Remove this code once launch issues are ironed
  // out.
  if (url.DomainIs("login.corp.google.com") ||
      url.DomainIs("accounts.google.com")) {
    GetGlicService()->window_controller().LoginPageCommitted();
  }
}

void GlicPageHandler::GuestAdded(content::WebContents* guest_contents) {
  guest_contents_ = guest_contents->GetWeakPtr();
}

void GlicPageHandler::NotifyWindowIntentToShow() {
  page_->IntentToShow();
}

void GlicPageHandler::ClosePanel() {
  GetGlicService()->ClosePanel();
}

void GlicPageHandler::ResizeWidget(const gfx::Size& size,
                                   base::TimeDelta duration,
                                   ResizeWidgetCallback callback) {
  GetGlicService()->ResizePanel(size, duration, std::move(callback));
}

void GlicPageHandler::IsProfileEnabled(IsProfileEnabledCallback callback) {
  bool enabled = GlicEnabling::IsEnabledForProfile(
      Profile::FromBrowserContext(browser_context_));
  std::move(callback).Run(enabled);
}

void GlicPageHandler::WebUiStateChanged(glic::mojom::WebUiState new_state) {
  GetGlicService()->window_controller().WebUiStateChanged(new_state);
}

}  // namespace glic
