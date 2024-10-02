// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_CONTROLLER_H_
#define ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_CONTROLLER_H_

#include <map>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/user_education/tutorial_controller.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "ui/base/interaction/element_identifier.h"

namespace ash {

class SessionController;
class WelcomeTourControllerObserver;

// Controller responsible for Welcome Tour feature tutorials. Note that the
// `WelcomeTourController` is owned by the `UserEducationController` and exists
// if and only if the Welcome Tour feature is enabled.
class ASH_EXPORT WelcomeTourController : public TutorialController,
                                         public SessionObserver {
 public:
  WelcomeTourController();
  WelcomeTourController(const WelcomeTourController&) = delete;
  WelcomeTourController& operator=(const WelcomeTourController&) = delete;
  ~WelcomeTourController() override;

  // Returns the singleton instance owned by the `UserEducationController`.
  // NOTE: Exists if and only if the Welcome Tour feature is enabled.
  static WelcomeTourController* Get();

  // Adds/removes the specified `observer` from being notified of events.
  void AddObserver(WelcomeTourControllerObserver* observer);
  void RemoveObserver(WelcomeTourControllerObserver* observer);

  // Returns the initial element context to be used to start the Welcome Tour.
  ui::ElementContext GetInitialElementContext() const;

 private:
  // TutorialController:
  std::map<TutorialId, user_education::TutorialDescription>
  GetTutorialDescriptions() override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;
  void OnChromeTerminating() override;
  void OnSessionStateChanged(session_manager::SessionState) override;

  // Starts the Welcome Tour tutorial iff the primary user session is active.
  void MaybeStartTutorial();

  // Invoked when the Welcome Tour is started/ended. The latter is called
  // regardless of whether the tour was completed or aborted.
  void OnWelcomeTourStarted();
  void OnWelcomeTourEnded();

  // The collection of observers to be notified of events.
  base::ObserverList<WelcomeTourControllerObserver> observer_list_;

  // Sessions are observed only until the primary user session is activated for
  // the first time at which point the Welcome Tour tutorial is started.
  base::ScopedObservation<SessionController, SessionObserver>
      session_observation_{this};

  // It is theoretically possible for the Welcome Tour tutorial to outlive
  // `this` controller during the destruction sequence.
  base::WeakPtrFactory<WelcomeTourController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_CONTROLLER_H_
