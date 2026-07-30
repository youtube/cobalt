// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_WIDGET_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_WIDGET_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "ui/views/widget/widget.h"

class ProfilePickerView;
class ThemeService;
class Profile;

class ProfilePickerWidget : public views::Widget, public ThemeServiceObserver {
 public:
  explicit ProfilePickerWidget(ProfilePickerView* profile_picker_view);
  ProfilePickerWidget(const ProfilePickerWidget&) = delete;
  ProfilePickerWidget& operator=(const ProfilePickerWidget&) = delete;
  ~ProfilePickerWidget() override;

  // ThemeServiceObserver:
  void OnThemeChanged() override;

  // views::Widget:
  ui::ColorProviderKey GetColorProviderKey() const override;

 private:
  raw_ptr<ProfilePickerView> profile_picker_view_;
  raw_ptr<Profile> profile_ = nullptr;
  base::ScopedObservation<ThemeService, ThemeServiceObserver>
      theme_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_WIDGET_H_
