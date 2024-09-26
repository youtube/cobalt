// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_VIDEO_UI_LAUNCHER_H_
#define ASH_AMBIENT_AMBIENT_VIDEO_UI_LAUNCHER_H_

#include <memory>

#include "ash/ambient/ambient_ui_launcher.h"
#include "ash/ambient/ambient_weather_controller.h"
#include "ash/constants/ambient_video.h"
#include "base/memory/raw_ptr.h"

class PrefService;

namespace ash {

class AmbientViewDelegate;

// Launches |AmbientTheme::kVideo|. Pulls the current |AmbientVideo| selected
// from pref storage when it's time to render the video.
class AmbientVideoUiLauncher : public AmbientUiLauncher {
 public:
  explicit AmbientVideoUiLauncher(PrefService* pref_service,
                                  AmbientViewDelegate* view_delegate);
  AmbientVideoUiLauncher(const AmbientVideoUiLauncher&) = delete;
  AmbientVideoUiLauncher& operator=(const AmbientVideoUiLauncher&) = delete;
  ~AmbientVideoUiLauncher() override;

  // AmbientUiLauncher overrides:
  void Initialize(InitializationCallback on_done) override;
  std::unique_ptr<views::View> CreateView() override;
  void Finalize() override;
  AmbientBackendModel* GetAmbientBackendModel() override;
  bool IsActive() override;
  bool IsReady() override;

 private:
  bool is_active_ = false;
  AmbientVideo current_video_;
  const base::raw_ptr<PrefService> pref_service_;
  const base::raw_ptr<AmbientViewDelegate> view_delegate_;
  std::unique_ptr<AmbientWeatherController::ScopedRefresher> weather_refresher_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_VIDEO_UI_LAUNCHER_H_
