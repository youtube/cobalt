// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/settings/fake_password_import_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

FakePasswordImportController::FakePasswordImportController() = default;

FakePasswordImportController::~FakePasswordImportController() = default;

void FakePasswordImportController::Import(
    content::WebContents* web_contents,
    password_manager::PasswordForm::Store to_store,
    ImportResultsCallback results_callback) {
  password_manager::ImportResults results;
  results.status = import_results_status_;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(results_callback), results));
}

void FakePasswordImportController::ContinueImport(
    const std::vector<int>& selected_ids,
    ImportResultsCallback results_callback) {
  password_manager::ImportResults results;
  results.status = import_results_status_;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(results_callback), results));
}

void FakePasswordImportController::ResetImporter(bool delete_file) {}
