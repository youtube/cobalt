// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_UI_SETTINGS_H_
#define ASH_AMBIENT_AMBIENT_UI_SETTINGS_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/constants/ambient_theme.h"
#include "ash/constants/ambient_video.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace ash {

// Captures the ambient UI that the user has selected in the Hub.
class ASH_EXPORT AmbientUiSettings {
 public:
  // Returns default set of |AmbientUiSettings| if the settings do not exist in
  // |pref_service| or the existing settings are corrupted in pref store.
  static AmbientUiSettings ReadFromPrefService(PrefService& pref_service);

  // Creates the default ui settings. This is guaranteed to be valid.
  AmbientUiSettings();
  // Fatal error occurs if an invalid combination of settings is provided.
  explicit AmbientUiSettings(
      AmbientTheme theme,
      absl::optional<AmbientVideo> video = absl::nullopt);
  AmbientUiSettings(const AmbientUiSettings&);
  AmbientUiSettings& operator=(const AmbientUiSettings&);
  ~AmbientUiSettings();

  AmbientTheme theme() const { return theme_; }
  // Must be set if |theme()| is |kVideo|. Otherwise, may be nullopt.
  const absl::optional<AmbientVideo>& video() const { return video_; }

  bool operator==(const AmbientUiSettings& other) const;
  bool operator!=(const AmbientUiSettings& other) const;

  void WriteToPrefService(PrefService& pref_service) const;

  // Used for both metrics and debugging.
  std::string ToString() const;

 private:
  // Returns nullopt if the |dict| contains an invalid group of settings.
  static absl::optional<AmbientUiSettings> CreateFromDict(
      const base::Value::Dict& dict);

  // This is private because the caller by design should never be working with
  // an invalid instance. A fatal error should occur before then.
  bool IsValid() const;

  AmbientTheme theme_ = kDefaultAmbientTheme;
  absl::optional<AmbientVideo> video_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_UI_SETTINGS_H_
