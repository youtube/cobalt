// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/note_taking_client.h"
#include "ash/system/palette/mock_palette_tool_delegate.h"
#include "ash/system/palette/palette_ids.h"
#include "ash/system/palette/palette_tool.h"
#include "ash/system/palette/tools/create_note_action.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/ptr_util.h"
#include "ui/views/view.h"

namespace ash {

class TestNoteTakingControllerClient : public NoteTakingClient {
 public:
  TestNoteTakingControllerClient() = default;

  TestNoteTakingControllerClient(const TestNoteTakingControllerClient&) =
      delete;
  TestNoteTakingControllerClient& operator=(
      const TestNoteTakingControllerClient&) = delete;

  ~TestNoteTakingControllerClient() override = default;

  int GetCreateNoteCount() {
    return create_note_count_;
  }

  void set_can_create(bool can_create) { can_create_ = can_create; }

  // NoteTakingClient:
  bool CanCreateNote() override { return can_create_; }
  void CreateNote() override { create_note_count_++; }

 private:
  bool can_create_ = true;
  int create_note_count_ = 0;
};

namespace {

// Base class for all create note ash tests.
class CreateNoteTest : public AshTestBase {
 public:
  CreateNoteTest() = default;

  CreateNoteTest(const CreateNoteTest&) = delete;
  CreateNoteTest& operator=(const CreateNoteTest&) = delete;

  ~CreateNoteTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    palette_tool_delegate_ = std::make_unique<MockPaletteToolDelegate>();
    tool_ = std::make_unique<CreateNoteAction>(palette_tool_delegate_.get());
  }

  void TearDown() override {
    // This needs to be called first to remove the event handler before the
    // shell instance gets torn down.
    tool_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<MockPaletteToolDelegate> palette_tool_delegate_;
  std::unique_ptr<PaletteTool> tool_;
};

}  // namespace

// The note tool is only visible when there is a note-taking app available.
TEST_F(CreateNoteTest, ViewOnlyCreatedWhenNoteAppIsAvailable) {
  EXPECT_FALSE(tool_->CreateView());
  tool_->OnViewDestroyed();

  auto note_taking_client = std::make_unique<TestNoteTakingControllerClient>();
  std::unique_ptr<views::View> view = base::WrapUnique(tool_->CreateView());
  EXPECT_TRUE(view);
  tool_->OnViewDestroyed();

  note_taking_client->set_can_create(false);
  EXPECT_FALSE(tool_->CreateView());
  tool_->OnViewDestroyed();

  note_taking_client.reset();
  EXPECT_FALSE(tool_->CreateView());
  tool_->OnViewDestroyed();
}

// Activating the note tool both creates a note on the client and also
// disables the tool and hides the palette.
TEST_F(CreateNoteTest, EnablingToolCreatesNewNoteAndDisablesTool) {
  auto note_taking_client = std::make_unique<TestNoteTakingControllerClient>();
  std::unique_ptr<views::View> view = base::WrapUnique(tool_->CreateView());

  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::CREATE_NOTE));
  EXPECT_CALL(*palette_tool_delegate_.get(), HidePalette());

  tool_->OnEnable();
  EXPECT_EQ(1, note_taking_client->GetCreateNoteCount());
}

TEST_F(CreateNoteTest, ClientGetsDisabledAfterViewCreated) {
  auto note_taking_client = std::make_unique<TestNoteTakingControllerClient>();
  std::unique_ptr<views::View> view = base::WrapUnique(tool_->CreateView());

  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::CREATE_NOTE));
  EXPECT_CALL(*palette_tool_delegate_.get(), HidePalette());

  note_taking_client->set_can_create(false);

  tool_->OnEnable();
  EXPECT_EQ(0, note_taking_client->GetCreateNoteCount());
}

TEST_F(CreateNoteTest, ClientGetsRemovedAfterViewCreated) {
  auto note_taking_client = std::make_unique<TestNoteTakingControllerClient>();
  std::unique_ptr<views::View> view = base::WrapUnique(tool_->CreateView());

  EXPECT_CALL(*palette_tool_delegate_.get(),
              DisableTool(PaletteToolId::CREATE_NOTE));
  EXPECT_CALL(*palette_tool_delegate_.get(), HidePalette());

  note_taking_client.reset();

  tool_->OnEnable();
}

TEST_F(CreateNoteTest, ToolIsEnabledWhenStylusButtonIsPressed) {
  auto note_taking_client = std::make_unique<TestNoteTakingControllerClient>();
  std::unique_ptr<views::View> view = base::WrapUnique(tool_->CreateView());

  // Send a stylus button event.
  PressAndReleaseKey(ui::VKEY_F19, ui::EF_IS_STYLUS_BUTTON);

  EXPECT_EQ(1, note_taking_client->GetCreateNoteCount());
}

// The note tool is only visible on the internal display
TEST_F(CreateNoteTest, ViewOnlyAppearsOnInternalDisplay) {
  auto note_taking_client = std::make_unique<TestNoteTakingControllerClient>();
  std::unique_ptr<views::View> view = base::WrapUnique(tool_->CreateView());
  EXPECT_TRUE(view);
  tool_->OnViewDestroyed();

  tool_->SetExternalDisplayForTest();
  EXPECT_FALSE(tool_->CreateView());
  tool_->OnViewDestroyed();
}
}  // namespace ash
