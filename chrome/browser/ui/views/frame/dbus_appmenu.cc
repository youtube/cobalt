// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/dbus_appmenu.h"

#include <dlfcn.h>
#include <stddef.h>

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/dbus_appmenu_registrar.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/history/core/browser/top_sites.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/strings/grit/components_strings.h"
#include "dbus/object_path.h"
#include "ui/base/accelerators/menu_label_accelerator_util_linux.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/gfx/text_elider.h"

// A line in the static menu definitions.
struct DbusAppmenuCommand {
  int command;
  int str_id;
};

namespace {

// The maximum number of most visited items to display.
const unsigned int kMostVisitedCount = 8;

// The number of recently closed items to get.
const unsigned int kRecentlyClosedCount = 8;

// Menus more than this many chars long will get trimmed.
const size_t kMaximumMenuWidthInChars = 50;

// Constants used in menu definitions.  The first non-Chrome command is at
// IDC_FIRST_UNBOUNDED_MENU.
enum ReservedCommandId {
  kLastChromeCommand = IDC_FIRST_UNBOUNDED_MENU - 1,
  kMenuEnd,
  kSeparator,
  kSubmenu,
  kTagRecentlyClosed,
  kTagMostVisited,
  kTagProfileEdit,
  kTagProfileCreate,
  kFirstUnreservedCommandId
};

constexpr DbusAppmenuCommand kFileMenu[] = {
    {IDC_NEW_TAB, IDS_NEW_TAB},
    {IDC_NEW_WINDOW, IDS_NEW_WINDOW},
    {IDC_NEW_INCOGNITO_WINDOW, IDS_NEW_INCOGNITO_WINDOW},
    {IDC_RESTORE_TAB, IDS_REOPEN_CLOSED_TABS_LINUX},
    {IDC_OPEN_FILE, IDS_OPEN_FILE_LINUX},
    {IDC_FOCUS_LOCATION, IDS_OPEN_LOCATION_LINUX},
    {kSeparator},
    {IDC_CLOSE_WINDOW, IDS_CLOSE_WINDOW_LINUX},
    {IDC_CLOSE_TAB, IDS_CLOSE_TAB_LINUX},
    {IDC_SAVE_PAGE, IDS_SAVE_PAGE},
    {kSeparator},
    {IDC_PRINT, IDS_PRINT},
    {kMenuEnd}};

constexpr DbusAppmenuCommand kEditMenu[] = {{IDC_CUT, IDS_CUT},
                                            {IDC_COPY, IDS_COPY},
                                            {IDC_PASTE, IDS_PASTE},
                                            {kSeparator},
                                            {IDC_FIND, IDS_FIND},
                                            {kSeparator},
                                            {IDC_OPTIONS, IDS_PREFERENCES},
                                            {kMenuEnd}};

constexpr DbusAppmenuCommand kViewMenu[] = {
    {IDC_SHOW_BOOKMARK_BAR, IDS_SHOW_BOOKMARK_BAR},
    {kSeparator},
    {IDC_STOP, IDS_STOP_MENU_LINUX},
    {IDC_RELOAD, IDS_RELOAD_MENU_LINUX},
    {kSeparator},
    {IDC_FULLSCREEN, IDS_FULLSCREEN},
    {IDC_ZOOM_NORMAL, IDS_TEXT_DEFAULT_LINUX},
    {IDC_ZOOM_PLUS, IDS_TEXT_BIGGER_LINUX},
    {IDC_ZOOM_MINUS, IDS_TEXT_SMALLER_LINUX},
    {kMenuEnd}};

constexpr DbusAppmenuCommand kHistoryMenu[] = {
    {IDC_HOME, IDS_HISTORY_HOME_LINUX},
    {IDC_BACK, IDS_HISTORY_BACK_LINUX},
    {IDC_FORWARD, IDS_HISTORY_FORWARD_LINUX},
    {kSeparator},
    {kTagRecentlyClosed, IDS_HISTORY_CLOSED_LINUX},
    {kSeparator},
    {kTagMostVisited, IDS_HISTORY_VISITED_LINUX},
    {kSeparator},
    {IDC_SHOW_HISTORY, IDS_HISTORY_SHOWFULLHISTORY_LINK},
    {kMenuEnd}};

constexpr DbusAppmenuCommand kToolsMenu[] = {
    {IDC_SHOW_DOWNLOADS, IDS_SHOW_DOWNLOADS},
    {IDC_SHOW_HISTORY, IDS_HISTORY_SHOW_HISTORY},
    {IDC_MANAGE_EXTENSIONS, IDS_SHOW_EXTENSIONS},
    {kSeparator},
    {IDC_TASK_MANAGER, IDS_TASK_MANAGER},
    {IDC_CLEAR_BROWSING_DATA, IDS_CLEAR_BROWSING_DATA},
    {kSeparator},
    {IDC_VIEW_SOURCE, IDS_VIEW_SOURCE},
    {IDC_DEV_TOOLS, IDS_DEV_TOOLS},
    {IDC_DEV_TOOLS_INSPECT, IDS_DEV_TOOLS_ELEMENTS},
    {IDC_DEV_TOOLS_CONSOLE, IDS_DEV_TOOLS_CONSOLE},
    {IDC_DEV_TOOLS_DEVICES, IDS_DEV_TOOLS_DEVICES},
    {kMenuEnd}};

constexpr DbusAppmenuCommand kProfilesMenu[] = {
    {kSeparator},
    {kTagProfileEdit, IDS_PROFILES_MANAGE_BUTTON_LABEL},
    {kTagProfileCreate, IDS_PROFILES_ADD_PROFILE_LABEL},
    {kMenuEnd}};

constexpr DbusAppmenuCommand kHelpMenu[] = {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {IDC_FEEDBACK, IDS_FEEDBACK},
#endif
    {IDC_HELP_PAGE_VIA_MENU, IDS_HELP_PAGE},
    {kMenuEnd}};

void FindMenuItemsForCommandAux(
    ui::MenuModel* menu,
    int command,
    std::vector<std::pair<ui::MenuModel*, size_t>>* menu_items) {
  for (size_t i = 0; i < menu->GetItemCount(); ++i) {
    if (menu->GetCommandIdAt(i) == command)
      menu_items->push_back({menu, i});
    if (menu->GetTypeAt(i) == ui::SimpleMenuModel::ItemType::TYPE_SUBMENU) {
      FindMenuItemsForCommandAux(menu->GetSubmenuModelAt(i), command,
                                 menu_items);
    }
  }
}

std::vector<std::pair<ui::MenuModel*, size_t>> FindMenuItemsForCommand(
    ui::MenuModel* menu,
    int command) {
  std::vector<std::pair<ui::MenuModel*, size_t>> menu_items;
  FindMenuItemsForCommandAux(menu, command, &menu_items);
  return menu_items;
}

}  // namespace

