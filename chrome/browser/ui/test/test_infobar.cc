// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/test/test_infobar.h"

#include <algorithm>
#include <iterator>

#include "base/ranges/algorithm.h"
#include "chrome/browser/infobars/infobar_observer.h"
#include "chrome/browser/ui/browser.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"

TestInfoBar::TestInfoBar() = default;

TestInfoBar::~TestInfoBar() = default;

void TestInfoBar::PreShow() {
  starting_infobars_ = GetNewInfoBars().value();
}

bool TestInfoBar::VerifyUi() {
  absl::optional<InfoBars> infobars = GetNewInfoBars();
  if (!infobars || infobars->empty()) {
    ADD_FAILURE() << "No new infobars were displayed.";
    return false;
  }

  bool expected_infobars_found =
      base::ranges::equal(*infobars, expected_identifiers_, std::equal_to<>(),
                          [](infobars::InfoBar* infobar) {
                            return infobar->delegate()->GetIdentifier();
                          });
  if (!expected_infobars_found)
    ADD_FAILURE() << "Found unexpected infobars.";

  return expected_infobars_found;
}

void TestInfoBar::WaitForUserDismissal() {
  while (!GetNewInfoBars().value_or(InfoBars()).empty()) {
    InfoBarObserver observer(GetInfoBarManager(),
                             InfoBarObserver::Type::kInfoBarRemoved);
    observer.Wait();
  }
}

void TestInfoBar::AddExpectedInfoBar(
    infobars::InfoBarDelegate::InfoBarIdentifier identifier) {
  expected_identifiers_.push_back(identifier);
}

content::WebContents* TestInfoBar::GetWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

const content::WebContents* TestInfoBar::GetWebContents() const {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

infobars::ContentInfoBarManager* TestInfoBar::GetInfoBarManager() {
  return const_cast<infobars::ContentInfoBarManager*>(
      static_cast<const TestInfoBar*>(this)->GetInfoBarManager());
}

const infobars::ContentInfoBarManager* TestInfoBar::GetInfoBarManager() const {
  // There may be no web contents if the browser window is closing.
  const content::WebContents* web_contents = GetWebContents();
  return web_contents
             ? infobars::ContentInfoBarManager::FromWebContents(web_contents)
             : nullptr;
}

absl::optional<TestInfoBar::InfoBars> TestInfoBar::GetNewInfoBars() const {
  const infobars::ContentInfoBarManager* infobar_manager = GetInfoBarManager();
  if (!infobar_manager)
    return absl::nullopt;
  const InfoBars& infobars = infobar_manager->infobars_;
  if ((infobars.size() < starting_infobars_.size()) ||
      !std::equal(starting_infobars_.begin(), starting_infobars_.end(),
                  infobars.begin()))
    return absl::nullopt;
  return InfoBars(std::next(infobars.begin(), starting_infobars_.size()),
                  infobars.end());
}
