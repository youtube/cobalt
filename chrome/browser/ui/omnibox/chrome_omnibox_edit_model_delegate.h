// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_EDIT_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_EDIT_MODEL_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/omnibox_edit_model_delegate.h"

class Browser;
class CommandUpdater;
class Profile;

namespace content {
class WebContents;
}

// Chrome-specific extension of the OmniboxEditModelDelegate base class.
class ChromeOmniboxEditModelDelegate : public OmniboxEditModelDelegate {
 public:
  ChromeOmniboxEditModelDelegate(Browser* browser,
                                 Profile* profile,
                                 CommandUpdater* command_updater);
  ChromeOmniboxEditModelDelegate(const ChromeOmniboxEditModelDelegate&) =
      delete;
  ChromeOmniboxEditModelDelegate& operator=(
      const ChromeOmniboxEditModelDelegate&) = delete;
  ~ChromeOmniboxEditModelDelegate() override;

  // OmniboxEditModelDelegate:
  void OnAutocompleteAccept(
      const GURL& destination_url,
      TemplateURLRef::PostContent* post_content,
      WindowOpenDisposition disposition,
      ui::PageTransition transition,
      AutocompleteMatchType::Type type,
      base::TimeTicks match_selection_timestamp,
      bool destination_url_entered_without_scheme,
      const std::u16string& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternative_nav_match,
      IDNA2008DeviationCharacter deviation_char_in_hostname) override;
  void OnInputInProgress(bool in_progress) override;

  // Returns the WebContents of the currently active tab.
  virtual content::WebContents* GetWebContents() = 0;

  // Called when the the controller should update itself without restoring any
  // tab state.
  virtual void UpdateWithoutTabRestore() = 0;

  CommandUpdater* command_updater() { return command_updater_; }
  const CommandUpdater* command_updater() const { return command_updater_; }

 private:
  const raw_ptr<Browser> browser_;
  const raw_ptr<Profile> profile_;
  const raw_ptr<CommandUpdater, DanglingUntriaged> command_updater_;
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_CHROME_OMNIBOX_EDIT_MODEL_DELEGATE_H_