struct DbusAppmenu::HistoryItem {
  HistoryItem() : session_id(SessionID::InvalidValue()) {}

  HistoryItem(const HistoryItem&) = delete;
  HistoryItem& operator=(const HistoryItem&) = delete;

  // The title for the menu item.
  std::u16string title;
  // The URL that will be navigated to if the user selects this item.
  GURL url;

  // This ID is unique for a browser session and can be passed to the
  // TabRestoreService to re-open the closed window or tab that this
  // references. A valid session ID indicates that this is an entry can be
  // restored that way. Otherwise, the URL will be used to open the item and
  // this ID will be invalid.
  SessionID session_id;

  // If the HistoryItem is a window, this will be the vector of tabs. Note
  // that this is a list of weak references. DbusAppmenu::history_items_
  // is the owner of all items. If it is not a window, then the entry is a
  // single page and the vector will be empty.
  std::vector<HistoryItem*> tabs;
};

DbusAppmenu::DbusAppmenu(BrowserView* browser_view, uint32_t browser_frame_id)
    : browser_(browser_view->browser()),
      profile_(browser_->profile()),
      browser_view_(browser_view),
      browser_frame_id_(browser_frame_id),
      tab_restore_service_(nullptr),
      last_command_id_(kFirstUnreservedCommandId - 1) {
  DbusAppmenuRegistrar::GetInstance()->OnMenuBarCreated(this);
}

