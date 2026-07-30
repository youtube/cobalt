// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/context_hub/context_hub_page_handler.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/context_hub/context_hub_service.h"
#include "chrome/browser/context_hub/context_hub_service_factory.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/personal_context/personal_context_service_factory.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/personal_context/core/mock_personal_context_service.h"
#include "components/personal_context/core/personal_context_service.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace context_hub {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;

class ContextHubPageHandlerTest : public testing::Test {
 public:
  ContextHubPageHandlerTest() {
    feature_list_.InitWithFeatures(
        {features::kContextHub, features::kAutoTodos, features::kMemoryBanks},
        {});
  }

  void SetUp() override {
    testing::Test::SetUp();

    PersonalContextServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              personal_context::MockPersonalContextService>();
        }));

    handler_ = std::make_unique<ContextHubPageHandler>(
        mojo::PendingReceiver<browser::context_hub::mojom::PageHandler>(),
        &profile_, nullptr);
  }

 protected:
  personal_context::MockPersonalContextService* GetMockService() {
    return static_cast<personal_context::MockPersonalContextService*>(
        PersonalContextServiceFactory::GetForProfile(&profile_));
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<ContextHubPageHandler> handler_;
};

TEST_F(ContextHubPageHandlerTest, GenerateAutoTodos_Success) {
  personal_context::proto::AutoTodosResponse response;
  personal_context::proto::AutoTodoItem* todo = response.add_todos();
  todo->set_title("Test Title");
  todo->set_description("Test Description");

  personal_context::proto::Any any_response;
  response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(
      *GetMockService(),
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  base::test::TestFuture<
      std::optional<std::vector<browser::context_hub::mojom::AutoTodoItemPtr>>>
      future;
  handler_->GenerateAutoTodos(future.GetCallback());

  std::optional<std::vector<browser::context_hub::mojom::AutoTodoItemPtr>>
      result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ(result->at(0)->title, "Test Title");
  EXPECT_EQ(result->at(0)->description, "Test Description");
  EXPECT_TRUE(result->at(0)->source_references.empty());
}

TEST_F(ContextHubPageHandlerTest, GenerateAutoTodos_WithSourceReferences) {
  personal_context::proto::AutoTodosResponse response;
  personal_context::proto::AutoTodoItem* todo = response.add_todos();
  todo->set_title("Test Title");
  todo->set_description("Test Description");

  personal_context::proto::SourceReference* ref_gmail =
      todo->add_source_references();
  ref_gmail->mutable_gmail()->set_message_url(
      "https://mail.google.com/mail/u/0/#inbox/123");

  personal_context::proto::SourceReference* ref_photos =
      todo->add_source_references();
  ref_photos->mutable_photos()->set_photos_url(
      "https://photos.google.com/photo/456");

  personal_context::proto::Any any_response;
  response.SerializeToString(any_response.mutable_value());

  EXPECT_CALL(
      *GetMockService(),
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(personal_context::FetchContextResult(
          base::ok(std::move(any_response)))));

  base::test::TestFuture<
      std::optional<std::vector<browser::context_hub::mojom::AutoTodoItemPtr>>>
      future;
  handler_->GenerateAutoTodos(future.GetCallback());

  std::optional<std::vector<browser::context_hub::mojom::AutoTodoItemPtr>>
      result = future.Take();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->size(), 1u);
  EXPECT_EQ(result->at(0)->title, "Test Title");
  EXPECT_EQ(result->at(0)->description, "Test Description");

  ASSERT_EQ(result->at(0)->source_references.size(), 2u);

  const browser::context_hub::mojom::SourceReferencePtr& mojo_ref1 =
      result->at(0)->source_references[0];
  ASSERT_TRUE(mojo_ref1->is_gmail());
  EXPECT_EQ(mojo_ref1->get_gmail()->message_url,
            GURL("https://mail.google.com/mail/u/0/#inbox/123"));

  const browser::context_hub::mojom::SourceReferencePtr& mojo_ref2 =
      result->at(0)->source_references[1];
  ASSERT_TRUE(mojo_ref2->is_photos());
  EXPECT_EQ(mojo_ref2->get_photos()->photos_url,
            GURL("https://photos.google.com/photo/456"));
}

TEST_F(ContextHubPageHandlerTest, GenerateAutoTodos_Failure) {
  EXPECT_CALL(
      *GetMockService(),
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AUTO_TODOS,
                   _, _, _))
      .WillOnce(RunOnceCallback<3>(
          personal_context::FetchContextResult(base::unexpected(
              personal_context::ContextMemoryError::FromExecutionError(
                  personal_context::ContextMemoryError::ExecutionError::
                      kUnknown)))));

  base::test::TestFuture<
      std::optional<std::vector<browser::context_hub::mojom::AutoTodoItemPtr>>>
      future;
  handler_->GenerateAutoTodos(future.GetCallback());

  std::optional<std::vector<browser::context_hub::mojom::AutoTodoItemPtr>>
      result = future.Take();
  EXPECT_FALSE(result.has_value());
}

