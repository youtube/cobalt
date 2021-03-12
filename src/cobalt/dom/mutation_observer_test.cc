// Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/mutation_observer_init.h"
#include "cobalt/dom/mutation_observer_task_manager.h"
#include "cobalt/dom/mutation_record.h"
#include "cobalt/dom/mutation_reporter.h"
#include "cobalt/dom/node_list.h"
#include "cobalt/dom/text.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "cobalt/test/empty_document.h"
#include "cobalt/test/mock_debugger_hooks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::SaveArg;

constexpr auto kOneshot = base::DebuggerHooks::AsyncTaskFrequency::kOneshot;

namespace cobalt {
namespace dom {
// Helper struct for childList mutations.
struct ChildListMutationArguments {
  scoped_refptr<dom::Element> previous_sibling;
  scoped_refptr<dom::Element> next_sibling;
  scoped_refptr<dom::NodeList> added_nodes;
  scoped_refptr<dom::NodeList> removed_nodes;
};

typedef ::testing::StrictMock<test::MockDebuggerHooks> DebuggerHooksMock;

class MutationCallbackMock {
 public:
  MOCK_METHOD2(NativeMutationCallback,
               void(const MutationObserver::MutationRecordSequence&,
                    const scoped_refptr<MutationObserver>&));
};

class MutationObserverTest : public ::testing::Test {
 protected:
  dom::Document* document() { return empty_document_.document(); }
  MutationObserverTaskManager* task_manager() { return &task_manager_; }
  DebuggerHooksMock* debugger_hooks() { return &debugger_hooks_; }
  MutationCallbackMock* callback_mock() { return &callback_mock_; }

  scoped_refptr<Element> CreateDiv() {
    scoped_refptr<Element> element = document()->CreateElement("div");
    DCHECK(element);
    document()->AppendChild(element);
    return element;
  }

  scoped_refptr<MutationObserver> CreateObserver() {
    return new MutationObserver(
        base::Bind(&MutationCallbackMock::NativeMutationCallback,
                   base::Unretained(&callback_mock_)),
        &task_manager_, debugger_hooks_);
  }

  ChildListMutationArguments CreateChildListMutationArguments() {
    // These nodes are not actually related in the way they are named
    // (next_sibling, previous_sibling, etc) but that is not relevant for the
    // purpose of these tests.
    ChildListMutationArguments args;
    args.previous_sibling = CreateDiv();
    args.next_sibling = CreateDiv();
    args.added_nodes = new NodeList();
    args.added_nodes->AppendNode(CreateDiv());
    args.removed_nodes = new NodeList();
    args.removed_nodes->AppendNode(CreateDiv());
    return args;
  }

