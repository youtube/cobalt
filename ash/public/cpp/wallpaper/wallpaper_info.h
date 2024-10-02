// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_INFO_H_
#define ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_INFO_H_

#include <ostream>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/wallpaper/google_photos_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "base/time/time.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

struct ASH_PUBLIC_EXPORT WallpaperInfo {
  WallpaperInfo();

  explicit WallpaperInfo(const OnlineWallpaperParams& online_wallpaper_params);
  explicit WallpaperInfo(
      const GooglePhotosWallpaperParams& google_photos_wallpaper_params);

  WallpaperInfo(const std::string& in_location,
                WallpaperLayout in_layout,
                WallpaperType in_type,
                const base::Time& in_date,
                const std::string& in_user_file_path = "");

  WallpaperInfo(const WallpaperInfo& other);
  WallpaperInfo& operator=(const WallpaperInfo& other);

  WallpaperInfo(WallpaperInfo&& other);
  WallpaperInfo& operator=(WallpaperInfo&& other);

  // MatchesAsset() takes the current wallpaper variant into account, whereas
  // MatchesSelection() doesn't. For example if WallpaperInfo A has theme X with
  // variant 1, and WallpaperInfo B has theme X with variant 2,
  // MatchesSelection() will be true and MatchesAsset() will be false. Put
  // differently, MatchesSelection() tells whether the same wallpaper has been
  // selected, whereas MatchesAsset() tells whether the exact same wallpaper
  // image is active.
  bool MatchesSelection(const WallpaperInfo& other) const;
  bool MatchesAsset(const WallpaperInfo& other) const;

  ~WallpaperInfo();

  // Either file name of migrated wallpaper including first directory level
  // (corresponding to user wallpaper_files_id), online wallpaper URL, or
  // Google Photos id.
  std::string location;
  // user_file_path is the full path of the wallpaper file and is used as
  // the new CurrentWallpaper key. This field is required as the old key which
  // was set to the filename part made the UI mistakenly highlight multiple
  // files with the same name as the currently set wallpaper (b/229420564).
  std::string user_file_path;
  WallpaperLayout layout;
  WallpaperType type;
  base::Time date;

  // These fields are applicable if |type| == WallpaperType::kOnceGooglePhotos
  // or WallpaperType::kDailyGooglePhotos.
  absl::optional<std::string> dedup_key;

  // These fields are applicable if |type| == WallpaperType::kOnline or
  // WallpaperType::kDaily.
  absl::optional<uint64_t> asset_id;
  std::string collection_id;
  absl::optional<uint64_t> unit_id;
  std::vector<OnlineWallpaperVariant> variants;

  // Not empty if type == WallpaperType::kOneShot.
  // This field is filled in by ShowWallpaperImage when image is already
  // decoded.
  gfx::ImageSkia one_shot_wallpaper;
};

// For logging use only. Prints out text representation of the `WallpaperInfo`.
ASH_PUBLIC_EXPORT std::ostream& operator<<(std::ostream& os,
                                           const WallpaperInfo& info);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_WALLPAPER_INFO_H_