TEST_F(ContextHubPageHandlerTest, GetAllMemoryBankEntries_Empty) {
  base::test::TestFuture<
      std::vector<browser::context_hub::mojom::MemoryBankEntryPtr>>
      future;
  handler_->GetAllMemoryBankEntries(future.GetCallback());

  std::vector<browser::context_hub::mojom::MemoryBankEntryPtr> result =
      future.Take();
  EXPECT_TRUE(result.empty());
}

TEST_F(ContextHubPageHandlerTest, GetAllMemoryBankEntries_Success) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  base::test::TestFuture<void> save_tab_future;
  service->SaveTab(GURL("https://example.com/tab"), "Tab Title",
                   save_tab_future.GetCallback());
  ASSERT_TRUE(save_tab_future.Wait());

  base::test::TestFuture<void> save_selection_future;
  service->SaveTextSelection(GURL("https://example.com/select"),
                             "Selection Title", "Selected Text Detail",
                             save_selection_future.GetCallback());
  ASSERT_TRUE(save_selection_future.Wait());

  base::test::TestFuture<
      std::vector<browser::context_hub::mojom::MemoryBankEntryPtr>>
      future;
  handler_->GetAllMemoryBankEntries(future.GetCallback());

  std::vector<browser::context_hub::mojom::MemoryBankEntryPtr> result =
      future.Take();

  ASSERT_EQ(result.size(), 2u);

  const browser::context_hub::mojom::MemoryBankEntryPtr* tab_entry = nullptr;
  const browser::context_hub::mojom::MemoryBankEntryPtr* text_entry = nullptr;
  for (const auto& entry : result) {
    if (entry->type == browser::context_hub::mojom::EntryType::kTab) {
      tab_entry = &entry;
    } else if (entry->type ==
               browser::context_hub::mojom::EntryType::kTextSelection) {
      text_entry = &entry;
    }
  }

  ASSERT_TRUE(tab_entry);
  EXPECT_EQ((*tab_entry)->url, GURL("https://example.com/tab"));
  EXPECT_EQ((*tab_entry)->tab_title, "Tab Title");
  EXPECT_FALSE((*tab_entry)->timestamp.is_null());

  ASSERT_TRUE(text_entry);
  EXPECT_EQ((*text_entry)->url, GURL("https://example.com/select"));
  EXPECT_EQ((*text_entry)->tab_title, "Selection Title");
  EXPECT_EQ((*text_entry)->selected_text, "Selected Text Detail");
  EXPECT_FALSE((*text_entry)->timestamp.is_null());
}

TEST_F(ContextHubPageHandlerTest, DeleteMemoryBankEntries_Success) {
  ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(&profile_);
  ASSERT_TRUE(service);

  base::test::TestFuture<void> save_tab_future1;
  service->SaveTab(GURL("https://example.com/tab1"), "Tab Title 1",
                   save_tab_future1.GetCallback());
  ASSERT_TRUE(save_tab_future1.Wait());

  base::test::TestFuture<void> save_tab_future2;
  service->SaveTab(GURL("https://example.com/tab2"), "Tab Title 2",
                   save_tab_future2.GetCallback());
  ASSERT_TRUE(save_tab_future2.Wait());

  base::test::TestFuture<
      std::vector<browser::context_hub::mojom::MemoryBankEntryPtr>>
      get_all_future1;
  handler_->GetAllMemoryBankEntries(get_all_future1.GetCallback());
  std::vector<browser::context_hub::mojom::MemoryBankEntryPtr> entries1 =
      get_all_future1.Take();
  ASSERT_EQ(entries1.size(), 2u);

  std::vector<int64_t> entry_ids = {entries1[0]->id, entries1[1]->id};
  base::test::TestFuture<void> delete_future;
  handler_->DeleteMemoryBankEntries(entry_ids, delete_future.GetCallback());
  ASSERT_TRUE(delete_future.Wait());

  base::test::TestFuture<
      std::vector<browser::context_hub::mojom::MemoryBankEntryPtr>>
      get_all_future2;
  handler_->GetAllMemoryBankEntries(get_all_future2.GetCallback());
  std::vector<browser::context_hub::mojom::MemoryBankEntryPtr> entries2 =
      get_all_future2.Take();
  EXPECT_TRUE(entries2.empty());
}

}  // namespace
}  // namespace context_hub
