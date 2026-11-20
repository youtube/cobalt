// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_widget.h"

#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/views/profiles/profile_picker_view.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/shell_integration_linux.h"
#endif

ProfilePickerWidget::ProfilePickerWidget(ProfilePickerView* profile_picker_view)
    : profile_picker_view_(profile_picker_view) {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  params.delegate = profile_picker_view;
#if BUILDFLAG(IS_LINUX)
  params.wm_class_name = shell_integration_linux::GetProgramClassName();
  params.wm_class_class = shell_integration_linux::GetProgramClassClass();
  params.wayland_app_id = params.wm_class_class;
#endif
  Init(std::move(params));

  content::WebContents* contents = profile_picker_view_->GetPickerContents();
  if (contents) {
    profile_ = Profile::FromBrowserContext(contents->GetBrowserContext());
    auto* theme_service = ThemeServiceFactory::GetForProfile(profile_);
    if (theme_service) {
      theme_observation_.Observe(theme_service);
    }
  }
}

ProfilePickerWidget::~ProfilePickerWidget() = default;

void ProfilePickerWidget::OnThemeChanged() {
  ThemeChanged();
}

ui::ColorProviderKey ProfilePickerWidget::GetColorProviderKey() const {
  auto key = views::Widget::GetColorProviderKey();
  if (theme_observation_.IsObserving() && profile_) {
    key = theme_observation_.GetSource()->GetColorProviderKey(key, profile_);
  }
  return key;
}
