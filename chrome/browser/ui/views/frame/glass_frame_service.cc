// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/glass_frame_service.h"

#include "base/check.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"

// static
GlassFrameService* GlassFrameService::GetInstance() {
  static base::NoDestructor<GlassFrameService> instance;
  return instance.get();
}

GlassFrameService::GlassFrameService() = default;

GlassFrameService::~GlassFrameService() = default;

void GlassFrameService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GlassFrameService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool GlassFrameService::IsBrowserWidgetEligible(BrowserWidget* widget) {
  return false;
}
