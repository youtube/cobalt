// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_APP_LIST_APP_LIST_TYPES_H_
#define ASH_PUBLIC_CPP_APP_LIST_APP_LIST_TYPES_H_

#include <ostream>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "components/sync/model/string_ordinal.h"
#include "components/sync/protocol/app_list_specifics.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/range/range.h"

namespace ash {

// The initial value of |profile_id_| in AppListControllerImpl.
constexpr int kAppListInvalidProfileID = -1;

// The value from which the unique profile id starts. Notice that this profile
// id is only used for mojo callings between AppListController and AppListClient
constexpr int kAppListProfileIdStartFrom = 0;

// The threshold of mouse drag event. The mouse event is treated as tapping if
// drag offset is smaller than the threshold.
constexpr int kMouseDragThreshold = 2;

// View group for launcher search result views that have a result set. Used
// primarily in browser tests to find shown search result views.
constexpr int kSearchResultViewGroup = 2;

// Id of OEM folder in app list.
ASH_PUBLIC_EXPORT extern const char kOemFolderId[];

// The AppListItem ID of the "Linux apps" folder.
ASH_PUBLIC_EXPORT extern const char kCrostiniFolderId[];

// App list config types supported by AppListConfig.
enum class AppListConfigType {
  // Config for tablet mode on typical size screens.
  kRegular,

  // Config for clamshell mode. Also used for tablet mode on small screens.
  kDense,
};

// Item types supported by SearchResultTextItem.
enum class SearchResultTextItemType {
  kString,         // Styled text.
  kIconCode,       // Built in vector icons.
  kCustomImage,    // Vector icons provided by the search model.
  kIconifiedText,  // Text to be styled like an icon e.g. "Alt", "Ctrl".
};

// A structure holding an item icon' color information.
class ASH_PUBLIC_EXPORT IconColor {
 public:
  // The minimum value of a valid hue. It is also the hue for the color that is
  // close or equal to pure white.
  static constexpr int kHueMin = -1;

  // The maximum value of a valid hue. It is also the hue for the color that is
  // close or equal to pure black.
  static constexpr int kHueMax = 360;

  static constexpr int kHueInvalid = kHueMin - 1;

  IconColor();
  IconColor(sync_pb::AppListSpecifics::ColorGroup background_color, int hue);
  IconColor(const IconColor&);
  IconColor& operator=(const IconColor&);
  ~IconColor();

  // The overloaded rational operators. NOTE: these operators assume that
  // operands are valid.
  bool operator<(const IconColor&) const;
  bool operator>(const IconColor&) const;
  bool operator>=(const IconColor&) const;
  bool operator<=(const IconColor&) const;
  bool operator==(const IconColor&) const;
  bool operator!=(const IconColor&) const;

  // Returns true only when all data members are valid: `background_color_` is
  // non-empty and `hue_` is in the valid range.
  bool IsValid() const;

  sync_pb::AppListSpecifics::ColorGroup background_color() const {
    return background_color_;
  }
  int hue() const { return hue_; }

 private:
  // Indicates an icon's background color.
  sync_pb::AppListSpecifics::ColorGroup background_color_ =
      sync_pb::AppListSpecifics::COLOR_EMPTY;

  // Indicates an icon's hue. NOTE: `hue_` falls in the range of [-1,360] that
  // is different from the normal range which is [0, 360). See the comments for
  // `kHueMin` and `kHueMax`.
  int hue_ = kHueInvalid;
};

// A structure holding the common information which is sent between ash and,
// chrome representing an app list item.
struct ASH_PUBLIC_EXPORT AppListItemMetadata {
  AppListItemMetadata();
  AppListItemMetadata(const AppListItemMetadata& rhs);
  ~AppListItemMetadata();

  std::string id;    // Id of the app list item.
  std::string name;  // Corresponding app/folder's name of the item.

  AppStatus app_status = AppStatus::kReady;  // App status.

  std::string folder_id;           // Id of folder where the item resides.
  syncer::StringOrdinal position;  // Position of the item.
  bool is_folder = false;          // Whether this item is a folder.

  // Whether the folder was system created (e.g. the OEM folder or Linux apps
  // folder). Historically (pre-2022) these folders were the only ones allowed
  // to contain a single item.
  bool is_system_folder = false;

  gfx::ImageSkia icon;                  // The icon of this item.
  SkColor badge_color = SK_ColorWHITE;  // Notification badge color.

