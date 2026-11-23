// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_MENU_MODEL_H_
#define CHROME_BROWSER_INDIGO_INDIGO_MENU_MODEL_H_

#include "base/memory/weak_ptr.h"
#include "ui/menus/simple_menu_model.h"

class Profile;

namespace indigo {

class IndigoPageActionController;

// Menu model for the Indigo anchored message menu.
class IndigoMenuModel : public ui::SimpleMenuModel,
                        public ui::SimpleMenuModel::Delegate {
 public:
  IndigoMenuModel(Profile* profile,
                  base::WeakPtr<IndigoPageActionController> controller);
  IndigoMenuModel(const IndigoMenuModel&) = delete;
  IndigoMenuModel& operator=(const IndigoMenuModel&) = delete;
  ~IndigoMenuModel() override;

  // ui::SimpleMenuModel::Delegate overrides:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  raw_ptr<Profile> profile_;
  base::WeakPtr<IndigoPageActionController> controller_;
};

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_MENU_MODEL_H_
