// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_QUICK_INSERT_CONTROLLER_H_
#define ASH_QUICK_INSERT_QUICK_INSERT_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/ash_export.h"
#include "ash/quick_insert/metrics/quick_insert_feature_usage_metrics.h"
#include "ash/quick_insert/metrics/quick_insert_session_metrics.h"
#include "ash/quick_insert/model/quick_insert_emoji_history_model.h"
#include "ash/quick_insert/model/quick_insert_emoji_suggester.h"
#include "ash/quick_insert/model/quick_insert_model.h"
#include "ash/quick_insert/quick_insert_asset_fetcher_impl_delegate.h"
#include "ash/quick_insert/quick_insert_caps_lock_bubble_controller.h"
#include "ash/quick_insert/quick_insert_insert_media_request.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/quick_insert_suggestions_controller.h"
#include "ash/quick_insert/quick_insert_web_paste_target.h"
#include "ash/quick_insert/search/quick_insert_search_controller.h"
#include "ash/quick_insert/views/quick_insert_feature_tour.h"
#include "ash/quick_insert/views/quick_insert_view_delegate.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/unique_widget_ptr.h"

class PrefRegistrySimple;
class PrefService;

namespace input_method {
class ImeKeyboard;
}

namespace ui {
class TextInputClient;
}

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {

class QuickInsertAssetFetcher;
class QuickInsertClient;
class QuickInsertModel;
class QuickInsertPasteRequest;
class QuickInsertActionOnNextFocusRequest;

// Controls a Quick Insert widget.
class ASH_EXPORT QuickInsertController
    : public QuickInsertViewDelegate,
      public views::ViewObserver,
      public QuickInsertAssetFetcherImplDelegate {
 public:
  QuickInsertController();
  QuickInsertController(const QuickInsertController&) = delete;
  QuickInsertController& operator=(const QuickInsertController&) = delete;
  ~QuickInsertController() override;

  // Maximum time to wait for focus to be regained after completing the feature
  // tour. If this timeout is reached, we stop waiting for focus and show the
  // Quick Insert widget regardless of the focus state.
  static constexpr base::TimeDelta kShowWidgetPostFeatureTourTimeout =
      base::Seconds(2);

  // Time from when the insert is issued and when we give up inserting.
  static constexpr base::TimeDelta kInsertMediaTimeout = base::Seconds(2);

  // Time from when a search starts to when the first set of results are
  // published.
  static constexpr base::TimeDelta kBurnInPeriod = base::Milliseconds(200);

  // Registers Quick Insert prefs to the provided `registry`.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Sets the `client` used by this class and the widget to communicate with the
  // browser. `client` may be set to null, which will close the Widget if it's
  // open, and may call "stop search" methods on the PREVIOUS client.
  // If `client` is not null, then it must remain valid for the lifetime of this
  // class, or until AFTER `SetClient` is called with a different client.
  // Caution: If `client` outlives this class, the client should avoid calling
  // this method on a destructed class instance to avoid a use after free.
  void SetClient(QuickInsertClient* client);

  // This should be run when the Prefs from the client is ready.
  void OnClientPrefsSet(PrefService* prefs);

  // Toggles the visibility of the Quick Insert widget.
  // This must only be called after `SetClient` is called with a valid client.
  // `trigger_event_timestamp` is the timestamp of the event that triggered the
  // Widget to be toggled. For example, if the feature was triggered by a mouse
  // click, then it should be the timestamp of the click. By default, the
  // timestamp is the time this function is called.
  void ToggleWidget(
      base::TimeTicks trigger_event_timestamp = base::TimeTicks::Now());

  // Returns the Quick Insert widget for tests.
  views::Widget* widget_for_testing() { return widget_.get(); }
  QuickInsertFeatureTour& feature_tour_for_testing() { return feature_tour_; }
  QuickInsertCapsLockBubbleController&
  caps_lock_bubble_controller_for_testing() {
    return caps_lock_bubble_controller_;
  }

  // QuickInsertViewDelegate:
  std::vector<QuickInsertCategory> GetAvailableCategories() override;
  void GetZeroStateSuggestedResults(SuggestedResultsCallback callback) override;
  void GetResultsForCategory(QuickInsertCategory category,
                             SearchResultsCallback callback) override;
  void StartSearch(std::u16string_view query,
                   std::optional<QuickInsertCategory> category,
                   SearchResultsCallback callback) override;
  void StopSearch() override;
  void StartEmojiSearch(std::u16string_view,
                        EmojiSearchResultsCallback callback) override;
  void CloseWidgetThenInsertResultOnNextFocus(
      const QuickInsertSearchResult& result) override;
  void OpenResult(const QuickInsertSearchResult& result) override;
  void ShowEmojiPicker(ui::EmojiPickerCategory category,
                       std::u16string_view query) override;
  void ShowEditor(std::optional<std::string> preset_query_id,
                  std::optional<std::string> freeform_text) override;
  void ShowLobster(std::optional<std::string> freeform_text) override;
  QuickInsertAssetFetcher* GetAssetFetcher() override;
  QuickInsertSessionMetrics& GetSessionMetrics() override;
  QuickInsertActionType GetActionForResult(
      const QuickInsertSearchResult& result) override;
  std::vector<QuickInsertEmojiResult> GetSuggestedEmoji() override;
  bool IsGifsEnabled() override;
  QuickInsertModeType GetMode() override;
  QuickInsertCapsLockPosition GetCapsLockPosition() override;

  // views:ViewObserver:
  void OnViewIsDeleting(views::View* view) override;

  // QuickInsertAssetFetcherImplDelegate:
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory()
      override;
  void FetchFileThumbnail(const base::FilePath& path,
                          const gfx::Size& size,
                          FetchFileThumbnailCallback callback) override;

  // Disables the feature tour. Only works in tests.
  static void DisableFeatureTourForTesting();

 private:
  // Trigger source for showing the Quick Insert widget. This is used to
  // determine how the widget should be shown on the screen.
  enum class WidgetTriggerSource {
    // The user triggered Quick Insert as part of their usual user flow, e.g.
    // toggled Quick Insert with a key press.
    kDefault,
    // The user triggered Quick Insert by completing the feature tour.
    kFeatureTour,
  };

  // Active Quick Insert session tied to the lifetime of the QuickInsertWidget.
  struct Session {
    QuickInsertModel model;
    QuickInsertEmojiHistoryModel emoji_history_model;
    QuickInsertEmojiSuggester emoji_suggester;
    QuickInsertSessionMetrics session_metrics;
    // Periodically records usage metrics based on the Standard Feature Usage
    // Logging (SFUL) framework.
    QuickInsertFeatureUsageMetrics feature_usage_metrics;

    Session(PrefService* prefs,
            ui::TextInputClient* focused_client,
            input_method::ImeKeyboard* ime_keyboard,
            QuickInsertModel::EditorStatus editor_status,
            QuickInsertModel::LobsterStatus lobster_status,
            QuickInsertEmojiSuggester::GetNameCallback get_name);
    ~Session();
  };

  void ShowWidget(base::TimeTicks trigger_event_timestamp,
                  WidgetTriggerSource trigger_source);
  void CloseWidget();
  void ShowWidgetPostFeatureTour();
  void InsertResultOnNextFocus(const QuickInsertSearchResult& result);
  void OnInsertCompleted(const QuickInsertRichMedia& media,
                         QuickInsertInsertMediaRequest::Result result);
  PrefService* GetPrefs();

  std::optional<QuickInsertWebPasteTarget> GetWebPasteTarget();

  QuickInsertFeatureTour feature_tour_;
  QuickInsertCapsLockBubbleController caps_lock_bubble_controller_;
  std::unique_ptr<Session> session_;
  views::UniqueWidgetPtr widget_;
  std::unique_ptr<QuickInsertAssetFetcher> asset_fetcher_;
  std::unique_ptr<QuickInsertInsertMediaRequest> insert_media_request_;
  std::unique_ptr<QuickInsertPasteRequest> paste_request_;
  std::unique_ptr<QuickInsertActionOnNextFocusRequest> caps_lock_request_;
  QuickInsertSuggestionsController suggestions_controller_;
  QuickInsertSearchController search_controller_;

  raw_ptr<QuickInsertClient> client_ = nullptr;

  base::OnceCallback<void(std::optional<std::string> preset_query_id,
                          std::optional<std::string> freeform_text)>
      show_editor_callback_;

  base::OnceCallback<void(std::optional<std::string> freeform_text)>
      show_lobster_callback_;

  // Timer used to delay closing the Widget for accessibility.
  base::OneShotTimer close_widget_delay_timer_;

  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};

  base::WeakPtrFactory<QuickInsertController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_QUICK_INSERT_CONTROLLER_H_