  // Whether the app was installed this session and has not yet been launched.
  bool is_new_install = false;

  int icon_version = 0;  // An int represent icon version. If changed, `icon`
                         // should be reloaded.

  // The item's icon color.
  IconColor icon_color;

  // Whether the item is ephemeral - i.e. an app or a folder that does not
  // persist across sessions.
  bool is_ephemeral = false;
};

// Where an app list item is being shown. Used for context menu.
enum class AppListItemContext {
  // Used in tests when the context doesn't matter.
  kNone,
  // The apps grid (the common case).
  kAppsGrid,
  // Recent apps.
  kRecentApps,
};

// All possible orders to sort app list items.
// Note: Do not change the order of these as they are used for metrics.
enum class AppListSortOrder {
  // The sort order is not set.
  kCustom = 0,

  // Items are sorted by the name alphabetical order. Note that folders are
  // always placed in front of other types of items.
  kNameAlphabetical,

  // Items are sorted by the name reverse alphabetical order. Note that folders
  // are always placed in front of other types of items.
  kNameReverseAlphabetical,

  // Items are sorted in order of color in rainbow order from red to purple.
  // Items are first sorted by the color of the icon background, then sorted
  // by the light vibrant color extracted from the icon.
  kColor,

  // Ephemeral apps and folders are sorted first, in alphabetical order,
  // followed by the non-ephemeral apps and folders in alphabetical order.
  // Note that folders are also sorted by their name and not automatically added
  // to the front.
  kAlphabeticalEphemeralAppFirst,

  kMaxValue = kAlphabeticalEphemeralAppFirst,
};

// All the events that affect the app list sort order (including the pref order
// and the temporary order).
// NOTE: Do not change the order of these as they are used for metrics.
enum class AppListOrderUpdateEvent {
  // Add a new item.
  kItemAdded = 0,

  // Remove an item.
  kItemRemoved = 1,

  // An item is moved due to sync.
  kItemSyncMove = 2,

  // An item is moved to a folder.
  kItemMovedToFolder = 3,

  // An item is moved to the root apps grid.
  kItemMovedToRoot = 4,

  // Sort reversion is triggered.
  kRevert = 5,

  // An item is moved but its parent apps grid does not change.
  kItemMoved = 6,

  // A folder is created.
  kFolderCreated = 7,

  // A folder is renamed.
  kFolderRenamed = 8,

  // The app list is hidden.
  kAppListHidden = 9,

  // User requests to sort.
  kSortRequested = 10,

  kMaxValue = kSortRequested,
};

// Lists the reasons that ash requests for item position update.
enum class RequestPositionUpdateReason {
  // Fix the position when multiple items share the same position.
  kFixItem,

  // Move an item.
  kMoveItem
};

// All possible states of the app list.
// Note: Do not change the order of these as they are used for metrics.
enum class AppListState {
  kStateApps = 0,
  kStateSearchResults,
  kStateStart_DEPRECATED,
  kStateEmbeddedAssistant,
  // Add new values here.

  kInvalidState,               // Don't use over IPC
  kStateLast = kInvalidState,  // Don't use over IPC
};

ASH_PUBLIC_EXPORT std::ostream& operator<<(std::ostream& os,
                                           AppListState state);

// Sub-pages of the app list bubble.
enum class AppListBubblePage {
  // Used at startup and when the app list bubble is not visible. Allows
  // detection of transitions like hidden -> apps or hidden -> assistant,
  // avoiding unnecessary page hide animations.
  kNone = 0,
  // The apps grid, as well as continue tasks and recent apps.
  kApps,
  // The search page.
  kSearch,
  // The assistant page.
  kAssistant
};

// The type of the toast that shows on the app list.
enum class AppListToastType {
  // The toast container is not showing any toast.
  kNone,

  // Shows the nudge to guide the users to use apps reordering using context
  // menu.
  kReorderNudge,

  // Shows the notification that the apps are temporarily sorted and allows
  // users to undo the sorting actions.
  kReorderUndo,
};

ASH_PUBLIC_EXPORT std::ostream& operator<<(std::ostream& os,
                                           AppListBubblePage page);

// All possible states of the app list view.
enum class AppListViewState {
  // Closes |app_list_main_view_| and dismisses the delegate.
  kClosed,
  // Default app list state in maximize and side shelf modes.
  kFullscreenAllApps,
  // Entered by entering text in the search box.
  kFullscreenSearch
};

ASH_PUBLIC_EXPORT std::ostream& operator<<(std::ostream& os,
                                           AppListViewState state);

// The status of the app list model.
enum class AppListModelStatus {
  kStatusNormal,
  kStatusSyncing,  // Syncing apps or installing synced apps.
};

// Indicate the state of animations that affect the entire apps grid (e.g.
// reorder/sorting, hide continue section). This does not cover smaller
// animations (e.g. drag and drop, folder open).
enum class AppListGridAnimationStatus {
  // No whole-grid animation is active.
  kEmpty,

