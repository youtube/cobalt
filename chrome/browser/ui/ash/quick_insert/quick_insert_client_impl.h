// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_QUICK_INSERT_QUICK_INSERT_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_QUICK_INSERT_QUICK_INSERT_CLIENT_IMPL_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/lobster/lobster_controller.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/quick_insert_client.h"
#include "ash/quick_insert/quick_insert_web_paste_target.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/search/ranking/ranker_manager.h"
#include "chrome/browser/ash/input_method/editor_announcer.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/ime/text_input_client.h"
#include "url/gurl.h"

class Profile;
class ChromeSearchResult;
class QuickInsertFileSuggester;
class QuickInsertThumbnailLoader;

namespace app_list {
class SearchEngine;
class SearchProvider;
}  // namespace app_list

namespace ash {
class QuickInsertController;
}

namespace user_manager {
class User;
}

// Implements the QuickInsertClient used by Ash.
class QuickInsertClientImpl
    : public ash::QuickInsertClient,
      public user_manager::UserManager::UserSessionStateObserver {
 public:
  // Sets this instance as the client of `controller`.
  // Automatically unsets the client when this instance is destroyed.
  // `manager` needs to outlive this class.
  explicit QuickInsertClientImpl(ash::QuickInsertController* controller,
                                 user_manager::UserManager* user_manager);
  QuickInsertClientImpl(const QuickInsertClientImpl&) = delete;
  QuickInsertClientImpl& operator=(const QuickInsertClientImpl&) = delete;
  ~QuickInsertClientImpl() override;

  // ash::QuickInsertClient:
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory()
      override;
  void StartCrosSearch(const std::u16string& query,
                       std::optional<ash::QuickInsertCategory> category,
                       CrosSearchResultsCallback callback) override;
  void StopCrosQuery() override;
  bool IsEligibleForEditor() override;
  ShowEditorCallback CacheEditorContext() override;
  ShowLobsterCallback CacheLobsterContext(
      ui::TextInputClient* text_input_client) override;
  void GetSuggestedEditorResults(
      SuggestedEditorResultsCallback callback) override;
  void GetRecentLocalFileResults(size_t max_files,
                                 base::TimeDelta now_delta,
                                 RecentFilesCallback callback) override;
  void GetRecentDriveFileResults(size_t max_files,
                                 RecentFilesCallback callback) override;
  void FetchFileThumbnail(const base::FilePath& path,
                          const gfx::Size& size,
                          FetchFileThumbnailCallback callback) override;
  std::optional<ash::QuickInsertWebPasteTarget> GetWebPasteTarget() override;
  void Announce(std::u16string_view message) override;
  history::HistoryService* GetHistoryService() override;
  favicon::FaviconService* GetFaviconService() override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

  void set_ranker_manager_for_test(
      std::unique_ptr<app_list::RankerManager> ranker_manager) {
    ranker_manager_ = std::move(ranker_manager);
  }

  // Returns a bitmask of `AutocompleteProvider::Type` for Quick Insert's
  // `SearchController`.
  int LauncherSearchProviderTypes(bool bookmarks, bool history, bool open_tabs);

 private:
  void OnCrosSearchResultsUpdated(
      CrosSearchResultsCallback callback,
      ash::AppListSearchResultType result_type,
      std::vector<std::unique_ptr<ChromeSearchResult>> results);
  void SetProfileByUser(const user_manager::User* user);
  void SetProfile(Profile* profile);

  std::unique_ptr<app_list::SearchProvider>
  CreateOmniboxProvider(bool bookmarks, bool history, bool open_tabs);
  std::unique_ptr<app_list::SearchProvider> CreateSearchProviderForCategory(
      ash::QuickInsertCategory category);

  void ShowEditor(std::optional<std::string> preset_query_id,
                  std::optional<std::string> freeform_text);

  void ShowLobster(std::optional<std::string> query);

  ash::input_method::EditorLiveRegionAnnouncer announcer_;

  raw_ptr<ash::QuickInsertController> controller_ = nullptr;
  raw_ptr<Profile> profile_ = nullptr;

  std::unique_ptr<app_list::SearchEngine> search_engine_;

  // A dedicated cros search engine for filtered searches.
  std::unique_ptr<app_list::SearchEngine> filtered_search_engine_;
  std::optional<ash::QuickInsertCategory> current_filter_category_;

  std::unique_ptr<app_list::RankerManager> ranker_manager_;

  std::unique_ptr<QuickInsertFileSuggester> file_suggester_;
  std::unique_ptr<QuickInsertThumbnailLoader> thumbnail_loader_;

  std::unique_ptr<ash::LobsterController::Trigger> lobster_trigger_;

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::UserSessionStateObserver>
      user_session_state_observation_{this};

  base::WeakPtrFactory<QuickInsertClientImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_QUICK_INSERT_QUICK_INSERT_CLIENT_IMPL_H_