DbusAppmenu::~DbusAppmenu() {
  auto* registrar = DbusAppmenuRegistrar::GetInstance();
  registrar->OnMenuBarDestroyed(this);

  if (!initialized_)
    return;

  registrar->bus()->UnregisterExportedObject(dbus::ObjectPath(GetPath()));

  for (int command : observed_commands_)
    chrome::RemoveCommandObserver(browser_, command, this);

  pref_change_registrar_.RemoveAll();

  if (tab_restore_service_)
    tab_restore_service_->RemoveObserver(this);

  BrowserList::RemoveObserver(this);
}

void DbusAppmenu::Initialize(DbusMenu::InitializedCallback callback) {
  DCHECK(!initialized_);
  initialized_ = true;

  // First build static menu content.
  root_menu_ = std::make_unique<ui::SimpleMenuModel>(this);

  BuildStaticMenu(IDS_FILE_MENU_LINUX, kFileMenu);
  BuildStaticMenu(IDS_EDIT_MENU_LINUX, kEditMenu);
  BuildStaticMenu(IDS_VIEW_MENU_LINUX, kViewMenu);
  history_menu_ = BuildStaticMenu(IDS_HISTORY_MENU_LINUX, kHistoryMenu);
  BuildStaticMenu(IDS_TOOLS_MENU_LINUX, kToolsMenu);
  profiles_menu_ = BuildStaticMenu(IDS_PROFILES_MENU_NAME, kProfilesMenu);
  BuildStaticMenu(IDS_HELP_MENU_LINUX, kHelpMenu);

  pref_change_registrar_.Init(browser_->profile()->GetPrefs());
  pref_change_registrar_.Add(
      bookmarks::prefs::kShowBookmarkBar,
      base::BindRepeating(&DbusAppmenu::OnBookmarkBarVisibilityChanged,
                          base::Unretained(this)));

  top_sites_ = TopSitesFactory::GetForProfile(profile_);
  if (top_sites_) {
    GetTopSitesData();

    // Register as TopSitesObserver so that we can update ourselves when the
    // TopSites changes.
    scoped_observation_.Observe(top_sites_.get());
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);
  avatar_menu_ = std::make_unique<AvatarMenu>(
      &profile_manager->GetProfileAttributesStorage(), this,
      BrowserList::GetInstance()->GetLastActive());
  avatar_menu_->RebuildMenu();
  BrowserList::AddObserver(this);

  RebuildProfilesMenu();

  menu_service_ = std::make_unique<DbusMenu>(
      DbusAppmenuRegistrar::GetInstance()->bus()->GetExportedObject(
          dbus::ObjectPath(GetPath())),
      std::move(callback));
  menu_service_->SetModel(root_menu_.get(), false);
}

std::string DbusAppmenu::GetPath() const {
  return base::StringPrintf("/com/canonical/menu/%X", browser_frame_id_);
}

ui::SimpleMenuModel* DbusAppmenu::BuildStaticMenu(
    int string_id,
    const DbusAppmenuCommand* commands) {
  toplevel_menus_.push_back(std::make_unique<ui::SimpleMenuModel>(this));
  ui::SimpleMenuModel* menu = toplevel_menus_.back().get();
  for (; commands->command != kMenuEnd; commands++) {
    int command_id = commands->command;
    if (command_id == kSeparator) {
      // Use InsertSeparatorAt() instead of AddSeparator() because the latter
      // refuses to add a separator to an empty menu.
      size_t old_item_count = menu->GetItemCount();
      menu->InsertSeparatorAt(old_item_count,
                              ui::MenuSeparatorType::SPACING_SEPARATOR);

      // Make extra sure the separator got added in case the behavior
      // InsertSeparatorAt() changes.
      CHECK_EQ(old_item_count + 1, menu->GetItemCount());
      continue;
    }

    int command_str_id = commands->str_id;
    if (command_id == IDC_SHOW_BOOKMARK_BAR)
      menu->AddCheckItemWithStringId(command_id, command_str_id);
    else
      menu->AddItemWithStringId(command_id, command_str_id);
    if (command_id < kLastChromeCommand)
      RegisterCommandObserver(command_id);
  }
  root_menu_->AddSubMenu(kSubmenu, l10n_util::GetStringUTF16(string_id), menu);
  return menu;
}

