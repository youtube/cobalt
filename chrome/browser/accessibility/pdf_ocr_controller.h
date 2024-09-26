// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_PDF_OCR_CONTROLLER_H_
#define CHROME_BROWSER_ACCESSIBILITY_PDF_OCR_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/services/screen_ai/public/cpp/screen_ai_install_state.h"

class Profile;

namespace content {
class WebContents;
}

namespace screen_ai {

class PdfOcrControllerFactory;

// Manages the PDF OCR feature that extracts text from an inaccessible PDF.
// Observes changes in the per-profile preference and updates the accessibility
// mode of WebContents when it changes, provided its feature flag is enabled.
class PdfOcrController : public KeyedService, ScreenAIInstallState::Observer {
 public:
  PdfOcrController(const PdfOcrController&) = delete;
  PdfOcrController& operator=(const PdfOcrController&) = delete;
  ~PdfOcrController() override;

  // Return all PDF-related WebContentses associated with the PDF Viewer
  // Mimehandlers in a given Profile for testing.
  static std::vector<content::WebContents*> GetAllPdfWebContentsesForTesting(
      Profile* profile);

  // Return true if the PDF OCR pref is true; false otherwise.
  bool IsEnabled() const;
  // Run PDF OCR only once regardless of the PDF OCR pref value. This function
  // doesn't update the PDF OCR pref value.
  void RunPdfOcrOnlyOnce(content::WebContents* web_contents);

  // ScreenAIInstallState::Observer:
  void StateChanged(ScreenAIInstallState::State state) override;

 private:
  friend class PdfOcrControllerFactory;

  explicit PdfOcrController(Profile* profile);

  void OnPdfOcrAlwaysActiveChanged();

  // Sends Pdf Ocr Always Active state to all relevant WebContents.
  void SendPdfOcrAlwaysActiveToAll(bool is_always_active);

  // Observes changes in Screen AI component download and readiness state.
  base::ScopedObservation<ScreenAIInstallState, ScreenAIInstallState::Observer>
      component_ready_observer_{this};

  // PdfOcrController will be created via PdfOcrControllerFactory on this
  // profile and then destroyed before the profile gets destroyed.
  raw_ptr<Profile> profile_;

  PrefChangeRegistrar pref_change_registrar_;

  // Indicates that user has selected always active and we are waiting for the
  // Screen AI service to be ready to send this bit.
  bool send_always_active_state_when_service_is_ready_{false};

  base::WeakPtrFactory<PdfOcrController> weak_ptr_factory_{this};
};

}  // namespace screen_ai

#endif  // CHROME_BROWSER_ACCESSIBILITY_PDF_OCR_CONTROLLER_H_
