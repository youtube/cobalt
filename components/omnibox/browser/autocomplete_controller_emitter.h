// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_CONTROLLER_EMITTER_H_
#define COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_CONTROLLER_EMITTER_H_

#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/autocomplete_controller.h"

#if !BUILDFLAG(IS_IOS)
#include "content/public/browser/browser_context.h"
#endif  // !BUILDFLAG(IS_IOS)

// This KeyedService is meant to observe multiple AutocompleteController
// instances and forward the notifications to its own observers.
// Its main purpose is to act as a bridge between the chrome://omnibox WebUI
// handler, and the many usages of AutocompleteController (Views, NTP, Android).
class AutocompleteControllerEmitter : public KeyedService,
                                      public AutocompleteController::Observer {
 public:
#if !BUILDFLAG(IS_IOS)
  static AutocompleteControllerEmitter* GetForBrowserContext(
      content::BrowserContext* browser_context);
#endif  // !BUILDFLAG(IS_IOS)

  AutocompleteControllerEmitter();
  ~AutocompleteControllerEmitter() override;
  AutocompleteControllerEmitter(const AutocompleteControllerEmitter&) = delete;
  AutocompleteControllerEmitter& operator=(
      const AutocompleteControllerEmitter&) = delete;

  // Add/remove observer.
  void AddObserver(AutocompleteController::Observer* observer);
  void RemoveObserver(AutocompleteController::Observer* observer);

  // AutocompleteController::Observer:
  void OnStart(AutocompleteController* controller,
               const AutocompleteInput& input) override;
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

#if !BUILDFLAG(IS_IOS)
  static void EnsureFactoryBuilt();
#endif  // !BUILDFLAG(IS_IOS)

 private:
  base::ObserverList<AutocompleteController::Observer> observers_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AUTOCOMPLETE_CONTROLLER_EMITTER_H_