  // Run the animation that fades out the obsolete layout before reordering.
  kReorderFadeOut,

  // After the fade out animation ends and before the fade in animation starts.
  kReorderIntermediaryState,

  // Run the animation that fades in the new layout after reordering.
  kReorderFadeIn,

  // Run the animation that slides up each row of icons when the continue
  // section is hidden by the user.
  kHideContinueSection,
};

// The UI component the user launched the search result from.
// This enum is used in a histogram, do not remove/renumber entries. If you're
// adding to this enum with the intention that it will be logged, update the
// AppListLaunchedFrom enum listing in tools/metrics/histograms/enums.xml.
enum class AppListLaunchedFrom {
  kLaunchedFromGrid = 1,
  DEPRECATED_kLaunchedFromSuggestionChip = 2,
  kLaunchedFromShelf = 3,
  kLaunchedFromSearchBox = 4,
  kLaunchedFromRecentApps = 5,
  kLaunchedFromContinueTask = 6,
  kLaunchedFromQuickAppAccess = 7,
  kMaxValue = kLaunchedFromQuickAppAccess,
};

// The UI representation of the app that's being launched. Currently all search
// results that are not apps (OminboxResult, LauncherSearcResult, etc.) are
// grouped into kSearchResult. Meanwhile app search results, apps that appear in
// the recent apps section are considered kAppSearchResult. kApp is used for
// apps launched from the apps grid.
enum class AppListLaunchType { kSearchResult, kAppSearchResult, kApp };

// Type of the search result, which is set in Chrome.
//
// This should not be used for metrics. Please use ash::SearchResultType in
// ash/public/cpp/app_list/app_list_metrics.h instead.
enum class AppListSearchResultType {
  kUnknown,       // Unknown type. Don't use over IPC
  kInstalledApp,  // Installed apps.
  kPlayStoreApp,  // Installable apps from PlayStore.
  kInstantApp,    // Instant apps.
  kInternalApp,   // Chrome OS apps.
  kOmnibox,       // Results from Omnibox.
  kLauncher,      // Results from launcher search (currently only from Files).
  kAnswerCard,    // WebContents based answer card.
  kPlayStoreReinstallApp,  // Reinstall recommendations from PlayStore.
  kArcAppShortcut,         // ARC++ app shortcuts.
  kZeroStateFile,          // Zero state local file results.
  kZeroStateDrive,         // Drive QuickAccess results.
  kOsSettings,             // OS settings results.
  kInternalPrivacyInfo,    // Result used internally by privacy notices.
  kAssistantText,          // Assistant text results.
  kHelpApp,                // Help App (aka Explore) results.
  kFileSearch,             // Local file search results.
  kDriveSearch,            // Drive file search results.
  kKeyboardShortcut,       // Keyboard shortcut search results.
  kOpenTab,                // Open tab search results.
  kGames,                  // Game sarch results.
  kPersonalization,        // Personalization search results.
  kZeroStateHelpApp,       // Help App (aka Explore) results for zero-state.
  kZeroStateApp,           // App recommendations for zero-state / recent apps.
  kImageSearch,            // Local image search result.
  kSystemInfo,             // System Info search result.
  kDesksAdminTemplate,     // Admin templates search results.
  // Add new values here.
  kMaxValue = kDesksAdminTemplate,
};

ASH_PUBLIC_EXPORT bool IsAppListSearchResultAnApp(
    AppListSearchResultType result_type);

// Returns whether the result type is a type of result shown in launcher
// apps page, i.e. results shown in launcher "continue" section and among recent
// apps.
ASH_PUBLIC_EXPORT bool IsZeroStateResultType(
    AppListSearchResultType result_type);

// The different categories a search result can be part of. Every search result
// to be displayed in the search box should be associated with one category. It
// is an error for results displayed in the search box to have a kUnknown
// category, but results displayed in other views - eg. the Continue section -
// may use kUnknown.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AppListSearchResultCategory {
  kUnknown = 0,
  kApps = 1,
  kAppShortcuts = 2,
  kWeb = 3,
  kFiles = 4,
  kSettings = 5,
  kHelp = 6,
  kPlayStore = 7,
  kSearchAndAssistant = 8,
  kGames = 9,
  kMaxValue = kGames,
};