std::unique_ptr<DbusAppmenu::HistoryItem> DbusAppmenu::HistoryItemForTab(
    const sessions::TabRestoreService::Tab& entry) {
  const sessions::SerializedNavigationEntry& current_navigation =
      entry.navigations.at(entry.current_navigation_index);
  auto item = std::make_unique<HistoryItem>();
  item->title = current_navigation.title();
  item->url = current_navigation.virtual_url();
  item->session_id = entry.id;
  return item;
}

void DbusAppmenu::AddHistoryItemToMenu(std::unique_ptr<HistoryItem> item,
                                       ui::SimpleMenuModel* menu,
                                       int index) {
  std::u16string title = item->title;
  std::string url_string = item->url.possibly_invalid_spec();

  if (title.empty())
    title = base::UTF8ToUTF16(url_string);
  gfx::ElideString(title, kMaximumMenuWidthInChars, &title);

  int command_id = NextCommandId();
  menu->InsertItemAt(index, command_id, title);
  history_items_[command_id] = std::move(item);
}

void DbusAppmenu::AddEntryToHistoryMenu(
    SessionID id,
    std::u16string title,
    int index,
    const std::vector<std::unique_ptr<sessions::TabRestoreService::Tab>>&
        tabs) {
  // Create the item for the parent/window.
  auto item = std::make_unique<HistoryItem>();
  item->session_id = id;

  auto parent_menu = std::make_unique<ui::SimpleMenuModel>(this);
  int command = NextCommandId();
  history_menu_->InsertSubMenuAt(index, command, title, parent_menu.get());
  parent_menu->AddItemWithStringId(command,
                                   IDS_HISTORY_CLOSED_RESTORE_WINDOW_LINUX);
  parent_menu->AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);

  // Loop over the tabs and add them to the submenu.
  int subindex = 2;
  for (const auto& tab : tabs) {
    std::unique_ptr<HistoryItem> tab_item = HistoryItemForTab(*tab);
    item->tabs.push_back(tab_item.get());
    AddHistoryItemToMenu(std::move(tab_item), parent_menu.get(), subindex++);
  }

  history_items_[command] = std::move(item);
  recently_closed_window_menus_.push_back(std::move(parent_menu));
}

void DbusAppmenu::GetTopSitesData() {
  DCHECK(top_sites_);

  top_sites_->GetMostVisitedURLs(base::BindOnce(
      &DbusAppmenu::OnTopSitesReceived, weak_ptr_factory_.GetWeakPtr()));
}

void DbusAppmenu::OnTopSitesReceived(
    const history::MostVisitedURLList& visited_list) {
  int index = ClearHistoryMenuSection(kTagMostVisited);

  for (size_t i = 0; i < visited_list.size() && i < kMostVisitedCount; ++i) {
    const history::MostVisitedURL& visited = visited_list[i];
    if (visited.url.spec().empty())
      break;  // This is the signal that there are no more real visited sites.

    auto item = std::make_unique<HistoryItem>();
    item->title = visited.title;
    item->url = visited.url;

    AddHistoryItemToMenu(std::move(item), history_menu_, index++);
  }

  if (menu_service_)
    menu_service_->MenuLayoutUpdated(history_menu_);
}

void DbusAppmenu::OnBookmarkBarVisibilityChanged() {
  menu_service_->MenuItemsPropertiesUpdated(
      FindMenuItemsForCommand(root_menu_.get(), IDC_SHOW_BOOKMARK_BAR));
}

