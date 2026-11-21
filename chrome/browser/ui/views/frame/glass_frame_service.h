// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_FRAME_SERVICE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_FRAME_SERVICE_H_

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

class BrowserWidget;

// A singleton service that is the single source of truth for whether
// a browser window should display the glass frame or not.
class GlassFrameService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnGlassFrameStateChanged(GlassFrameService* service) = 0;
  };

  static GlassFrameService* GetInstance();

  GlassFrameService(const GlassFrameService&) = delete;
  GlassFrameService& operator=(const GlassFrameService&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool IsBrowserWidgetEligible(BrowserWidget* widget);

 private:
  friend class base::NoDestructor<GlassFrameService>;

  GlassFrameService();
  ~GlassFrameService();

  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_FRAME_SERVICE_H_