// Which UI container(s) the result should be displayed in.
// Do not change the order of these as they are used for metrics.
enum class SearchResultDisplayType {
  kNone = 0,
  kList = 1,  // Displays in search list
  // kTile = 2,  // No longer used, Displays in search tiles
  // kRecommendation = 3  // No longer used, split between kTile and kChip
  kAnswerCard = 4,  // Displays in answer cards
  // kChip = 5,        // No longer used, Displays in suggestion chips
  kContinue = 6,    // Displays in the Continue section
  kRecentApps = 7,  // Displays in recent apps row
  kImage = 8,       // Displays in a list of image results
  // Add new values here
  kLast,  // Don't use over IPC
};

// Actions for search results. These map to the buttons beside some search
// results, and do not include the launching of the result itself.
enum SearchResultActionType {
  // Removes the search result.
  kRemove,
};

// The shape to mask a search result icon with.
enum class SearchResultIconShape {
  kDefault,
  kRectangle,
  kRoundedRectangle,
  kCircle,
};

// The storage types that are available within the System Info Card storage type
// answer card.
enum class SearchResultSystemInfoStorageType {
  kMyFiles,
  kDriveOfflineFiles,
  kBrowsingData,
  kAppsExtensions,
  kCrostini,
  kOtherUsers,
  kSystem,
  kTotal
};

// The display type of the answer cards created by the System Info Provider. The
// Text Card provides a similar UI to the omnibox answer cards while the bar
// chart and multi element bar chart provide an additional bar chart with system
// information.
enum class SystemInfoAnswerCardDisplayType {
  kBarChart,
  kTextCard,
  kMultiElementBarChart,
};

struct ASH_PUBLIC_EXPORT SearchResultIconInfo {
  SearchResultIconInfo();
  // TODO(crbug.com/1232897): Make the search backend explicitly set the shape
  // for all icons by removing the two-argument version of the constructor.
  SearchResultIconInfo(gfx::ImageSkia icon, int dimension);
  SearchResultIconInfo(gfx::ImageSkia icon,
                       int dimension,
                       SearchResultIconShape shape);

  SearchResultIconInfo(const SearchResultIconInfo&);

  ~SearchResultIconInfo();

  // The icon itself.
  gfx::ImageSkia icon;

  // The size to display the icon at, while preserving aspect ratio. Only
  // used for the results list view.
  int dimension;

  // The shape to mask the icon with. Only used by the results list view.
  SearchResultIconShape shape = SearchResultIconShape::kDefault;
};

// Data required for System Info Answer Card result type.
struct ASH_PUBLIC_EXPORT SystemInfoAnswerCardData {
 public:
  SystemInfoAnswerCardData();
  explicit SystemInfoAnswerCardData(
      SystemInfoAnswerCardDisplayType display_type);
  explicit SystemInfoAnswerCardData(double bar_chart_percentage);
  explicit SystemInfoAnswerCardData(std::map<SearchResultSystemInfoStorageType,
                                             int64_t> storage_type_to_size);

  SystemInfoAnswerCardData(const SystemInfoAnswerCardData&);

  ~SystemInfoAnswerCardData();

  SystemInfoAnswerCardDisplayType display_type;

  // This stores the percentage of the bar chart to be filled for System Info
  // Answer card results which are a bar chart type. This will be a value
  // between 0 and 100.
  absl::optional<double> bar_chart_percentage;

  // This stores the amount of space occupied by each storage type if this
  // answer card is showing storage. All sizes are in bytes.
  std::map<SearchResultSystemInfoStorageType, int64_t> storage_type_to_size;
};

// A tagged range in search result text.
struct ASH_PUBLIC_EXPORT SearchResultTag {
  // Similar to ACMatchClassification::Style, the style values are not
  // mutually exclusive.
  enum Style {
    NONE = 0,
    URL = 1 << 0,
    MATCH = 1 << 1,
    DIM = 1 << 2,
    GREEN = 1 << 3,
    RED = 1 << 4,
  };

  SearchResultTag();
  SearchResultTag(int styles, uint32_t start, uint32_t end);