void DbusAppmenu::RebuildProfilesMenu() {
  while (profiles_menu_->GetTypeAt(0) != ui::MenuModel::TYPE_SEPARATOR)
    profiles_menu_->RemoveItemAt(0);
  profile_commands_.clear();

  // Don't call avatar_menu_->GetActiveProfileIndex() as the as the index might
  // be incorrect if RebuildProfilesMenu() is called while we deleting the
  // active profile and closing all its browser windows.
  active_profile_index_ = -1;
  for (size_t i = 0; i < avatar_menu_->GetNumberOfItems(); ++i) {
    const AvatarMenu::Item& item = avatar_menu_->GetItemAt(i);
    std::u16string title = item.name;
    gfx::ElideString(title, kMaximumMenuWidthInChars, &title);

    if (item.active)
      active_profile_index_ = i;

    int command = NextCommandId();
    profile_commands_[command] = i;
    profiles_menu_->InsertCheckItemAt(i, command, title);
  }

  if (menu_service_)
    menu_service_->MenuLayoutUpdated(profiles_menu_);
}

int DbusAppmenu::ClearHistoryMenuSection(int header_command_id) {
  int index = 0;
  while (history_menu_->GetCommandIdAt(index++) != header_command_id) {
  }
  while (history_menu_->GetTypeAt(index) != ui::MenuModel::TYPE_SEPARATOR) {
    history_items_.erase(history_menu_->GetCommandIdAt(index));
    history_menu_->RemoveItemAt(index);
  }
  return index;
}

void DbusAppmenu::RegisterCommandObserver(int command) {
  if (command > kLastChromeCommand)
    return;

  // Keep track of which commands are already registered to avoid
  // registering them twice.
  const bool inserted = observed_commands_.insert(command).second;
  if (!inserted)
    return;

  chrome::AddCommandObserver(browser_, command, this);
}

int DbusAppmenu::NextCommandId() {
  do {
    if (last_command_id_ == std::numeric_limits<int>::max())
      last_command_id_ = kFirstUnreservedCommandId;
    else
      last_command_id_++;
  } while (base::Contains(history_items_, last_command_id_) ||
           base::Contains(profile_commands_, last_command_id_));
  return last_command_id_;
}

void DbusAppmenu::OnAvatarMenuChanged(AvatarMenu* avatar_menu) {
  RebuildProfilesMenu();
}

void DbusAppmenu::OnBrowserSetLastActive(Browser* browser) {
  // Notify the avatar menu of the change and rebuild the menu. Note: The
  // ActiveBrowserChanged() call needs to happen first to update the state.
  avatar_menu_->ActiveBrowserChanged(browser);
  avatar_menu_->RebuildMenu();
  RebuildProfilesMenu();
}

void DbusAppmenu::EnabledStateChangedForCommand(int id, bool enabled) {
  menu_service_->MenuItemsPropertiesUpdated(
      FindMenuItemsForCommand(root_menu_.get(), id));
}

void DbusAppmenu::TopSitesLoaded(history::TopSites* top_sites) {}

void DbusAppmenu::TopSitesChanged(history::TopSites* top_sites,
                                  ChangeReason change_reason) {
  GetTopSitesData();
}

void DbusAppmenu::TabRestoreServiceChanged(
    sessions::TabRestoreService* service) {
  const sessions::TabRestoreService::Entries& entries = service->entries();

  int index = ClearHistoryMenuSection(kTagRecentlyClosed);
  recently_closed_window_menus_.clear();

  unsigned int added_count = 0;
  for (auto it = entries.begin();
       it != entries.end() && added_count < kRecentlyClosedCount; ++it) {
    sessions::TabRestoreService::Entry* entry = it->get();

    if (entry->type == sessions::TabRestoreService::WINDOW) {
      sessions::TabRestoreService::Window* window =
          static_cast<sessions::TabRestoreService::Window*>(entry);

      auto& tabs = window->tabs;
      if (tabs.empty())
        continue;

      std::u16string title = l10n_util::GetPluralStringFUTF16(
          IDS_RECENTLY_CLOSED_WINDOW, tabs.size());

      AddEntryToHistoryMenu(window->id, title, index++, tabs);
      ++added_count;
    } else if (entry->type == sessions::TabRestoreService::TAB) {
      sessions::TabRestoreService::Tab* tab =
          static_cast<sessions::TabRestoreService::Tab*>(entry);
      AddHistoryItemToMenu(HistoryItemForTab(*tab), history_menu_, index++);
      ++added_count;
    } else if (entry->type == sessions::TabRestoreService::GROUP) {
      sessions::TabRestoreService::Group* group =
          static_cast<sessions::TabRestoreService::Group*>(entry);

      auto& tabs = group->tabs;
      if (tabs.empty())
        continue;

      std::u16string title;
      if (group->visual_data.title().empty()) {
        title = l10n_util::GetPluralStringFUTF16(
            IDS_RECENTLY_CLOSED_GROUP_UNNAMED, tabs.size());
      } else {
        title = l10n_util::GetPluralStringFUTF16(IDS_RECENTLY_CLOSED_GROUP,
                                                 tabs.size());
        title = base::ReplaceStringPlaceholders(
            title, {group->visual_data.title()}, nullptr);
      }

      AddEntryToHistoryMenu(group->id, title, index++, tabs);
      ++added_count;
    }
  }

  menu_service_->MenuLayoutUpdated(history_menu_);
}

