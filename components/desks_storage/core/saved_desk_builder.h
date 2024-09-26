// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_SAVED_DESK_BUILDER_H_
#define COMPONENTS_DESKS_STORAGE_CORE_SAVED_DESK_BUILDER_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/uuid.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_info.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/sync/protocol/workspace_desk_specifics.pb.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

namespace ash {
class DeskTemplate;
enum class DeskTemplateSource;
enum class DeskTemplateType;
}  // namespace ash

namespace desks_storage {

// This structure represents an app that has been built by a builder.
// Both the window_info struct and the launch info struct are needed by the
// AppRestoreClass to add a new app.
struct BuiltApp {
  enum Status {
    kOk,
    // All apps must have a window Id.
    kNoWindowId
  };

  BuiltApp(Status status,
           std::unique_ptr<app_restore::WindowInfo> window_info,
           std::unique_ptr<app_restore::AppLaunchInfo> launch_info);
  BuiltApp(BuiltApp&&);
  BuiltApp& operator=(BuiltApp&&);
  ~BuiltApp();

  // Any BuiltApp that does not have status kOk should be discarded.
  Status status;
  std::unique_ptr<app_restore::WindowInfo> window_info;
  std::unique_ptr<app_restore::AppLaunchInfo> launch_info;
};

// Generic App Builder class that contains the definitions for the
// base fields that can be set for every app.  Chrome Apps, SWAs
// and other unknown types can be generated via this class.
class SavedDeskGenericAppBuilder {
 public:
  SavedDeskGenericAppBuilder();
  SavedDeskGenericAppBuilder(const SavedDeskGenericAppBuilder&) = delete;
  SavedDeskGenericAppBuilder& operator=(const SavedDeskGenericAppBuilder&) =
      delete;
  SavedDeskGenericAppBuilder(SavedDeskGenericAppBuilder&&);
  SavedDeskGenericAppBuilder& operator=(SavedDeskGenericAppBuilder&&);
  ~SavedDeskGenericAppBuilder();

  // This method builds a generic app and populates a BuiltApp struct with
  // initial fields.
  BuiltApp Build();

  // Setters and Adders.
  SavedDeskGenericAppBuilder& SetAppId(std::string app_id_);
  SavedDeskGenericAppBuilder& SetWindowBound(gfx::Rect bounds);
  SavedDeskGenericAppBuilder& SetWindowState(chromeos::WindowStateType state);
  SavedDeskGenericAppBuilder& SetPreMinimizedWindowState(
      ui::WindowShowState state);
  SavedDeskGenericAppBuilder& SetZIndex(int index);
  SavedDeskGenericAppBuilder& SetWindowId(int window_id);
  SavedDeskGenericAppBuilder& SetDisplayId(int64_t display_id);
  SavedDeskGenericAppBuilder& SetLaunchContainer(
      apps::LaunchContainer container);
  SavedDeskGenericAppBuilder& SetWindowOpenDisposition(
      WindowOpenDisposition disposition);
  SavedDeskGenericAppBuilder& SetName(std::string name);
  SavedDeskGenericAppBuilder& SetSnapPercentage(int percentage);

  // Apps are keyed by their ID, this returns the ID associated with this app
  // assuming it has been set.  There are some special app cases in which this
  // ID is a constant (i.e. a browser)  In the case where this is an app
  // with an assignable ID then a random one is assigned upon calling this
  // method.
  const std::string& GetAppId();

 private:
  absl::optional<std::string> app_id_;
  absl::optional<gfx::Rect> window_bounds_;
  absl::optional<chromeos::WindowStateType> window_show_state_;
  absl::optional<ui::WindowShowState> pre_minimized_window_show_state_;
  absl::optional<int> z_index_;
  absl::optional<int> window_id_;
  absl::optional<int64_t> display_id_;
  absl::optional<apps::LaunchContainer> launch_conatiner_;
  absl::optional<WindowOpenDisposition> disposition_;
  absl::optional<std::string> name_;
  absl::optional<int> snap_percentage_;
};

// Builder for TabGroups.  Each instance represents a single tab group.
class SavedDeskTabGroupBuilder {
 public:
  enum TabGroupBuildStatus {
    kOk,
    // All fields must be set to build a TabGroup.
    kNotAllFieldsSet
  };

  // Caller will have to release the TabGroup from the unique_ptr contained
  // within.  This will help facilitate moving this result object around after
  // calling build.
  struct TabGroupWithStatus {
    TabGroupWithStatus(TabGroupBuildStatus status,
                       std::unique_ptr<tab_groups::TabGroupInfo> tab_group);
    ~TabGroupWithStatus();

    TabGroupBuildStatus status;
    std::unique_ptr<tab_groups::TabGroupInfo> tab_group;
  };

  SavedDeskTabGroupBuilder();
  SavedDeskTabGroupBuilder(const SavedDeskTabGroupBuilder&) = delete;
  SavedDeskTabGroupBuilder& operator=(const SavedDeskTabGroupBuilder&) = delete;
  SavedDeskTabGroupBuilder(SavedDeskTabGroupBuilder&&);
  ~SavedDeskTabGroupBuilder();

  // Setters and adders.
  SavedDeskTabGroupBuilder& SetRange(gfx::Range range);
  SavedDeskTabGroupBuilder& SetTitle(std::string title);
  SavedDeskTabGroupBuilder& SetColor(tab_groups::TabGroupColorId color);
  SavedDeskTabGroupBuilder& SetIsCollapsed(bool is_collapsed);