  int styles;
  gfx::Range range;
};
using SearchResultTags = std::vector<SearchResultTag>;

// Data representing an action that can be performed on this search result.
// An action could be represented as an icon set or as a blue button with
// a label. Icon set is chosen if label text is empty. Otherwise, a blue
// button with the label text will be used.
struct ASH_PUBLIC_EXPORT SearchResultAction {
  SearchResultAction();
  SearchResultAction(SearchResultActionType type,
                     const std::u16string& tooltip_text);
  SearchResultAction(const SearchResultAction& other);
  ~SearchResultAction();

  SearchResultActionType type;
  std::u16string tooltip_text;
};
using SearchResultActions = std::vector<SearchResultAction>;

// A structure holding a search result's text with support for embedded icon.
class ASH_PUBLIC_EXPORT SearchResultTextItem {
 public:
  enum IconCode {
    kKeyboardShortcutBrowserBack,
    kKeyboardShortcutBrowserForward,
    kKeyboardShortcutBrowserRefresh,
    kKeyboardShortcutZoom,
    kKeyboardShortcutMediaLaunchApp1,
    kKeyboardShortcutBrightnessDown,
    kKeyboardShortcutBrightnessUp,
    kKeyboardShortcutVolumeMute,
    kKeyboardShortcutVolumeDown,
    kKeyboardShortcutVolumeUp,
    kKeyboardShortcutUp,
    kKeyboardShortcutDown,
    kKeyboardShortcutLeft,
    kKeyboardShortcutRight,
    kKeyboardShortcutPrivacyScreenToggle,
    kKeyboardShortcutSnapshot,
  };

  // Only used for SearchResultTextItemType kString
  enum OverflowBehavior {
    kNoElide,  // Prioritize this text item for space allocation: do not elide.
    kElide,    // Elide this text item when there is not enough space.
    kHide,     // Completely hide this text item when there is not enough space.
  };

  explicit SearchResultTextItem(SearchResultTextItemType type);
  SearchResultTextItem(const SearchResultTextItem&);
  SearchResultTextItem& operator=(const SearchResultTextItem&);
  ~SearchResultTextItem();

  SearchResultTextItemType GetType() const;

  const std::u16string& GetText() const;
  SearchResultTextItem& SetText(std::u16string text);

  const SearchResultTags& GetTextTags() const;
  SearchResultTags& GetTextTags();
  SearchResultTextItem& SetTextTags(SearchResultTags tags);

  const gfx::VectorIcon* GetIconFromCode() const;
  SearchResultTextItem& SetIconCode(IconCode icon_code);

  gfx::ImageSkia GetImage() const;
  SearchResultTextItem& SetImage(gfx::ImageSkia icon);

  OverflowBehavior GetOverflowBehavior() const;
  SearchResultTextItem& SetOverflowBehavior(OverflowBehavior overflow_behavior);

 private:
  SearchResultTextItemType item_type_;
  // used for type SearchResultTextItemType::kString.
  absl::optional<std::u16string> raw_text_;
  absl::optional<SearchResultTags> text_tags_;
  // used for type SearchResultTextItemType::kIconCode.
  absl::optional<IconCode> icon_code_;
  // used for type SearchResultTextItemType::kCustomIcon.
  absl::optional<gfx::ImageSkia> raw_image_;
  // Behavior of the text item when there is not enough space to show it in the
  // UI. only applicable to SearchResultTextItemType::kString.
  OverflowBehavior overflow_behavior_ = kElide;
};

// A structure holding the common information which is sent from chrome to ash,
// representing a search result.
struct ASH_PUBLIC_EXPORT SearchResultMetadata {
  SearchResultMetadata();
  SearchResultMetadata(const SearchResultMetadata& rhs);
  ~SearchResultMetadata();

  // The id of the result.
  std::string id;

  // The title of the result, e.g. an app's name, an autocomplete query, etc.
  // TODO (crbug/1216097): deprecate title text.
  std::u16string title;

  // A detail string of this result.
  // TODO (crbug/1216097): deprecate details text.
  std::u16string details;

  // How the title matches the query. See the SearchResultTag section for more
  // details.
  // TODO (crbug/1216097): deprecate title_tags.
  std::vector<SearchResultTag> title_tags;

  // How the details match the query. See the SearchResultTag section for more
  // details.
  // TODO (crbug/1216097): deprecate details_tags.
  std::vector<SearchResultTag> details_tags;

