// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_PROVIDER_SOURCE_H_
#define UI_COLOR_COLOR_PROVIDER_SOURCE_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_manager.h"

namespace ui {

class ColorProviderSourceObserver;

// Encapsulates the theme bits that are used to uniquely identify a
// ColorProvider instance (i.e. the bits that comprise the
// ColorProviderManager::ColorProviderKey). Notifies observers when the
// ColorProvider instance that maps to these theme bits changes. References are
// managed to avoid dangling pointers in the case the source or the observer are
// deleted.
class COMPONENT_EXPORT(COLOR) ColorProviderSource {
 public:
  ColorProviderSource();
  virtual ~ColorProviderSource();

  // Returns the ColorProvider associated with the Key returned by
  // GetColorProviderKey();
  virtual const ColorProvider* GetColorProvider() const = 0;

  void AddObserver(ColorProviderSourceObserver* observer);
  void RemoveObserver(ColorProviderSourceObserver* observer);

  // Should be called by the implementation whenever the ColorProvider supplied
  // in the method `GetColorProvider()` changes.
  void NotifyColorProviderChanged();

  base::ObserverList<ColorProviderSourceObserver>& observers_for_testing() {
    return observers_;
  }

 protected:
  // Implementations should return the ColorProviderManager::Key associated with
  // this source.
  virtual ColorProviderManager::Key GetColorProviderKey() const = 0;

  // Gets the user color for this color source. This is used as an input to
  // generate color palettes for the material design system.
  virtual absl::optional<SkColor> GetUserColor() const;

 private:
  base::ObserverList<ColorProviderSourceObserver> observers_;
};

}  // namespace ui

#endif  // UI_COLOR_COLOR_PROVIDER_H_