 private:
  MutationObserverTaskManager task_manager_;
  DebuggerHooksMock debugger_hooks_;
  test::EmptyDocument empty_document_;
  MutationCallbackMock callback_mock_;
  base::MessageLoop message_loop_;
};

TEST_F(MutationObserverTest, CreateAttributeMutationRecord) {
  scoped_refptr<dom::Element> target = CreateDiv();
  scoped_refptr<MutationRecord> record =
      MutationRecord::CreateAttributeMutationRecord(target, "attribute_name",
                                                    std::string("old_value"));
  ASSERT_TRUE(record);

  // "type" and attribute-related attributes are set as expected
  EXPECT_STREQ("attributes", record->type().c_str());
  EXPECT_TRUE(record->attribute_name());
  EXPECT_STREQ("attribute_name", record->attribute_name()->c_str());
  EXPECT_TRUE(record->old_value());
  EXPECT_STREQ("old_value", record->old_value()->c_str());

  // "target" is set as expected.
  EXPECT_EQ(target, record->target());

  // Namespaces are not supported.
  EXPECT_FALSE(record->attribute_namespace());

  // Unrelated attributes are not set.
  ASSERT_TRUE(record->added_nodes());
  EXPECT_EQ(record->added_nodes()->length(), 0);
  ASSERT_TRUE(record->removed_nodes());
  EXPECT_EQ(record->removed_nodes()->length(), 0);
  EXPECT_FALSE(record->previous_sibling());
  EXPECT_FALSE(record->next_sibling());
}

TEST_F(MutationObserverTest, CreateCharacterDataMutationRecord) {
  scoped_refptr<dom::Element> target = CreateDiv();
  scoped_refptr<MutationRecord> record =
      MutationRecord::CreateCharacterDataMutationRecord(
          target, std::string("old_character_data"));
  ASSERT_TRUE(record);

  // "type" and attribute-related attributes are set as expected
  EXPECT_STREQ("characterData", record->type().c_str());
  EXPECT_TRUE(record->old_value());
  EXPECT_STREQ("old_character_data", record->old_value()->c_str());

  // "target" is set as expected.
  EXPECT_EQ(target, record->target());

  // Unrelated attributes are not set.
  EXPECT_FALSE(record->attribute_name());
  EXPECT_FALSE(record->attribute_namespace());
  ASSERT_TRUE(record->added_nodes());
  EXPECT_EQ(record->added_nodes()->length(), 0);
  ASSERT_TRUE(record->removed_nodes());
  EXPECT_EQ(record->removed_nodes()->length(), 0);
  EXPECT_FALSE(record->previous_sibling());
  EXPECT_FALSE(record->next_sibling());
}

TEST_F(MutationObserverTest, CreateChildListMutationRecord) {
  scoped_refptr<dom::Element> target = CreateDiv();

  ChildListMutationArguments args = CreateChildListMutationArguments();
  scoped_refptr<MutationRecord> record =
      MutationRecord::CreateChildListMutationRecord(
          target, args.added_nodes, args.removed_nodes, args.previous_sibling,
          args.next_sibling);
  ASSERT_TRUE(record);

  // "type" and attribute-related attributes are set as expected
  EXPECT_STREQ("childList", record->type().c_str());
  EXPECT_EQ(args.added_nodes, record->added_nodes());
  EXPECT_EQ(args.removed_nodes, record->removed_nodes());
  EXPECT_EQ(args.previous_sibling, record->previous_sibling());
  EXPECT_EQ(args.next_sibling, record->next_sibling());

  // "target" is set as expected.
  EXPECT_EQ(target, record->target());

  // Unrelated attributes are not set.
  EXPECT_FALSE(record->attribute_name());
  EXPECT_FALSE(record->attribute_namespace());
  EXPECT_FALSE(record->old_value());
}

TEST_F(MutationObserverTest, MutationObserverInit) {
  MutationObserverInit init;
  // Default values are set according to spec.
  EXPECT_FALSE(init.child_list());
  EXPECT_FALSE(init.subtree());

  // Other values are not set.
  EXPECT_FALSE(init.has_attributes());
  EXPECT_FALSE(init.has_character_data());
  EXPECT_FALSE(init.has_attribute_old_value());
  EXPECT_FALSE(init.has_character_data_old_value());
  EXPECT_FALSE(init.has_attribute_filter());

  // Set other values.
  init.set_attributes(true);
  init.set_character_data(true);
  init.set_attribute_old_value(true);
  init.set_character_data_old_value(true);
  script::Sequence<std::string> attribute_filter;
  attribute_filter.push_back("a_filter");
  init.set_attribute_filter(attribute_filter);

  // Other values are now set.
  EXPECT_TRUE(init.has_attributes());
  EXPECT_TRUE(init.attributes());
  EXPECT_TRUE(init.has_character_data());
  EXPECT_TRUE(init.character_data());
  EXPECT_TRUE(init.has_attribute_old_value());
  EXPECT_TRUE(init.attribute_old_value());
  EXPECT_TRUE(init.has_character_data_old_value());
  EXPECT_TRUE(init.character_data_old_value());
  EXPECT_TRUE(init.has_attribute_filter());
  EXPECT_EQ(init.attribute_filter().size(), attribute_filter.size());
  EXPECT_EQ(init.attribute_filter().at(0), attribute_filter.at(0));
}

TEST_F(MutationObserverTest, TakeRecords) {
  scoped_refptr<dom::Element> target = CreateDiv();

  // Newly created observer shouldn't have records.
  scoped_refptr<MutationObserver> observer = CreateObserver();
  MutationObserver::MutationRecordSequence records;
  records = observer->TakeRecords();
  EXPECT_TRUE(records.empty());

  // Append a mutation records.
  scoped_refptr<MutationRecord> record =
      MutationRecord::CreateCharacterDataMutationRecord(
          target, std::string("old_character_data"));
  const void* async_task;
  EXPECT_CALL(*debugger_hooks(),
              AsyncTaskScheduled(_, "characterData", kOneshot))
      .WillOnce(SaveArg<0>(&async_task));
  observer->QueueMutationRecord(record);

  // The queued record can be taken once.
  EXPECT_CALL(*debugger_hooks(), AsyncTaskCanceled(async_task));
  records = observer->TakeRecords();
  ASSERT_EQ(1, records.size());
  ASSERT_EQ(records.at(0), record);
  records = observer->TakeRecords();
  EXPECT_TRUE(records.empty());
}

TEST_F(MutationObserverTest, Notify) {
  scoped_refptr<dom::Element> target = CreateDiv();

  // Create a new observer and queue a mutation record.
  scoped_refptr<MutationObserver> observer = CreateObserver();
  scoped_refptr<MutationRecord> record =
      MutationRecord::CreateCharacterDataMutationRecord(
          target, std::string("old_character_data"));
  const void* async_task;
  EXPECT_CALL(*debugger_hooks(),
              AsyncTaskScheduled(_, "characterData", kOneshot))
      .WillOnce(SaveArg<0>(&async_task));
  observer->QueueMutationRecord(record);

  // Callback should be fired with the first argument being a sequence of the
  // queued record, and the second argument being the observer.
  MutationObserver::MutationRecordSequence records;
  EXPECT_CALL(*debugger_hooks(), AsyncTaskStarted(async_task));
  EXPECT_CALL(*callback_mock(), NativeMutationCallback(_, observer))
      .WillOnce(SaveArg<0>(&records));
  EXPECT_CALL(*debugger_hooks(), AsyncTaskFinished(async_task));
  observer->Notify();
  ASSERT_EQ(1, records.size());
  EXPECT_EQ(record, records.at(0));

  // There should be no more records queued up after the callback has been
  // fired.
  records = observer->TakeRecords();
  EXPECT_TRUE(records.empty());

  // Queue another mutation record on the same observer.
  record = MutationRecord::CreateAttributeMutationRecord(
      target, "attribute_name", std::string("old_attribute_data"));
  EXPECT_CALL(*debugger_hooks(), AsyncTaskScheduled(_, "attributes", kOneshot))
      .WillOnce(SaveArg<0>(&async_task));
  observer->QueueMutationRecord(record);

  // Check that the new record goes to the callback.
  EXPECT_CALL(*debugger_hooks(), AsyncTaskStarted(async_task));
  EXPECT_CALL(*callback_mock(), NativeMutationCallback(_, observer))
      .WillOnce(SaveArg<0>(&records));
  EXPECT_CALL(*debugger_hooks(), AsyncTaskFinished(async_task));
  observer->Notify();
  ASSERT_EQ(1, records.size());
  EXPECT_EQ(record, records.at(0));

  // No more records after notifying.
  records = observer->TakeRecords();
  EXPECT_TRUE(records.empty());
}

TEST_F(MutationObserverTest, ReportMutation) {
  scoped_refptr<dom::Element> target = CreateDiv();
  // Create a registered observer that cares about attribute and character data
  // mutations.
  scoped_refptr<MutationObserver> observer = CreateObserver();
  MutationObserverInit init;
  init.set_attributes(true);
  init.set_child_list(false);
  init.set_character_data(true);

  // Create a MutationReporter for the list of registered observers.
  std::unique_ptr<std::vector<RegisteredObserver> > registered_observers(
      new std::vector<RegisteredObserver>());
  registered_observers->push_back(
      RegisteredObserver(target.get(), observer, init));
  MutationReporter reporter(target.get(), std::move(registered_observers));

  // Report a few mutations.
  const void* async_task_1;
  const void* async_task_2;
  EXPECT_CALL(*debugger_hooks(), AsyncTaskScheduled(_, "attributes", kOneshot))
      .WillOnce(SaveArg<0>(&async_task_1));
  EXPECT_CALL(*debugger_hooks(),
              AsyncTaskScheduled(_, "characterData", kOneshot))
      .WillOnce(SaveArg<0>(&async_task_2));
  reporter.ReportAttributesMutation("attribute_name", std::string("old_value"));
  reporter.ReportCharacterDataMutation("old_character_data");
  ChildListMutationArguments args = CreateChildListMutationArguments();
  reporter.ReportChildListMutation(args.added_nodes, args.removed_nodes,
                                   args.previous_sibling, args.next_sibling);

  // Check that mutation records for the mutation types we care about have
  // been queued.
  EXPECT_CALL(*debugger_hooks(), AsyncTaskCanceled(async_task_1));
  EXPECT_CALL(*debugger_hooks(), AsyncTaskCanceled(async_task_2));
  MutationObserver::MutationRecordSequence records = observer->TakeRecords();
  ASSERT_EQ(2, records.size());
  EXPECT_EQ(records.at(0)->type(), "attributes");
  EXPECT_EQ(records.at(1)->type(), "characterData");
}

TEST_F(MutationObserverTest, AttributeFilter) {
  scoped_refptr<dom::Element> target = CreateDiv();
  // Create a registered observer that cares about attribute and character data
  // mutations.
  scoped_refptr<MutationObserver> observer = CreateObserver();
  MutationObserverInit init;
  script::Sequence<std::string> attribute_filter;
  attribute_filter.push_back("banana");
  attribute_filter.push_back("potato");
  init.set_attribute_filter(attribute_filter);
  init.set_attributes(true);

  // Create a MutationReporter for the list of registered observers.
  std::unique_ptr<std::vector<RegisteredObserver> > registered_observers(
      new std::vector<RegisteredObserver>());
  registered_observers->push_back(
      RegisteredObserver(target.get(), observer, init));
  MutationReporter reporter(target.get(), std::move(registered_observers));

  // Report a few attribute mutations, two of which will get through the filter.
  const void* async_task_1;
  const void* async_task_2;
  EXPECT_CALL(*debugger_hooks(), AsyncTaskScheduled(_, "attributes", kOneshot))
      .WillOnce(SaveArg<0>(&async_task_1))
      .WillOnce(SaveArg<0>(&async_task_2));
  reporter.ReportAttributesMutation("banana", std::string("rotten"));
  reporter.ReportAttributesMutation("apple", std::string("wormy"));
  reporter.ReportAttributesMutation("potato", std::string("mashed"));

  // Check that mutation records for the filtered attributes have been queued.
  EXPECT_CALL(*debugger_hooks(), AsyncTaskCanceled(async_task_1));
  EXPECT_CALL(*debugger_hooks(), AsyncTaskCanceled(async_task_2));
  MutationObserver::MutationRecordSequence records = observer->TakeRecords();
  ASSERT_EQ(2, records.size());
  EXPECT_STREQ(records.at(0)->attribute_name()->c_str(), "banana");
  EXPECT_STREQ(records.at(1)->attribute_name()->c_str(), "potato");
}

TEST_F(MutationObserverTest, RegisteredObserverList) {
  scoped_refptr<dom::Element> target = CreateDiv();
  scoped_refptr<MutationObserver> observer = CreateObserver();

  RegisteredObserverList observer_list(target.get());

  // Add an observer with options.
  MutationObserverInit options;
  options.set_attributes(true);
  EXPECT_TRUE(observer_list.AddMutationObserver(observer, options));
  EXPECT_EQ(1, observer_list.registered_observers().size());
  EXPECT_EQ(observer, observer_list.registered_observers()[0].observer());
  EXPECT_TRUE(observer_list.registered_observers()[0].options().attributes());
  EXPECT_FALSE(
      observer_list.registered_observers()[0].options().has_character_data());
  EXPECT_FALSE(observer_list.registered_observers()[0].options().child_list());

  // Adding the same observer updates the options.
  options = MutationObserverInit();
  options.set_child_list(true);
  EXPECT_TRUE(observer_list.AddMutationObserver(observer, options));
  EXPECT_EQ(1, observer_list.registered_observers().size());
  EXPECT_EQ(observer, observer_list.registered_observers()[0].observer());
  EXPECT_FALSE(
      observer_list.registered_observers()[0].options().has_attributes());
  EXPECT_FALSE(
      observer_list.registered_observers()[0].options().has_character_data());
  EXPECT_TRUE(observer_list.registered_observers()[0].options().child_list());

  // Remove the observer.
  observer_list.RemoveMutationObserver(observer);
  EXPECT_EQ(0, observer_list.registered_observers().size());
}

TEST_F(MutationObserverTest, LazilySetOptions) {
  scoped_refptr<dom::Element> target = CreateDiv();
  scoped_refptr<MutationObserver> observer = CreateObserver();

  RegisteredObserverList observer_list(target.get());

  // |attributes| gets set if an attribute-related option is set.
  MutationObserverInit options;
  options.set_attribute_old_value(true);
  EXPECT_TRUE(observer_list.AddMutationObserver(observer, options));
  EXPECT_EQ(1, observer_list.registered_observers().size());
  EXPECT_TRUE(observer_list.registered_observers()[0].options().attributes());

  // |character_data| gets set if an attribute-related option is set.
  options = MutationObserverInit();
  options.set_character_data_old_value(true);
  EXPECT_TRUE(observer_list.AddMutationObserver(observer, options));
  EXPECT_EQ(1, observer_list.registered_observers().size());
  EXPECT_TRUE(
      observer_list.registered_observers()[0].options().character_data());
}

TEST_F(MutationObserverTest, InvalidOptions) {
  scoped_refptr<dom::Element> target = CreateDiv();
  scoped_refptr<MutationObserver> observer = CreateObserver();

  RegisteredObserverList observer_list(target.get());

  // No type of mutation is set.
  MutationObserverInit options;
  EXPECT_FALSE(observer_list.AddMutationObserver(observer, options));
  EXPECT_EQ(0, observer_list.registered_observers().size());

  // |attributes| is set as false, but attribute old data is set.
  options.set_attributes(false);
  options.set_attribute_old_value(true);
  EXPECT_FALSE(observer_list.AddMutationObserver(observer, options));
  EXPECT_EQ(0, observer_list.registered_observers().size());

  // |character_data| is set as false, but attribute old data is set.
  options = MutationObserverInit();
  options.set_character_data(false);
  options.set_character_data_old_value(true);
  EXPECT_FALSE(observer_list.AddMutationObserver(observer, options));
  EXPECT_EQ(0, observer_list.registered_observers().size());
}

TEST_F(MutationObserverTest, InvalidOptionsRaisesException) {
  scoped_refptr<dom::Element> target = CreateDiv();
  scoped_refptr<MutationObserver> observer = CreateObserver();
  MutationObserverInit invalid_options;

  script::testing::MockExceptionState exception_state;
  EXPECT_CALL(exception_state, SetSimpleExceptionVA(script::kTypeError, _, _));
  observer->Observe(target, invalid_options, &exception_state);
}

TEST_F(MutationObserverTest, AddChildNodes) {
  scoped_refptr<Element> root = CreateDiv();
  scoped_refptr<MutationObserver> observer = CreateObserver();
  MutationObserverInit options;
  options.set_subtree(true);
  options.set_child_list(true);

  const void* async_task_1;
  const void* async_task_2;
  EXPECT_CALL(*debugger_hooks(), AsyncTaskScheduled(_, "childList", kOneshot))
      .WillOnce(SaveArg<0>(&async_task_1))
      .WillOnce(SaveArg<0>(&async_task_2));
  observer->Observe(root, options);

  scoped_refptr<Element> child1 = document()->CreateElement("div");
  scoped_refptr<Element> child2 = document()->CreateElement("div");
  ASSERT_TRUE(child1);
  ASSERT_TRUE(child2);

  root->AppendChild(child1);
  child1->AppendChild(child2);

  EXPECT_CALL(*debugger_hooks(), AsyncTaskCanceled(async_task_1));
  EXPECT_CALL(*debugger_hooks(), AsyncTaskCanceled(async_task_2));
  MutationObserver::MutationRecordSequence records = observer->TakeRecords();
  ASSERT_EQ(2, records.size());
  EXPECT_EQ("childList", records.at(0)->type());
  EXPECT_EQ(root, records.at(0)->target());
  ASSERT_TRUE(records.at(0)->removed_nodes());
  ASSERT_EQ(0, records.at(0)->removed_nodes()->length());
  ASSERT_TRUE(records.at(0)->added_nodes());
  ASSERT_EQ(1, records.at(0)->added_nodes()->length());
  EXPECT_EQ(child1, records.at(0)->added_nodes()->Item(0));

  EXPECT_EQ("childList", records.at(1)->type());
  EXPECT_EQ(child1, records.at(1)->target());
  ASSERT_TRUE(records.at(1)->removed_nodes());
  ASSERT_EQ(0, records.at(1)->removed_nodes()->length());
  ASSERT_TRUE(records.at(1)->added_nodes());
  ASSERT_EQ(1, records.at(1)->added_nodes()->length());
  EXPECT_EQ(child2, records.at(1)->added_nodes()->Item(0));
}

TEST_F(MutationObserverTest, RemoveChildNode) {
  scoped_refptr<Element> root = CreateDiv();
  scoped_refptr<Element> child1 = document()->CreateElement("div");
  scoped_refptr<Element> child2 = document()->CreateElement("div");
  ASSERT_TRUE(child1);
  ASSERT_TRUE(child2);

  root->AppendChild(child1);
  child1->AppendChild(child2);

  scoped_refptr<MutationObserver> observer = CreateObserver();
  MutationObserverInit options;
  options.set_subtree(true);
  options.set_child_list(true);

  const void* async_task;
  EXPECT_CALL(*debugger_hooks(), AsyncTaskScheduled(_, "childList", kOneshot))
      .WillOnce(SaveArg<0>(&async_task));
  observer->Observe(root, options);

  child1->RemoveChild(child2);

  EXPECT_CALL(*debugger_hooks(), AsyncTaskCanceled(async_task));
  MutationObserver::MutationRecordSequence records = observer->TakeRecords();
  ASSERT_EQ(1, records.size());
  EXPECT_EQ("childList", records.at(0)->type());
  EXPECT_EQ(child1, records.at(0)->target());
  ASSERT_TRUE(records.at(0)->removed_nodes());
  ASSERT_EQ(1, records.at(0)->removed_nodes()->length());
  EXPECT_EQ(child2, records.at(0)->removed_nodes()->Item(0));
}

TEST_F(MutationObserverTest, MutateCharacterData) {
  scoped_refptr<Element> root = CreateDiv();
  scoped_refptr<Text> text = document()->CreateTextNode("initial-data");
  ASSERT_TRUE(text);
  root->AppendChild(text);

  scoped_refptr<MutationObserver> observer = CreateObserver();
  MutationObserverInit options;
  options.set_subtree(true);
  options.set_character_data(true);

  const void* async_task;
  EXPECT_CALL(*debugger_hooks(),
              AsyncTaskScheduled(_, "characterData", kOneshot))
      .WillOnce(SaveArg<0>(&async_task));
  observer->Observe(root, options);

  text->set_data("new-data");

  EXPECT_CALL(*debugger_hooks(), AsyncTaskCanceled(async_task));
  MutationObserver::MutationRecordSequence records = observer->TakeRecords();
  ASSERT_EQ(1, records.size());
  EXPECT_EQ("characterData", records.at(0)->type());
  EXPECT_EQ(text, records.at(0)->target());
  EXPECT_FALSE(records.at(0)->old_value());
}

TEST_F(MutationObserverTest, MutateCharacterDataWithOldValue) {
  scoped_refptr<Element> root = CreateDiv();
  scoped_refptr<Text> text = document()->CreateTextNode("initial-data");
  ASSERT_TRUE(text);
  root->AppendChild(text);

  scoped_refptr<MutationObserver> observer = CreateObserver();
  MutationObserverInit options;
  options.set_subtree(true);
  options.set_character_data_old_value(true);

  const void* async_task;
  EXPECT_CALL(*debugger_hooks(),
              AsyncTaskScheduled(_, "characterData", kOneshot))
      .WillOnce(SaveArg<0>(&async_task));
  observer->Observe(root, options);

  text->set_data("new-data");

  EXPECT_CALL(*debugger_hooks(), AsyncTaskCanceled(async_task));
  MutationObserver::MutationRecordSequence records = observer->TakeRecords();
  ASSERT_EQ(1, records.size());
  EXPECT_EQ("characterData", records.at(0)->type());
  EXPECT_EQ(text, records.at(0)->target());
  ASSERT_TRUE(records.at(0)->old_value());
  EXPECT_STREQ("initial-data", records.at(0)->old_value()->c_str());
}

TEST_F(MutationObserverTest, MutateAttribute) {
  scoped_refptr<Element> root = CreateDiv();
  root->SetAttribute("banana", "purple");
  root->SetAttribute("apple", "green");

  scoped_refptr<MutationObserver> observer = CreateObserver();
  script::Sequence<std::string> filter;
  filter.push_back("banana");
  MutationObserverInit options;
  options.set_attributes(true);
  options.set_attribute_filter(filter);

  const void* async_task;
  EXPECT_CALL(*debugger_hooks(), AsyncTaskScheduled(_, "attributes", kOneshot))
      .WillOnce(SaveArg<0>(&async_task));
  observer->Observe(root, options);

  root->SetAttribute("banana", "yellow");
  root->SetAttribute("apple", "brown");

  EXPECT_CALL(*debugger_hooks(), AsyncTaskCanceled(async_task));
  MutationObserver::MutationRecordSequence records = observer->TakeRecords();
  ASSERT_EQ(1, records.size());
  EXPECT_EQ("attributes", records.at(0)->type());
  EXPECT_EQ(root, records.at(0)->target());
  EXPECT_FALSE(records.at(0)->old_value());
  ASSERT_TRUE(records.at(0)->attribute_name());
  EXPECT_STREQ("banana", records.at(0)->attribute_name()->c_str());
}

TEST_F(MutationObserverTest, MutateAttributeWithOldValue) {
  scoped_refptr<Element> root = CreateDiv();
  root->SetAttribute("banana", "purple");

  scoped_refptr<MutationObserver> observer = CreateObserver();
  MutationObserverInit options;
  options.set_attribute_old_value(true);

  const void* async_task;
  EXPECT_CALL(*debugger_hooks(), AsyncTaskScheduled(_, "attributes", kOneshot))
      .WillOnce(SaveArg<0>(&async_task));
  observer->Observe(root, options);

  root->SetAttribute("banana", "yellow");

  EXPECT_CALL(*debugger_hooks(), AsyncTaskCanceled(async_task));
  MutationObserver::MutationRecordSequence records = observer->TakeRecords();
  ASSERT_EQ(1, records.size());
  EXPECT_EQ("attributes", records.at(0)->type());
  EXPECT_EQ(root, records.at(0)->target());
  ASSERT_TRUE(records.at(0)->old_value());
  ASSERT_TRUE(records.at(0)->attribute_name());
  EXPECT_STREQ("banana", records.at(0)->attribute_name()->c_str());
  EXPECT_STREQ("purple", records.at(0)->old_value()->c_str());
}

TEST_F(MutationObserverTest, Disconnect) {
  scoped_refptr<Element> root = CreateDiv();
  scoped_refptr<Text> text = document()->CreateTextNode("initial-data");
  ASSERT_TRUE(text);
  root->AppendChild(text);

  scoped_refptr<MutationObserver> observer = CreateObserver();
  MutationObserverInit options;
  options.set_subtree(true);
  options.set_character_data(true);

  const void* async_task;
  EXPECT_CALL(*debugger_hooks(),
              AsyncTaskScheduled(_, "characterData", kOneshot))
      .WillOnce(SaveArg<0>(&async_task));
  observer->Observe(root, options);

  // This should queue up a mutation record.
  text->set_data("new-data");

  EXPECT_CALL(*debugger_hooks(), AsyncTaskCanceled(async_task));
  observer->Disconnect();
  MutationObserver::MutationRecordSequence records = observer->TakeRecords();
  // MutationObserver.disconnect() should clear any queued records.
  EXPECT_EQ(0, records.size());

  // This should not queue a mutation record.
  text->set_data("more-new-data");
  records = observer->TakeRecords();
  EXPECT_EQ(0, records.size());
}

}  // namespace dom
}  // namespace cobalt