  // The title of the result, e.g. an app's name, an autocomplete query, etc.
  // Supports embedded icons.
  std::vector<SearchResultTextItem> title_vector;

  // The details of the result, supports embedded icons.
  std::vector<SearchResultTextItem> details_vector;

  // Whether or not the title field can be split over multiple lines. UI
  // implementation does not support multiline if the title vector has more
  // than one text item, so if multiline_title is set then title_vector
  // cannot have more than one element.
  bool multiline_title = false;

  // Whether or not the details field can be split over multiple lines. UI
  // implementation does not support multiline if the details vector has more
  // than one text item, so if multiline_details is set then details_vector
  // cannot have more than one element.
  bool multiline_details = false;

  // Big title text to be displayed prominently on an answer card.
  std::vector<SearchResultTextItem> big_title_vector;

  // Superscript text for the big title on an answer card.
  std::vector<SearchResultTextItem> big_title_superscript_vector;

  // Text for keyboard shortcuts displayed below the title. Only used for
  // keyboard shortcut results.
  std::vector<SearchResultTextItem> keyboard_shortcut_vector;

  // Text to be announced by a screen reader app.
  std::u16string accessible_name;

  // Actions that can be performed on this result. See the SearchResultAction
  // section for more details.
  std::vector<SearchResultAction> actions;

  // The average rating score of the app corresponding to this result, ranged
  // from 0 to 5. It's negative if there's no rating for the result.
  float rating = -1.0;

  // A formatted price string, e.g. "$7.09", "HK$3.94", etc.
  std::u16string formatted_price;

  // Which category a search result is contained in within the search box. May
  // be kUnknown for results displayed in non-search-box views, eg. the Continue
  // section.
  AppListSearchResultCategory category = AppListSearchResultCategory::kUnknown;

  // Whether this result is a top match and should be shown in the Top Matches
  // section instead of its category.
  bool best_match = false;

  // The type of this result.
  AppListSearchResultType result_type = AppListSearchResultType::kUnknown;

  // A search result type used for metrics.
  ash::SearchResultType metrics_type = ash::SEARCH_RESULT_TYPE_BOUNDARY;

  // Which UI container(s) the result should be displayed in.
  SearchResultDisplayType display_type = SearchResultDisplayType::kList;

  // A score to determine the result display order.
  double display_score = 0;

  // Whether this is searched from Omnibox.
  bool is_omnibox_search = false;

  // Whether this result is a recommendation.
  bool is_recommendation = false;

  // Whether this result can have its update animation skipped.
  bool skip_update_animation = false;

  // The icon of this result.
  SearchResultIconInfo icon;

  // The details for an answer card result with System Information. This field
  // is only set for this specific result type.
  absl::optional<SystemInfoAnswerCardData> system_info_answer_card_data;

  // The icon of this result in a smaller dimension to be rendered in suggestion
  // chip view.
  // TODO(crbug.com/1225161): Remove this and replace it with |icon| and an
  // appropriately set |icon_dimension|.
  gfx::ImageSkia chip_icon;

  // The badge icon of this result that indicates its type, e.g. installable
  // from PlayStore, installable from WebStore, etc.
  ui::ImageModel badge_icon;

  // Flag indicating whether the `badge_icon` should be painted atop a circle
  // background image.
  bool use_badge_icon_background = false;
};

// A struct holding a search result id and its corresponding position index that
// was being shown to the user.
struct SearchResultIdWithPositionIndex {
  SearchResultIdWithPositionIndex(std::string result_id, int index)
      : id(result_id), position_index(index) {}

  // The id of the result.
  std::string id;

  // The position index of the result.
  int position_index;
};

// `ScopedIphSession` manages an IPH session. A UI must show an IPH once an
// IPH session gets created. Also the UI must destroy
// `ScopedIphSession` when it has stopped showing an IPH.
class ASH_PUBLIC_EXPORT ScopedIphSession {
 public:
  ScopedIphSession() = default;
  virtual ~ScopedIphSession() = default;

  ScopedIphSession(const ScopedIphSession&) = delete;
  ScopedIphSession& operator=(const ScopedIphSession&) = delete;

  // Notify an IPH event with name of `event`.
  virtual void NotifyEvent(const std::string& event) = 0;
};

using SearchResultIdWithPositionIndices =
    std::vector<SearchResultIdWithPositionIndex>;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_APP_LIST_APP_LIST_TYPES_H_
