// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_APP_MODE_KIOSK_SESSION_SERVICE_LACROS_H_
#define CHROME_BROWSER_LACROS_APP_MODE_KIOSK_SESSION_SERVICE_LACROS_H_

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chromeos/crosapi/mojom/kiosk_session_service.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "url/gurl.h"

namespace chromeos {
class AppSession;
}

class Browser;

// Manage the kiosk session and related resources at the lacros side.
class KioskSessionServiceLacros {
 public:
  // Get the global instance. This singleton instance should be initialized
  // first before using it.
  static KioskSessionServiceLacros* Get();

  // Registers all local state prefs associated with the service.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Registers all profile prefs associated with the service.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  KioskSessionServiceLacros();
  KioskSessionServiceLacros(const KioskSessionServiceLacros&) = delete;
  KioskSessionServiceLacros& operator=(const KioskSessionServiceLacros&) =
      delete;
  virtual ~KioskSessionServiceLacros();

  // Initialize the current Chrome Kiosk session with the |app_id|.
  void InitChromeKioskSession(Profile* profile, const std::string& app_id);

  // Initialize the current Web Kiosk session with the |install_url| and the
  // browser that is running the app.
  void InitWebKioskSession(Browser* browser, const GURL& install_url);

  // Get install URL for Web Kiosk session.
  const GURL& GetInstallURL() const { return install_url_; }

  // Get app session object for testing purpose only.
  chromeos::AppSession* GetAppSessionForTesting() const {
    return app_session_.get();
  }

 protected:
  // Tell the ash-chrome to end the kiosk session and return the current user
  // to the login screen by calling the API provided by
  // `kiosk_session_service_`. Virtual for tesitng.
  virtual void AttemptUserExit();

  // The install URL used to initialize Web Kiosk session.
  GURL install_url_;

  // The app session instance to observe the window status, and take action if
  // necessary.
  std::unique_ptr<chromeos::AppSession> app_session_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<KioskSessionServiceLacros> weak_factory_{this};
};

#endif  // CHROME_BROWSER_LACROS_APP_MODE_KIOSK_SESSION_SERVICE_LACROS_H_
