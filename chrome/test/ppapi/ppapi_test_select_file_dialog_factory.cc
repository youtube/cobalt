// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ppapi/ppapi_test_select_file_dialog_factory.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "url/gurl.h"

namespace {

class PPAPITestSelectFileDialog : public ui::SelectFileDialog {
 public:
  PPAPITestSelectFileDialog(
      Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy,
      const PPAPITestSelectFileDialogFactory::SelectedFileInfoList&
          selected_file_info,
      PPAPITestSelectFileDialogFactory::Mode mode)
      : ui::SelectFileDialog(listener, std::move(policy)),
        selected_file_info_(selected_file_info),
        mode_(mode) {}

 protected:
  // ui::SelectFileDialog
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params,
                      const GURL* caller) override {
    switch (mode_) {
      case PPAPITestSelectFileDialogFactory::RESPOND_WITH_FILE_LIST:
        break;

      case PPAPITestSelectFileDialogFactory::CANCEL:
        EXPECT_EQ(0u, selected_file_info_.size());
        break;

      case PPAPITestSelectFileDialogFactory::REPLACE_BASENAME:
        EXPECT_EQ(1u, selected_file_info_.size());
        for (auto& selected_file : selected_file_info_) {
          selected_file = ui::SelectedFileInfo(
              selected_file.file_path.DirName().Append(default_path.BaseName()),
              selected_file.local_path.DirName().Append(
                  default_path.BaseName()));
        }
        break;

      case PPAPITestSelectFileDialogFactory::NOT_REACHED:
        ADD_FAILURE() << "Unexpected SelectFileImpl invocation.";
    }

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &PPAPITestSelectFileDialog::RespondToFileSelectionRequest, this,
            params));
  }
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

  // BaseShellDialog
  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override {}

 private:
  void RespondToFileSelectionRequest(void* params) {
    if (selected_file_info_.size() == 0)
      listener_->FileSelectionCanceled(params);
    else if (selected_file_info_.size() == 1)
      listener_->FileSelectedWithExtraInfo(selected_file_info_.front(), 0,
                                           params);
    else
      listener_->MultiFilesSelectedWithExtraInfo(selected_file_info_, params);
  }

  PPAPITestSelectFileDialogFactory::SelectedFileInfoList selected_file_info_;
  PPAPITestSelectFileDialogFactory::Mode mode_;
};

}  // namespace

PPAPITestSelectFileDialogFactory::PPAPITestSelectFileDialogFactory(
    Mode mode,
    const SelectedFileInfoList& selected_file_info)
    : selected_file_info_(selected_file_info), mode_(mode) {
  // Only safe because this class is 'final'
  ui::SelectFileDialog::SetFactory(this);
}

// SelectFileDialogFactory
ui::SelectFileDialog* PPAPITestSelectFileDialogFactory::Create(
    ui::SelectFileDialog::Listener* listener,
    std::unique_ptr<ui::SelectFilePolicy> policy) {
  return new PPAPITestSelectFileDialog(listener, std::move(policy),
                                       selected_file_info_, mode_);
}