void DbusAppmenu::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  tab_restore_service_ = nullptr;
}

bool DbusAppmenu::IsCommandIdChecked(int command_id) const {
  if (command_id == IDC_SHOW_BOOKMARK_BAR) {
    return browser_->profile()->GetPrefs()->GetBoolean(
        bookmarks::prefs::kShowBookmarkBar);
  }

  auto it = profile_commands_.find(command_id);
  return it != profile_commands_.end() && it->second == active_profile_index_;
}

bool DbusAppmenu::IsCommandIdEnabled(int command_id) const {
  if (command_id <= kLastChromeCommand)
    return chrome::IsCommandEnabled(browser_, command_id);
  // There is no active profile in Guest mode, in which case the action
  // buttons should be disabled.
  if (command_id == kTagProfileEdit || command_id == kTagProfileCreate)
    return active_profile_index_ >= 0;
  return command_id != kTagRecentlyClosed && command_id != kTagMostVisited;
}

void DbusAppmenu::ExecuteCommand(int command_id, int event_flags) {
  if (command_id <= kLastChromeCommand) {
    chrome::ExecuteCommand(browser_, command_id);
  } else if (command_id == kTagProfileEdit) {
    avatar_menu_->EditProfile(active_profile_index_);
  } else if (command_id == kTagProfileCreate) {
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        ProfilePicker::EntryPoint::kProfileMenuAddNewProfile));
  } else if (base::Contains(history_items_, command_id)) {
    HistoryItem* item = history_items_[command_id].get();
    // If this item can be restored using TabRestoreService, do so.
    // Otherwise, just load the URL.
    sessions::TabRestoreService* service =
        TabRestoreServiceFactory::GetForProfile(profile_);
    if (item->session_id.is_valid() && service) {
      service->RestoreEntryById(browser_->live_tab_context(), item->session_id,
                                WindowOpenDisposition::UNKNOWN);
    } else {
      DCHECK(item->url.is_valid());
      browser_->OpenURL(
          content::OpenURLParams(item->url, content::Referrer(),
                                 WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                 ui::PAGE_TRANSITION_AUTO_BOOKMARK, false));
    }
  } else if (base::Contains(profile_commands_, command_id)) {
    avatar_menu_->SwitchToProfile(profile_commands_[command_id], false);
  }
}

void DbusAppmenu::OnMenuWillShow(ui::SimpleMenuModel* source) {
  if (source != history_menu_ || tab_restore_service_)
    return;

  tab_restore_service_ = TabRestoreServiceFactory::GetForProfile(profile_);
  if (!tab_restore_service_)
    return;

  tab_restore_service_->LoadTabsFromLastSession();
  tab_restore_service_->AddObserver(this);

  // If LoadTabsFromLastSession doesn't load tabs, it won't call
  // TabRestoreServiceChanged(). This ensures that all new windows after
  // the first one will have their menus populated correctly.
  TabRestoreServiceChanged(tab_restore_service_);
}

bool DbusAppmenu::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return browser_view_->GetAccelerator(command_id, accelerator);
}
