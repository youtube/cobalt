// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"

#include "base/metrics/user_metrics.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search_engines/template_url_table_model.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"

using base::UserMetricsAction;

KeywordEditorController::KeywordEditorController(Profile* profile)
    : url_model_(TemplateURLServiceFactory::GetForProfile(profile)) {
  table_model_ = std::make_unique<TemplateURLTableModel>(url_model_);
}

KeywordEditorController::~KeywordEditorController() {
}

int KeywordEditorController::AddTemplateURL(const std::u16string& title,
                                            const std::u16string& keyword,
                                            const std::string& url) {
  DCHECK(!url.empty());

  base::RecordAction(UserMetricsAction("KeywordEditor_AddKeyword"));

  const int new_index = table_model_->last_other_engine_index();
  table_model_->Add(new_index, title, keyword, url);

  return new_index;
}

void KeywordEditorController::ModifyTemplateURL(TemplateURL* template_url,
                                                const std::u16string& title,
                                                const std::u16string& keyword,
                                                const std::string& url) {
  DCHECK(!url.empty());
  const absl::optional<size_t> index =
      table_model_->IndexOfTemplateURL(template_url);
  if (!index.has_value()) {
    // Will happen if url was deleted out from under us while the user was
    // editing it.
    return;
  }

  // Don't do anything if the entry didn't change.
  if ((template_url->short_name() == title) &&
      (template_url->keyword() == keyword) && (template_url->url() == url))
    return;

  table_model_->ModifyTemplateURL(index.value(), title, keyword, url);

  base::RecordAction(UserMetricsAction("KeywordEditor_ModifiedKeyword"));
}

bool KeywordEditorController::CanEdit(const TemplateURL* url) const {
  return (url->type() == TemplateURL::NORMAL) &&
      (url != url_model_->GetDefaultSearchProvider() ||
       !url_model_->is_default_search_managed());
}

bool KeywordEditorController::CanMakeDefault(const TemplateURL* url) const {
  return url_model_->CanMakeDefault(url);
}

bool KeywordEditorController::CanRemove(const TemplateURL* url) const {
  return (url->type() == TemplateURL::NORMAL) &&
         (url != url_model_->GetDefaultSearchProvider()) &&
         (url->starter_pack_id() == 0);
}

bool KeywordEditorController::CanActivate(const TemplateURL* url) const {
  return (url->is_active() != TemplateURLData::ActiveStatus::kTrue) &&
         (url->prepopulate_id() == 0);
}

bool KeywordEditorController::CanDeactivate(const TemplateURL* url) const {
  return (url->is_active() == TemplateURLData::ActiveStatus::kTrue &&
          url != url_model_->GetDefaultSearchProvider() &&
          url->prepopulate_id() == 0);
}

bool KeywordEditorController::ShouldConfirmDeletion(
    const TemplateURL* url) const {
  // Currently, only built-in search engines require confirmation before
  // deletion.
  return url->prepopulate_id() != 0;
}

void KeywordEditorController::RemoveTemplateURL(int index) {
  table_model_->Remove(index);
  base::RecordAction(UserMetricsAction("KeywordEditor_RemoveKeyword"));
}

const TemplateURL* KeywordEditorController::GetDefaultSearchProvider() {
  return url_model_->GetDefaultSearchProvider();
}

void KeywordEditorController::MakeDefaultTemplateURL(int index) {
  table_model_->MakeDefaultTemplateURL(index);
}

void KeywordEditorController::SetIsActiveTemplateURL(int index,
                                                     bool is_active) {
  table_model_->SetIsActiveTemplateURL(index, is_active);
}

bool KeywordEditorController::loaded() const {
  return url_model_->loaded();
}

TemplateURL* KeywordEditorController::GetTemplateURL(int index) {
  return table_model_->GetTemplateURL(index);
}