  // Returns a TabGroupWithStatus, pointer to the tab group will be nullptr
  // unless all fields associated with this struct are set.
  TabGroupWithStatus Build();

 private:
  absl::optional<gfx::Range> range_;
  absl::optional<std::string> title_;
  absl::optional<tab_groups::TabGroupColorId> color_;
  absl::optional<bool> is_collapsed_;
};

// Builder that constructs Browser representations, this can also be used to
// create PWAs.  To construct a PWA only use a single URL in the URLs field
// and set IsApp to true.  Requires a GenericAppBuilder with `window_id` set
// in order to correctly build.
class SavedDeskBrowserBuilder {
 public:
  SavedDeskBrowserBuilder();
  SavedDeskBrowserBuilder(const SavedDeskBrowserBuilder&) = delete;
  SavedDeskBrowserBuilder& operator=(const SavedDeskBrowserBuilder&) = delete;
  ~SavedDeskBrowserBuilder();

  BuiltApp Build();

  // Compose a generic builder to build with altered generic traits.
  SavedDeskBrowserBuilder& SetGenericBuilder(
      SavedDeskGenericAppBuilder& generic_builder);

  // setters and adders.
  SavedDeskBrowserBuilder& SetFirstNonPinnedTabIndex(int index);
  SavedDeskBrowserBuilder& SetActiveTabIndex(int index);
  SavedDeskBrowserBuilder& SetUrls(std::vector<GURL> urls);
  SavedDeskBrowserBuilder& SetIsLacros(bool is_lacros);
  SavedDeskBrowserBuilder& AddTabGroupBuilder(
      SavedDeskTabGroupBuilder tab_group);

  // Indicates that this browser instance is a PWA.
  SavedDeskBrowserBuilder& SetIsApp(bool is_app);

  const std::string& GetAppId();

 private:
  SavedDeskGenericAppBuilder generic_builder_;

  bool is_lacros_ = false;
  absl::optional<bool> is_app_;
  std::vector<SavedDeskTabGroupBuilder> tab_group_builders_;
  absl::optional<int> active_tab_index_;
  absl::optional<int> first_non_pinned_tab_index_;
  std::vector<GURL> urls_;
};

// Builder that constructs arc apps.  Requires a GenericAppBuilder with the
// `window_id` field set to construct properly.
class SavedDeskArcAppBuilder {
 public:
  SavedDeskArcAppBuilder();
  SavedDeskArcAppBuilder(const SavedDeskArcAppBuilder&) = delete;
  SavedDeskArcAppBuilder operator=(const SavedDeskArcAppBuilder&) = delete;
  ~SavedDeskArcAppBuilder();

  BuiltApp Build();

  // set composed generic builder for setting generic traits.
  SavedDeskArcAppBuilder& SetGenericBuilder(
      SavedDeskGenericAppBuilder& generic_builder);

  // setters and adders.
  SavedDeskArcAppBuilder& SetAppId(std::string app_id);
  SavedDeskArcAppBuilder& SetMinimumSize(gfx::Size size);
  SavedDeskArcAppBuilder& SetMaximumSize(gfx::Size size);
  SavedDeskArcAppBuilder& SetBoundsInRoot(gfx::Rect bounds_in_root);

 private:
  SavedDeskGenericAppBuilder generic_builder_;

  absl::optional<std::string> app_id_;
  absl::optional<gfx::Size> minimum_size_;
  absl::optional<gfx::Size> maximum_size_;
  absl::optional<gfx::Rect> bounds_in_root_;
};

// Helper class for building a saved desk for test.
class SavedDeskBuilder {
 public:
  SavedDeskBuilder();
  SavedDeskBuilder(const SavedDeskBuilder&) = delete;
  SavedDeskBuilder& operator=(const SavedDeskBuilder&) = delete;
  ~SavedDeskBuilder();

  // Builds a saved desk. This should only be called once per builder
  // instance.
  std::unique_ptr<ash::DeskTemplate> Build();

  // Sets saved desk UUID. If not set, the built desk will have a random UUID.
  SavedDeskBuilder& SetUuid(const std::string& uuid);

  // Sets saved desk name. If not set, the built desk will have a fixed name.
  SavedDeskBuilder& SetName(const std::string& name);

  // Sets saved desk type. If not set, the built desk will default to
  // DeskTemplate.
  SavedDeskBuilder& SetType(ash::DeskTemplateType desk_type);

  // Sets saved desk source. If not set, the built desk will default to kUser.
  SavedDeskBuilder& SetSource(ash::DeskTemplateSource desk_source);

  // Sets saved desk creation timestamp. If not set, the built desk will have
  // its creation timestamp set at the creation time of the SavedDeskBuilder.
  SavedDeskBuilder& SetCreatedTime(base::Time& created_time);

  // Adds an app window.
  SavedDeskBuilder& AddAppWindow(BuiltApp built_app);

 private:
  base::Uuid desk_uuid_;
  std::string desk_name_;
  ash::DeskTemplateSource desk_source_;
  ash::DeskTemplateType desk_type_;
  base::Time created_time_;
  std::vector<BuiltApp> built_apps_;
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_SAVED_DESK_BUILDER_H_
