// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_CONTROLLER_IMPL_H_
#define ASH_WALLPAPER_WALLPAPER_CONTROLLER_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/image_downloader.h"
#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/public/cpp/wallpaper/google_photos_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/shell_observer.h"
#include "ash/system/scheduled_feature/scheduled_feature.h"
#include "ash/wallpaper/online_wallpaper_variant_info_fetcher.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_calculated_colors.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-forward.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/timer/wall_clock_timer.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/user_manager/user_type.h"
#include "ui/compositor/compositor_lock.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace color_utils {
struct ColorProfile;
}  // namespace color_utils

namespace ash {

class WallpaperColorCalculator;
class WallpaperDriveFsDelegate;
class WallpaperImageDownloader;
class WallpaperMetricsManager;
class WallpaperPrefManager;
class WallpaperResizer;
class WallpaperWindowStateManager;

// The |CustomWallpaperElement| contains |first| the path of the image which
// is currently being loaded and or in progress of being loaded and |second|
// the image itself.
using CustomWallpaperElement = std::pair<base::FilePath, gfx::ImageSkia>;
using CustomWallpaperMap = std::map<AccountId, CustomWallpaperElement>;

// Controls the desktop background wallpaper:
//   - Sets a wallpaper image and layout;
//   - Handles display change (add/remove display, configuration change etc);
//   - Calculates prominent colors.
//   - Move wallpaper to locked container(s) when session state is not ACTIVE to
//     hide the user desktop and move it to unlocked container when session
//     state is ACTIVE;
class ASH_EXPORT WallpaperControllerImpl
    : public WallpaperController,
      public WindowTreeHostManager::Observer,
      public ShellObserver,
      public SessionObserver,
      public TabletModeObserver,
      public OverviewObserver,
      public ui::CompositorLockClient,
      public ui::NativeThemeObserver,
      public ScheduledFeature::CheckpointObserver {
 public:
  // Directory names of custom wallpapers.
  static const char kSmallWallpaperSubDir[];
  static const char kLargeWallpaperSubDir[];
  static const char kOriginalWallpaperSubDir[];

  static std::unique_ptr<WallpaperControllerImpl> Create(
      PrefService* local_state);

  static void SetWallpaperPrefManagerForTesting(
      std::unique_ptr<WallpaperPrefManager> pref_manager);

  static void SetWallpaperImageDownloaderForTesting(
      std::unique_ptr<WallpaperImageDownloader> image_downloader);

  // Prefer to use `Create` to obtain an new instance unless injecting
  // non-production members i.e. in tests.
  explicit WallpaperControllerImpl(
      std::unique_ptr<WallpaperPrefManager> pref_manager,
      std::unique_ptr<OnlineWallpaperVariantInfoFetcher> fetcher,
      std::unique_ptr<WallpaperImageDownloader> image_downloader);

  WallpaperControllerImpl(const WallpaperControllerImpl&) = delete;
  WallpaperControllerImpl& operator=(const WallpaperControllerImpl&) = delete;

  ~WallpaperControllerImpl() override;

  // Returns custom wallpaper path. Appends |sub_dir|, |wallpaper_files_id| and
  // |file_name| to custom wallpaper directory.
  static base::FilePath GetCustomWallpaperPath(
      const std::string& sub_dir,
      const std::string& wallpaper_files_id,
      const std::string& file_name);

  // Returns custom wallpaper directory by appending corresponding |sub_dir|.
  static base::FilePath GetCustomWallpaperDir(const std::string& sub_dir);

  // Returns the prominent color based on |color_profile|.
  SkColor GetProminentColor(color_utils::ColorProfile color_profile) const;

  // Returns the k mean color of the current wallpaper.
  SkColor GetKMeanColor() const;

  // Returns the set of calculated colors. If the colors have not yet been
  // calculated yet, returns an empty object.
  const absl::optional<WallpaperCalculatedColors>& calculated_colors() const {
    return calculated_colors_;
  }

  // Returns current image on the wallpaper, or an empty image if there's no
  // wallpaper.
  gfx::ImageSkia GetWallpaper() const;

  // Returns the layout of the current wallpaper, or an invalid value if there's
  // no wallpaper.
  WallpaperLayout GetWallpaperLayout() const;

  // Returns the type of the current wallpaper, or an invalid value if there's
  // no wallpaper.
  WallpaperType GetWallpaperType() const;

  base::TimeDelta animation_duration() const { return animation_duration_; }

  // Returns true if the slower initial animation should be shown (as opposed to
  // the faster animation that's used e.g. when switching between different
  // wallpapers at login screen).
  bool ShouldShowInitialAnimation();

  // Returns true if the active user is allowed to open the wallpaper picker.
  bool CanOpenWallpaperPicker();

  // Returns whether any wallpaper has been shown. It returns false before the
  // first wallpaper is set (which happens momentarily after startup), and will
  // always return true thereafter.
  bool HasShownAnyWallpaper() const;

  // Exit wallpaper preview state if it is open and do nothing if it is not
  // open.
  void MaybeClosePreviewWallpaper();

  // Shows the wallpaper and alerts observers of changes.
  // Does not show the image if:
  // 1)  |preview_mode| is false and the current wallpaper is still being
  //     previewed. See comments for |confirm_preview_wallpaper_callback_|.
  // 2)  |is_override| is false but the current wallpaper is overridden.
  void ShowWallpaperImage(const gfx::ImageSkia& image,
                          WallpaperInfo info,
                          bool preview_mode,
                          bool is_override);

  // Update the blurred state of the current wallpaper for lock screen. Applies
  // blur if |blur| is true and blur is allowed by the controller, otherwise any
  // existing blur is removed.
  void UpdateWallpaperBlurForLockState(bool blur);

  // Restores the wallpaper blur from lock state.
  void RestoreWallpaperBlurForLockState(float blur);

  // A shield should be applied on the wallpaper for overview, login, lock, OOBE
  // and add user screens.
  bool ShouldApplyShield() const;

  // Returns whether the current wallpaper is allowed to be blurred on
  // lock/login screen. See https://crbug.com/775591.
  bool IsBlurAllowedForLockState() const;

  // True if the wallpaper is set.
  bool is_wallpaper_set() const { return !!current_wallpaper_.get(); }

  // Sets wallpaper info for |account_id| and saves it to local state if the
  // user is not ephemeral. Returns false if it fails (which happens if local
  // state is not available).
  bool SetUserWallpaperInfo(const AccountId& account_id,
                            const WallpaperInfo& info);
  // Overload for |SetUserWallpaperInfo| that allow callers to specify
  // whether |account_id| is ephemeral. Used for callers before signin has
  // occurred and |is_ephemeral| cannot be determined by session controller.
  bool SetUserWallpaperInfo(const AccountId& account_id,
                            bool is_ephemeral,
                            const WallpaperInfo& info);

  // Gets encoded wallpaper from cache. Returns true if success.
  bool GetWallpaperFromCache(const AccountId& account_id,
                             gfx::ImageSkia* image);

  // Gets path of encoded wallpaper from cache. Returns true if success.
  bool GetPathFromCache(const AccountId& account_id, base::FilePath* path);

  // Runs |callback| upon the completion of the first wallpaper animation that's
  // shown on |window|'s root window.
  void AddFirstWallpaperAnimationEndCallback(base::OnceClosure callback,
                                             aura::Window* window);

  // A wrapper of |ReadAndDecodeWallpaper| used in |SetWallpaperFromPath|.
  void StartDecodeFromPath(const AccountId& account_id,
                           const user_manager::UserType user_type,
                           const WallpaperInfo& info,
                           bool show_wallpaper,
                           const base::FilePath& wallpaper_path);

  // Returns false when the color extraction algorithm shouldn't be run based on
  // system state (e.g. wallpaper image, SessionState, etc.).
  bool ShouldCalculateColors() const;

  // WallpaperController:
  void SetClient(WallpaperControllerClient* client) override;
  WallpaperDragDropDelegate* GetDragDropDelegate() override;
  void SetDragDropDelegate(
      std::unique_ptr<WallpaperDragDropDelegate> delegate) override;
  void SetDriveFsDelegate(
      std::unique_ptr<WallpaperDriveFsDelegate> drivefs_delegate) override;
  void Init(const base::FilePath& user_data,
            const base::FilePath& wallpapers,
            const base::FilePath& custom_wallpapers,
            const base::FilePath& device_policy_wallpaper) override;
  bool CanSetUserWallpaper(const AccountId& account_id) const override;
  void SetCustomWallpaper(const AccountId& account_id,
                          const base::FilePath& file_path,
                          WallpaperLayout layout,
                          bool preview_mode,
                          SetWallpaperCallback callback) override;
  void SetDecodedCustomWallpaper(const AccountId& account_id,
                                 const std::string& file_name,
                                 WallpaperLayout layout,
                                 bool preview_mode,
                                 SetWallpaperCallback callback,
                                 const std::string& file_path,
                                 const gfx::ImageSkia& image) override;
  void SetOnlineWallpaper(const OnlineWallpaperParams& params,
                          SetWallpaperCallback callback) override;
  void SetGooglePhotosWallpaper(const GooglePhotosWallpaperParams& params,
                                SetWallpaperCallback callback) override;
  void SetGooglePhotosDailyRefreshAlbumId(const AccountId& account_id,
                                          const std::string& album_id) override;
  std::string GetGooglePhotosDailyRefreshAlbumId(
      const AccountId& account_id) const override;
  bool SetDailyGooglePhotosWallpaperIdCache(
      const AccountId& account_id,
      const DailyGooglePhotosIdCache& ids) override;
  bool GetDailyGooglePhotosWallpaperIdCache(
      const AccountId& account_id,
      DailyGooglePhotosIdCache& ids_out) const override;

  void SetDefaultWallpaper(const AccountId& account_id,
                           bool show_wallpaper,
                           SetWallpaperCallback callback) override;
  base::FilePath GetDefaultWallpaperPath(
      user_manager::UserType user_type) override;
  void SetCustomizedDefaultWallpaperPaths(
      const base::FilePath& customized_default_small_path,
      const base::FilePath& customized_default_large_path) override;
  void SetPolicyWallpaper(const AccountId& account_id,
                          user_manager::UserType user_type,
                          const std::string& data) override;
  void SetDevicePolicyWallpaperPath(
      const base::FilePath& device_policy_wallpaper_path) override;
  bool SetThirdPartyWallpaper(const AccountId& account_id,
                              const std::string& file_name,
                              WallpaperLayout layout,
                              const gfx::ImageSkia& image) override;
  void ConfirmPreviewWallpaper() override;
  void CancelPreviewWallpaper() override;
  void UpdateCurrentWallpaperLayout(const AccountId& account_id,
                                    WallpaperLayout layout) override;
  void ShowUserWallpaper(const AccountId& account_id) override;
  void ShowUserWallpaper(const AccountId& account_id,
                         const user_manager::UserType user_type) override;
  void ShowSigninWallpaper() override;
  void ShowOneShotWallpaper(const gfx::ImageSkia& image) override;
  void ShowOverrideWallpaper(const base::FilePath& image_path,
                             bool always_on_top) override;
  void RemoveOverrideWallpaper() override;
  void RemoveUserWallpaper(const AccountId& account_id,
                           base::OnceClosure on_removed) override;
  void RemovePolicyWallpaper(const AccountId& account_id) override;
  void SetAnimationDuration(base::TimeDelta animation_duration) override;
  void OpenWallpaperPickerIfAllowed() override;
  void MinimizeInactiveWindows(const std::string& user_id_hash) override;
  void RestoreMinimizedWindows(const std::string& user_id_hash) override;
  void AddObserver(WallpaperControllerObserver* observer) override;
  void RemoveObserver(WallpaperControllerObserver* observer) override;
  gfx::ImageSkia GetWallpaperImage() override;
  scoped_refptr<base::RefCountedMemory> GetPreviewImage() override;
  bool IsWallpaperBlurredForLockState() const override;
  bool IsActiveUserWallpaperControlledByPolicy() override;
  bool IsWallpaperControlledByPolicy(
      const AccountId& account_id) const override;
  absl::optional<WallpaperInfo> GetActiveUserWallpaperInfo() const override;
  bool ShouldShowWallpaperSetting() override;
  void SetDailyRefreshCollectionId(const AccountId& account_id,
                                   const std::string& collection_id) override;
  std::string GetDailyRefreshCollectionId(
      const AccountId& account_id) const override;
  void UpdateDailyRefreshWallpaper(
      RefreshWallpaperCallback callback = base::DoNothing()) override;
  void SyncLocalAndRemotePrefs(const AccountId& account_id) override;

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // ShellObserver:
  void OnRootWindowAdded(aura::Window* root_window) override;
  void OnShellInitialized() override;
  void OnShellDestroying() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // ScheduledFeature::CheckpointObserver:
  void OnCheckpointChanged(const ScheduledFeature* src,
                           const ScheduleCheckpoint new_checkpoint) override;

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // OverviewObserver:
  void OnOverviewModeWillStart() override;
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnded() override;

  // CompositorLockClient:
  void CompositorLockTimedOut() override;

  // Shows a default wallpaper for testing, without changing users' wallpaper
  // info.
  void ShowDefaultWallpaperForTesting();

  // Creates an empty wallpaper. Some tests require a wallpaper widget is ready
  // when running. However, the wallpaper widgets are created asynchronously. If
  // loading a real wallpaper, there are cases that these tests crash because
  // the required widget is not ready. This function synchronously creates an
  // empty widget for those tests to prevent crashes.
  void CreateEmptyWallpaperForTesting();

  void set_wallpaper_reload_no_delay_for_test() {
    wallpaper_reload_delay_ = base::Milliseconds(0);
  }

  // Proxy to private ReloadWallpaper().
  void ReloadWallpaperForTesting(bool clear_cache);

  // Needed when logoff is simulated in testing.
  void ClearPrefChangeObserverForTesting();

  void set_bypass_decode_for_testing() { bypass_decode_for_testing_ = true; }

  void set_allow_blur_or_shield_for_testing() {
    allow_blur_or_shield_for_testing_ = true;
  }

  // Exposed for testing.
  void UpdateDailyRefreshWallpaperForTesting();
  base::WallClockTimer& GetUpdateWallpaperTimerForTesting();

  WallpaperDriveFsDelegate* drivefs_delegate_for_testing() {
    return drivefs_delegate_.get();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(WallpaperControllerTest, BasicReparenting);
  FRIEND_TEST_ALL_PREFIXES(WallpaperControllerTest,
                           WallpaperMovementDuringUnlock);
  friend class WallpaperControllerTest;
  friend class WallpaperControllerTestApi;
  friend class KeyboardBacklightColorControllerTest;

  enum WallpaperMode { WALLPAPER_NONE, WALLPAPER_IMAGE };

  // Cached default wallpaper image and file path. The file path can be used to
  // check if the image is outdated (i.e. when there's a new default wallpaper).
  struct CachedDefaultWallpaper {
    gfx::ImageSkia image;
    base::FilePath file_path;
  };

  // Callback after `WallpaperResizer` is done scaling the current wallpaper to
  // the current display size.
  void OnWallpaperResized();

  // Gets wallpaper info of |account_id| from local state, or memory if the user
  // is ephemeral. Returns false if wallpaper info is not found.
  bool GetUserWallpaperInfo(const AccountId& account_id,
                            WallpaperInfo* info) const;

  // Update a Wallpaper for |root_window|.
  void UpdateWallpaperForRootWindow(aura::Window* root_window,
                                    bool lock_state_changed,
                                    bool new_root);

  // Update a Wallpaper for all root windows.
  void UpdateWallpaperForAllRootWindows(bool lock_state_changed);

  // Moves the wallpaper to the specified container across all root windows.
  // Returns true if a wallpaper moved.
  bool ReparentWallpaper(int container);

  // Returns the wallpaper container id for unlocked and locked states.
  int GetWallpaperContainerId(bool locked);

  // Implementation of |RemoveUserWallpaper|, which deletes |account_id|'s
  // custom wallpapers and directories.
  void RemoveUserWallpaperImpl(const AccountId& account_id,
                               base::OnceClosure on_removed);

  void RemoveUserWallpaperImplWithFilesId(
      const AccountId& account_id,
      base::OnceClosure on_removed,
      const std::string& wallpaper_files_id);

  // Implementation of |SetDefaultWallpaper|. Sets wallpaper to default if
  // |show_wallpaper| is true. Otherwise just save the defaut wallpaper to
  // cache.
  void SetDefaultWallpaperImpl(user_manager::UserType user_type,
                               bool show_wallpaper,
                               SetWallpaperCallback callback);

  // Returns true if the specified wallpaper is already stored in
  // |current_wallpaper_|. If |compare_layouts| is false, layout is ignored.
  bool WallpaperIsAlreadyLoaded(const gfx::ImageSkia& image,
                                bool compare_layouts,
                                WallpaperLayout layout) const;

  // Reads image from |file_path| on disk, and calls |OnWallpaperDataRead|
  // with the result of |ReadFileToString|.
  void ReadAndDecodeWallpaper(image_util::DecodeImageCallback callback,
                              const base::FilePath& file_path);

  // Sets wallpaper info for the user to default and saves it to local
  // state the user is not ephemeral. Returns false if this fails.
  bool SetDefaultWallpaperInfo(const AccountId& account_id,
                               const base::Time& date);

  // Used as the callback of checking `WallpaperType::kOnline` wallpaper
  // existence in `SetOnlineWallpaper`. Initiates reading and decoding
  // the wallpaper if `file_path` is not empty.
  void SetOnlineWallpaperFromPath(SetWallpaperCallback callback,
                                  const OnlineWallpaperParams& params,
                                  const base::FilePath& file_path);

  // Used as the callback of checking that all the wallpaper variants' paths
  // exist. If they do, set the online wallpaper from the given |params.url|.
  void SetOnlineWallpaperFromVariantPaths(
      SetWallpaperCallback callback,
      const OnlineWallpaperParams& params,
      const base::flat_map<std::string, base::FilePath>& url_to_file_path_map);

  // Handler to receive Fetch*Wallpaper variants callbacks.
  void OnWallpaperVariantsFetched(WallpaperType type,
                                  bool start_daily_refresh_timer,
                                  SetWallpaperCallback callback,
                                  absl::optional<OnlineWallpaperParams> params);

  // Used as the callback of decoding wallpapers of type
  // `WallpaperType::kOnline`. Saves the image to local file if `save_file` is
  // true, and shows the wallpaper immediately if `params.account_id` is the
  // active user.
  void OnOnlineWallpaperDecoded(const OnlineWallpaperParams& params,
                                bool save_file,
                                SetWallpaperCallback callback,
                                const gfx::ImageSkia& image);

  // Used as the callback of fetching the data for a Google Photos photo from
  // the unique id.
  void OnGooglePhotosPhotoFetched(
      GooglePhotosWallpaperParams params,
      SetWallpaperCallback callback,
      ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
      bool success);

  void OnDailyGooglePhotosPhotoFetched(
      const AccountId& account_id,
      const std::string& album_id,
      RefreshWallpaperCallback callback,
      ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
      bool success);

  void OnDailyGooglePhotosWallpaperDownloaded(
      const AccountId& account_id,
      const std::string& photo_id,
      const std::string& album_id,
      absl::optional<std::string> dedup_key,
      RefreshWallpaperCallback callback,
      const gfx::ImageSkia& image);

  void GetGooglePhotosWallpaperFromCacheOrDownload(
      const GooglePhotosWallpaperParams& params,
      ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
      SetWallpaperCallback callback,
      const base::FilePath& cached_path,
      bool cached_path_exists);

  void OnGooglePhotosWallpaperDecoded(const WallpaperInfo& info,
                                      const AccountId& account_id,
                                      const base::FilePath& path,
                                      SetWallpaperCallback callback,
                                      const gfx::ImageSkia& image);

  void OnGooglePhotosAuthenticationTokenFetched(
      ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
      const AccountId& account_id,
      ImageDownloader::DownloadCallback callback,
      const absl::optional<std::string>& access_token);

  // Used as the callback of downloading wallpapers of type
  // `WallpaperType::kOnceGooglePhotos`. Shows the wallpaper immediately if
  // `params.account_id` is the active user.
  void OnGooglePhotosWallpaperDownloaded(
      const GooglePhotosWallpaperParams& params,
      SetWallpaperCallback callback,
      const gfx::ImageSkia& image);

  // Sets the current wallpaper to the Google Photos photo specified by `info`
  // and updates the Google Photos cache to contain only `image`. Shows the
  // wallpaper on screen if `show_wallpaper` is true.
  void SetGooglePhotosWallpaperAndUpdateCache(const AccountId& account_id,
                                              const WallpaperInfo& info,
                                              const gfx::ImageSkia& image,
                                              bool show_wallpaper);

  // Implementation of |SetOnlineWallpaper|. Shows the wallpaper on screen if
  // |show_wallpaper| is true.
  void SetOnlineWallpaperImpl(const OnlineWallpaperParams& params,
                              const gfx::ImageSkia& image,
                              bool show_wallpaper);

  // Decodes |account_id|'s wallpaper. Shows the decoded wallpaper if
  // |show_wallpaper| is true.
  void SetWallpaperFromInfo(const AccountId& account_id,
                            const WallpaperInfo& info,
                            bool show_wallpaper);

  // Used as the callback of default wallpaper decoding. Sets default wallpaper
  // to be the decoded image, and shows the wallpaper now if |show_wallpaper|
  // is true.
  void OnDefaultWallpaperDecoded(const base::FilePath& path,
                                 WallpaperLayout layout,
                                 bool show_wallpaper,
                                 SetWallpaperCallback callback,
                                 const gfx::ImageSkia& image);

  // Saves |image| to disk if the user's data is not ephemeral, or if it is a
  // policy wallpaper for public accounts. Shows the wallpaper immediately if
  // |show_wallpaper| is true, otherwise only sets the wallpaper info and
  // updates the cache.
  void SaveAndSetWallpaper(const AccountId& account_id,
                           bool is_ephemeral,
                           const std::string& file_name,
                           const std::string& file_path,
                           WallpaperType type,
                           WallpaperLayout layout,
                           bool show_wallpaper,
                           const gfx::ImageSkia& image);

  // |image_saved| is only called on success.
  void SaveAndSetWallpaperWithCompletion(
      const AccountId& account_id,
      bool is_ephemeral,
      const std::string& file_name,
      const std::string& file_path,
      WallpaperType type,
      WallpaperLayout layout,
      bool show_wallpaper,
      const gfx::ImageSkia& image,
      base::OnceCallback<void(const base::FilePath&)> image_saved_callback);

  void SaveAndSetWallpaperWithCompletionFilesId(
      const AccountId& account_id,
      bool is_ephemeral,
      const std::string& file_name,
      const std::string& file_path,
      WallpaperType type,
      WallpaperLayout layout,
      bool show_wallpaper,
      const gfx::ImageSkia& image,
      base::OnceCallback<void(const base::FilePath&)> image_saved_callback,
      const std::string& wallpaper_files_id);

  // Used as the callback of wallpaper decoding. (Wallpapers of type
  // `WallpaperType::kOnline`, `WallpaperType::kDefault`,
  // `WallpaperType::kCustom`, and `Wallpapertype::kDevice` should use their
  // corresponding `*Decoded`, and all other types should use this.) Shows the
  // wallpaper immediately if `show_wallpaper` is true. Otherwise, only updates
  // the cache.
  void OnWallpaperDecoded(const AccountId& account_id,
                          const base::FilePath& path,
                          const WallpaperInfo& info,
                          bool show_wallpaper,
                          const gfx::ImageSkia& image);

  // Reloads the current wallpaper. It may change the wallpaper size based
  // on the current display's resolution. If |clear_cache| is true, all
  // wallpaper cache should be cleared. This is required when the display's
  // native resolution changes to a larger resolution (e.g. when hooked up a
  // large external display) and we need to load a larger resolution
  // wallpaper for the display. All the previous small resolution wallpaper
  // cache should be cleared.
  void ReloadWallpaper(bool clear_cache);

  // Sets |calculated_colors_| and notifies the observers if
  // there is a change.
  void SetCalculatedColors(const WallpaperCalculatedColors& calculated_colors);

  // Sets all elements of |calculated_colors_.prominent_colors| and
  // |calculated_colors_.k_mean_color| to |kInvalidWallpaperColor| via
  // SetCalculatedColors().
  void ResetCalculatedColors();

  // Calculates prominent colors based on the wallpaper image and notifies
  // |observers_| of the value, either synchronously or asynchronously. In some
  // cases the wallpaper image will not actually be processed (e.g. user isn't
  // logged in, feature isn't enabled).
  // If an existing calculation is in progress it is destroyed.
  void CalculateWallpaperColors();

  // Callback to handle the completed color computation for the wallpaper
  // matching `info`. Caches `colors` locally and saves the result to local
  // state.
  void OnColorCalculationComplete(const WallpaperInfo& info,
                                  const WallpaperCalculatedColors& colors);

  // The callback when decoding of the override wallpaper completes.
  void OnOverrideWallpaperDecoded(const WallpaperInfo& info,
                                  const gfx::ImageSkia& image);

  // Returns whether the current wallpaper is set by device policy.
  bool IsDevicePolicyWallpaper() const;

  // Returns whether the current wallpaper has type of
  // `Wallpapertype::kOneShot`.
  bool IsOneShotWallpaper() const;

  // Returns true if device wallpaper policy is in effect and we are at the
  // login screen right now.
  bool ShouldSetDevicePolicyWallpaper() const;

  // Reads the device wallpaper file and sets it as the current wallpaper. Note
  // when it's called, it's guaranteed that ShouldSetDevicePolicyWallpaper()
  // should be true.
  void SetDevicePolicyWallpaper();

  // Called when the device policy controlled wallpaper has been decoded.
  void OnDevicePolicyWallpaperDecoded(const gfx::ImageSkia& image);

  // When wallpaper resizes, we can check which displays will be affected. For
  // simplicity, we only lock the compositor for the internal display.
  void GetInternalDisplayCompositorLock();

  // Schedules paint on all WallpaperViews owned by WallpaperWidgetControllers.
  // This is used when we want to change wallpaper dimming.
  void RepaintWallpaper();

  void HandleWallpaperInfoSyncedIn(const AccountId& account_id,
                                   const WallpaperInfo& info);
  void OnAttemptSetOnlineWallpaper(const OnlineWallpaperParams& params,
                                   SetWallpaperCallback callback,
                                   bool success);

  // Save the downloaded |params.variants| at |current_index|.
  void OnOnlineWallpaperVariantDownloaded(const OnlineWallpaperParams& params,
                                          base::RepeatingClosure on_done,
                                          size_t current_index,
                                          const gfx::ImageSkia& image);

  // Check that all variants are downloaded successfully and set the wallpaper
  // from |params.url|.
  void OnAllOnlineWallpaperVariantsDownloaded(
      const OnlineWallpaperParams& params,
      SetWallpaperCallback callback);

  // If daily refresh wallpapers is enabled by the user.
  bool IsDailyRefreshEnabled() const;

  // If a Google Photos daily refresh wallpaper is the active user's wallpaper.
  bool IsDailyGooglePhotosWallpaperSelected();

  // If the user has a Google Photos wallpaper set.
  bool IsGooglePhotosWallpaperSet() const;

  // Starts a wall clock timer, to update the wallpaper 24 hours since the last
  // wallpaper was set.
  void StartDailyRefreshTimer();

  // Starts a wall clock timer, to confirm that the current Google Photos
  // photo set as the wallpaper still exists in the user's library.
  void StartGooglePhotosStalenessTimer();

  // Starts a wall clock timer to retry fetching a daily refresh wallpaper.
  void OnFetchDailyWallpaperFailed();

  // Starts a wall clock timer with the specified |delay|.
  void StartUpdateWallpaperTimer(base::TimeDelta delay);

  // Time to next wallpaper update for daily refresh; 24 hours since last
  // wallpaper set.
  base::TimeDelta GetTimeToNextDailyRefreshUpdate() const;

  // Called when `update_wallpaper_timer_` expires to take the appropriate
  // action for whatever type of wallpaper is currently set.
  void OnUpdateWallpaperTimerExpired();

  // Checks to make sure the currently selected Google Photos wallpaper still
  // exists in the user's Google Photos library.
  void CheckGooglePhotosStaleness(const AccountId& account_id,
                                  const WallpaperInfo& info);
  void HandleGooglePhotosStalenessCheck(
      const AccountId& account_id,
      ash::personalization_app::mojom::GooglePhotosPhotoPtr photo,
      bool success);

  void SaveWallpaperToDriveFsAndSyncInfo(const AccountId& account_id,
                                         const base::FilePath& origin_path);

  void WallpaperSavedToDriveFS(const AccountId& account_id, bool success);

  void HandleCustomWallpaperInfoSyncedIn(const AccountId& account_id,
                                         const WallpaperInfo& info);

  void OnDriveFsWallpaperChange(const AccountId& account_id, bool success);

  void OnGetDriveFsWallpaperModificationTime(
      const AccountId& account_id,
      const WallpaperInfo& wallpaper_info,
      base::Time modification_time);

  PrefService* GetUserPrefServiceSyncable(const AccountId& account_id) const;

  // This will not update a new wallpaper if the synced |info.collection_id| is
  // the same as the user's current collection_id.
  void HandleDailyWallpaperInfoSyncedIn(const AccountId& account_id,
                                        const WallpaperInfo& info);

  void HandleGooglePhotosWallpaperInfoSyncedIn(const AccountId& account_id,
                                               const WallpaperInfo& info);

  // Updates the online and daily wallpaper with the correct variant based on
  // the color mode.
  void HandleSettingOnlineWallpaperFromWallpaperInfo(
      const AccountId& account_id,
      const WallpaperInfo& info);

  void CleanUpBeforeSettingUserWallpaperInfo(const AccountId& account_id,
                                             const WallpaperInfo& info);

  bool locked_ = false;

  WallpaperMode wallpaper_mode_ = WALLPAPER_NONE;

  // Client interface in chrome browser.
  raw_ptr<WallpaperControllerClient, ExperimentalAsh>
      wallpaper_controller_client_ = nullptr;

  base::ObserverList<WallpaperControllerObserver>::Unchecked observers_;

  std::unique_ptr<WallpaperMetricsManager> wallpaper_metrics_manager_;

  std::unique_ptr<WallpaperResizer> current_wallpaper_;

  // Manages interactions with relevant preferences.
  std::unique_ptr<WallpaperPrefManager> pref_manager_;

  // The delegate for drag-and-drop events over the wallpaper.
  // NOTE: May be `nullptr` when drag-and-drop related features are disabled.
  std::unique_ptr<WallpaperDragDropDelegate> drag_drop_delegate_;

  std::unique_ptr<WallpaperDriveFsDelegate> drivefs_delegate_;

  // Asynchronous task to extract colors from the wallpaper.
  std::unique_ptr<WallpaperColorCalculator> color_calculator_;

  // Manages the states of the other windows when the wallpaper app window is
  // active.
  std::unique_ptr<WallpaperWindowStateManager> window_state_manager_;

  // Delegate to resolve online wallpaper variants.
  std::unique_ptr<OnlineWallpaperVariantInfoFetcher> variant_info_fetcher_;

  // The calculated colors extracted from the current wallpaper.
  // Empty state is used to denote when colors have not yet been calculated.
  absl::optional<WallpaperCalculatedColors> calculated_colors_;

  // Caches the color profiles that need to do wallpaper color extracting.
  const std::vector<color_utils::ColorProfile> color_profiles_;

  // Account id of the current user.
  AccountId current_user_;

  // Cached wallpapers of users.
  CustomWallpaperMap wallpaper_cache_map_;

  // Cached default wallpaper.
  CachedDefaultWallpaper cached_default_wallpaper_;

  // The paths of the customized default wallpapers, if they exist.
  base::FilePath customized_default_small_path_;
  base::FilePath customized_default_large_path_;

  gfx::Size current_max_display_size_;

  base::OneShotTimer timer_;

  base::TimeDelta wallpaper_reload_delay_;

  bool is_wallpaper_blurred_for_lock_state_ = false;

  // The wallpaper animation duration. An empty value disables the animation.
  base::TimeDelta animation_duration_;

  base::FilePath device_policy_wallpaper_path_;

  // Whether the current wallpaper (if any) is the first wallpaper since the
  // controller initialization. Empty wallpapers for testing don't count.
  bool is_first_wallpaper_ = false;

  // If true, the current wallpaper should always stay on top.
  bool is_always_on_top_wallpaper_ = false;

  // If true, the current wallpaper is overridden.
  bool is_override_wallpaper_ = false;

  const std::unique_ptr<WallpaperImageDownloader> wallpaper_image_downloader_;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  ScopedSessionObserver scoped_session_observer_{this};

  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      theme_observation_{this};

  std::unique_ptr<ui::CompositorLock> compositor_lock_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // A non-empty value indicates the current wallpaper is in preview mode, which
  // expects either |ConfirmPreviewWallpaper| or |CancelPreviewWallpaper| to be
  // called to exit preview. In preview mode, other types of wallpaper requests
  // may still update wallpaper info for the user, but the preview wallpaper
  // cannot be replaced, except by another preview wallpaper.
  base::OnceClosure confirm_preview_wallpaper_callback_;

  // Called when the preview wallpaper needs to be reloaded (e.g. display size
  // change). Has the same lifetime with |confirm_preview_wallpaper_callback_|.
  base::RepeatingClosure reload_preview_wallpaper_callback_;

  // Called when the override wallpaper needs to be reloaded (e.g. display size
  // change). Non-empty if and only if |is_override_wallpaper_| is true.
  base::RepeatingClosure reload_override_wallpaper_callback_;

  // Transient storage for the wallpaper variant (out of the N total variants
  // that may exist for a given "unit") that was requested by the client. The
  // other N - 1 variants are saved to disc for potential future usage. After
  // all variants have been downloaded and saved, the
  // |online_wallpaper_variant_to_use_| is released and passed on for further
  // processing in the pipeline.
  gfx::ImageSkia online_wallpaper_variant_to_use_;
  size_t num_variants_downloaded_ = 0;

  // If true, use a solid color wallpaper as if it is the decoded image.
  bool bypass_decode_for_testing_ = false;

  // Tracks how many wallpapers have been set.
  int wallpaper_count_for_testing_ = 0;

  // If true, the one shot wallpaper is allowed to be blurred or shielded.
  bool allow_blur_or_shield_for_testing_ = false;

  // The file paths of decoding requests that have been initiated. Must be a
  // list because more than one decoding requests may happen during a single
  // 'set wallpaper' request. (e.g. when a custom wallpaper decoding fails, a
  // default wallpaper decoding is initiated.)
  std::vector<base::FilePath> decode_requests_for_testing_;

  base::WallClockTimer update_wallpaper_timer_;

  base::WeakPtrFactory<WallpaperControllerImpl> weak_factory_{this};

  // Used for setting different types of wallpaper.
  base::WeakPtrFactory<WallpaperControllerImpl> set_wallpaper_weak_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_CONTROLLER_IMPL_H_
