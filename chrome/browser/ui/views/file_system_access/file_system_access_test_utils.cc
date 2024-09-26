// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_test_utils.h"

#include "url/gurl.h"

SelectPredeterminedFileDialog::SelectPredeterminedFileDialog(
    std::vector<base::FilePath> result,
    Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy)
    : ui::SelectFileDialog(listener, std::move(policy)),
      result_(std::move(result)) {}

SelectPredeterminedFileDialog::~SelectPredeterminedFileDialog() = default;

void SelectPredeterminedFileDialog::SelectFileImpl(
    Type type,
    const std::u16string& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owning_window,
    void* params,
    const GURL* caller) {
  if (result_.size() == 1)
    listener_->FileSelected(result_[0], 0, params);
  else
    listener_->MultiFilesSelected(result_, params);
}

bool SelectPredeterminedFileDialog::IsRunning(
    gfx::NativeWindow owning_window) const {
  return false;
}

void SelectPredeterminedFileDialog::ListenerDestroyed() {}

bool SelectPredeterminedFileDialog::HasMultipleFileTypeChoicesImpl() {
  return false;
}

SelectPredeterminedFileDialogFactory::SelectPredeterminedFileDialogFactory(
    std::vector<base::FilePath> result)
    : result_(std::move(result)) {}

SelectPredeterminedFileDialogFactory::~SelectPredeterminedFileDialogFactory() =
    default;

ui::SelectFileDialog* SelectPredeterminedFileDialogFactory::Create(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  return new SelectPredeterminedFileDialog(result_, listener,
                                           std::move(policy));
}
