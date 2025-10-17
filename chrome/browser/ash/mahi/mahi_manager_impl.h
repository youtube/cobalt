// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_MAHI_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_MAHI_MAHI_MANAGER_IMPL_H_

#include <memory>
#include <string>

#include "ash/system/mahi/mahi_ui_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/mahi/mahi_cache_manager.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/components/mahi/public/cpp/mahi_web_contents_manager.h"
#include "chromeos/crosapi/mojom/mahi.mojom-forward.h"
#include "chromeos/crosapi/mojom/mahi.mojom.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/manta/mahi_provider.h"
#include "ui/gfx/image/image_skia.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

class MahiNudgeController;

// Implementation of `MahiManager`.
class MahiManagerImpl : public chromeos::MahiManager,
                        public chromeos::MagicBoostState::Observer,
                        public history::HistoryServiceObserver {
 public:
  MahiManagerImpl();

  MahiManagerImpl(const MahiManagerImpl&) = delete;
  MahiManagerImpl& operator=(const MahiManagerImpl&) = delete;

  ~MahiManagerImpl() override;

  // chromeos::MahiManager:
  std::u16string GetContentTitle() override;
  gfx::ImageSkia GetContentIcon() override;
  GURL GetContentUrl() override;
  std::u16string GetSelectedText() override;
  void GetContent(MahiContentCallback callback) override;
  void GetSummary(MahiSummaryCallback callback) override;
  void GetElucidation(MahiElucidationCallback callback) override;
  void GetOutlines(MahiOutlinesCallback callback) override;
  void GoToOutlineContent(int outline_id) override;
  void AnswerQuestion(const std::u16string& question,
                      bool current_panel_content,
                      MahiAnswerQuestionCallback callback) override;
  void AnswerQuestionRepeating(
      const std::u16string& question,
      bool current_panel_content,
      MahiAnswerQuestionCallbackRepeating callback) override;
  void GetSuggestedQuestion(MahiGetSuggestedQuestionCallback callback) override;
  void SetCurrentFocusedPageInfo(crosapi::mojom::MahiPageInfoPtr info) override;
  void OnContextMenuClicked(
      crosapi::mojom::MahiContextMenuRequestPtr context_menu_request) override;
  void OpenFeedbackDialog() override;
  void OpenMahiPanel(int64_t display_id,
                     const gfx::Rect& mahi_menu_bounds) override;
  bool IsEnabled() override;
  void SetMediaAppPDFFocused() override;
  void MediaAppPDFClosed(
      const base::UnguessableToken media_app_client_id) override;
  std::optional<base::UnguessableToken> GetMediaAppPDFClientId() const override;
  void ClearCache() override;
  bool AllowRepeatingAnswers() override;

  // Called when availability for a refresh changes based on the shown content.
  void NotifyRefreshAvailability(bool available);

  MahiUiController* ui_controller_for_test() { return &ui_controller_; }

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;

 private:
  friend class MahiManagerImplTest;
  friend class MahiManagerImplFeatureKeyTest;

  // chromeos::MagicBoostState::Observer:
  void OnHMREnabledUpdated(bool enabled) override;
  void OnIsDeleting() override;

  // Interrupts the flow of `context_menu_request` handling by showing a
  // disclaimer view. The original flow will be resumed if the consent status
  // becomes approved. NOTE: This function should be called only if the magic
  // boost feature is enabled.
  void InterrputRequestHandlingWithDisclaimerView(
      crosapi::mojom::MahiContextMenuRequestPtr context_menu_request);

  // Initialize required provider if it is not initialized yet, and discard
  // pending requests to avoid racing condition.
  // Returns true if successfully initialized.
  bool MaybeInitializeAndDiscardPendingRequests();

  void MaybeObserveHistoryService();

  void OpenMahiPanelForElucidation(int64_t display_id,
                                   const gfx::Rect& mahi_menu_bounds);

  void OnGetPageContent(crosapi::mojom::MahiPageInfoPtr request_page_info,
                        MahiContentCallback callback,
                        crosapi::mojom::MahiPageContentPtr mahi_content_ptr);

  void OnGetPageContentForSummary(
      crosapi::mojom::MahiPageInfoPtr request_page_info,
      MahiSummaryCallback callback,
      crosapi::mojom::MahiPageContentPtr mahi_content_ptr);

  void OnGetPageContentForElucidation(
      const std::u16string& selected_text,
      crosapi::mojom::MahiPageInfoPtr request_page_info,
      MahiElucidationCallback callback,
      crosapi::mojom::MahiPageContentPtr mahi_content_ptr);

  void OnGetPageContentForQA(
      crosapi::mojom::MahiPageInfoPtr request_page_info,
      const std::u16string& question,
      MahiAnswerQuestionCallback callback,
      crosapi::mojom::MahiPageContentPtr mahi_content_ptr);

  void OnMahiProviderSummaryResponse(
      crosapi::mojom::MahiPageInfoPtr request_page_info,
      MahiSummaryCallback summary_callback,
      base::Value::Dict dict,
      manta::MantaStatus status);

  void OnMahiProviderElucidationResponse(
      crosapi::mojom::MahiPageInfoPtr request_page_info,
      const std::u16string& selected_text,
      MahiElucidationCallback elucidation_callback,
      base::Value::Dict dict,
      manta::MantaStatus status);

  void OnMahiProviderQAResponse(
      crosapi::mojom::MahiPageInfoPtr request_page_info,
      const std::u16string& question,
      MahiAnswerQuestionCallback callback,
      base::Value::Dict dict,
      manta::MantaStatus status);

  void CacheCurrentPanelContent(crosapi::mojom::MahiPageInfo request_page_info,
                                crosapi::mojom::MahiPageContent mahi_content);

  // Updates `current_selected_text_` from web contents manager or media app
  // content manager.
  void UpdateCurrentSelectedText();

  base::ScopedObservation<chromeos::MagicBoostState,
                          chromeos::MagicBoostState::Observer>
      magic_boost_state_observation_{this};

  // These `Ptr`s should never be null. To invalidate them, assign them a
  // `New()` instead of calling `reset()`.
  crosapi::mojom::MahiPageInfoPtr current_page_info_ =
      crosapi::mojom::MahiPageInfo::New();

  crosapi::mojom::MahiPageContentPtr current_panel_content_ =
      crosapi::mojom::MahiPageContent::New();

  // Stores metadata of the current content in the panel.
  crosapi::mojom::MahiPageInfoPtr current_panel_info_ =
      crosapi::mojom::MahiPageInfo::New();

  // Stores current selected text when the user triggers feature that works for
  // selected text, e.g. Elucidation, or Summary for selection.
  // Unlike `current_page_info_` that may change when the user activates another
  // browser tab, therefore we need `current_panel_info_` to keep track of the
  // page info used by the existing result panel for future interactions
  // launched from the panel (e.g. QA), `current_selected_text_` is only updated
  // when user explicitly triggers Mahi functions from the widget, therefore it
  // is always the source of truth if an existing result panel is based on user
  // selected text.
  // That's to say, if `current_selected_text_` is not nullopt, the existing
  // result panel must be based on it. We rely on this fact for a few checks.
  std::optional<std::u16string> current_selected_text_;

  // Pair of question and their corresponding answer for the current panel
  // content
  std::vector<std::pair<std::string, std::string>> current_panel_qa_;

  std::unique_ptr<manta::MahiProvider> mahi_provider_;

  raw_ptr<chromeos::MahiWebContentsManager> mahi_web_contents_manager_ =
      nullptr;

  // Keeps track of the latest result and code, used for feedback.
  std::u16string latest_summary_;
  std::u16string latest_elucidation_;
  chromeos::MahiResponseStatus latest_response_status_;

  MahiUiController ui_controller_;

  std::unique_ptr<MahiCacheManager> cache_manager_;

  std::unique_ptr<MahiNudgeController> mahi_nudge_controller_;

  // If true, tries to get content from MediaAppContentManager instead.
  bool media_app_pdf_focused_ = false;
  base::UnguessableToken media_app_client_id_;

  // Runs the specified closures when the consent state becomes approved or
  // declined. Built when handling particular context menu actions without the
  // Mahi feature approved by user. Destroyed when user responds to the
  // disclaimer view.
  // NOTE: It is used only when the magic boost feature is enabled.
  std::unique_ptr<chromeos::MagicBoostState::Observer>
      on_consent_state_update_closure_runner_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      scoped_history_service_observer_{this};

  base::WeakPtrFactory<MahiManagerImpl> weak_ptr_factory_for_closure_runner_{
      this};

  base::WeakPtrFactory<MahiManagerImpl> weak_ptr_factory_for_requests_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_MAHI_MAHI_MANAGER_IMPL_H_
