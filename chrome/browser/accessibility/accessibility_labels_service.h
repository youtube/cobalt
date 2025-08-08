// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_LABELS_SERVICE_H_
#define CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_LABELS_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "net/base/network_change_notifier.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_mode_observer.h"
#endif

class Profile;

namespace image_annotation {
class ImageAnnotationService;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Manages the feature that generates automatic image labels for accessibility.
// Tracks the per-profile preference and updates the accessibility mode of
// WebContents when it changes, provided image labeling is not disabled via
// command-line switch.
class AccessibilityLabelsService
    : public KeyedService
#if BUILDFLAG(IS_ANDROID)
    // On Android, implement NetworkChangeObserver for "only on wifi" option,
    // and an AXModeObserver for detecting when a screen reader is enabled.
    ,
      public net::NetworkChangeNotifier::NetworkChangeObserver,
      public ui::AXModeObserver
#endif
{

 public:
  AccessibilityLabelsService(const AccessibilityLabelsService&) = delete;
  AccessibilityLabelsService& operator=(const AccessibilityLabelsService&) =
      delete;

  ~AccessibilityLabelsService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Off the record profiles will default to having the feature disabled.
  static void InitOffTheRecordPrefs(Profile* off_the_record_profile);

  void Init();

  bool IsEnabled();

  void EnableLabelsServiceOnce();

  // Routes an Annotator interface receiver to the Image Annotation service for
  // binding.
  void BindImageAnnotator(
      mojo::PendingReceiver<image_annotation::mojom::Annotator> receiver);

  // Allows tests to override how this object binds a connection to a remote
  // ImageAnnotationService.
  using ImageAnnotatorBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<image_annotation::mojom::ImageAnnotationService>)>;
  void OverrideImageAnnotatorBinderForTesting(ImageAnnotatorBinder binder);

#if BUILDFLAG(IS_ANDROID)
  // net::NetworkChangeNotifier::NetworkChangeObserver
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // ui::AXModeObserver
  void OnAXModeAdded(ui::AXMode mode) override;

  bool GetAndroidEnabledStatus();
#endif

 private:
  friend class AccessibilityLabelsServiceFactory;

  // Use |AccessibilityLabelsServiceFactory::GetForProfile(..)| to get
  // an instance of this service.
  explicit AccessibilityLabelsService(Profile* profile);

  void OnImageLabelsEnabledChanged();

  void UpdateAccessibilityLabelsHistograms();

  // Owns us via the KeyedService mechanism.
  raw_ptr<Profile> profile_;

  PrefChangeRegistrar pref_change_registrar_;

  // Implementation of and remote connection to the Image Annotation service.
  std::unique_ptr<image_annotation::ImageAnnotationService> service_;
  mojo::Remote<image_annotation::mojom::ImageAnnotationService> remote_service_;

  base::WeakPtrFactory<AccessibilityLabelsService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_ACCESSIBILITY_LABELS_SERVICE_H_
