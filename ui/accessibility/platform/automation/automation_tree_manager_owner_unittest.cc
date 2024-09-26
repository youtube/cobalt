// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/automation/automation_tree_manager_owner.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "gin/array_buffer.h"
#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"
#include "gin/v8_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/platform/automation/automation_v8_bindings.h"
#include "ui/accessibility/platform/automation/automation_v8_router.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-template.h"

namespace ui {

// Tests will be run against this class which overrides
// AutomationTreeManagerOwner.
class FakeAutomationTreeManagerOwner : public AutomationTreeManagerOwner {
 public:
  FakeAutomationTreeManagerOwner() = default;
  FakeAutomationTreeManagerOwner(const FakeAutomationTreeManagerOwner&) =
      delete;
  FakeAutomationTreeManagerOwner& operator=(
      const FakeAutomationTreeManagerOwner&) = delete;
  ~FakeAutomationTreeManagerOwner() override = default;

  // AutomationTreeManagerOwner:
  AutomationV8Bindings* GetAutomationV8Bindings() const override {
    return automation_v8_bindings_;
  }

  void NotifyTreeEventListenersChanged() override {}

  // For testing:
  void SetAutomationV8Bindings(AutomationV8Bindings* bindings) {
    automation_v8_bindings_ = bindings;
  }

 private:
  raw_ptr<AutomationV8Bindings> automation_v8_bindings_ = nullptr;
};

// A skeleton AutomationV8Router implementation for use by a test.
// Starts V8 when constructed. Does not construct a context and will
// fail the test if GetContext() is called.
class FakeAutomationV8Router : public AutomationV8Router {
 public:
  FakeAutomationV8Router() {
    if (!gin::IsolateHolder::Initialized()) {
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
      gin::V8Initializer::LoadV8Snapshot();
#endif
      gin::IsolateHolder::Initialize(
          gin::IsolateHolder::kNonStrictMode,
          gin::ArrayBufferAllocator::SharedInstance());
    }
    isolate_holder_ = std::make_unique<gin::IsolateHolder>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        gin::IsolateHolder::kSingleThread,
        gin::IsolateHolder::IsolateType::kUtility);
  }
  FakeAutomationV8Router(const FakeAutomationV8Router&) = delete;
  FakeAutomationV8Router& operator=(const FakeAutomationV8Router&) = delete;
  virtual ~FakeAutomationV8Router() = default;

  // AutomationV8Router:
  void ThrowInvalidArgumentsException(bool is_fatal = true) const override {}

  v8::Isolate* GetIsolate() const override {
    return isolate_holder_->isolate();
  }

  v8::Local<v8::Context> GetContext() const override {
    DCHECK(context_holder_) << "V8 context was not initialized for this test.";
    return context_holder_->context();
  }

  void StartCachingAccessibilityTrees() override {}

  void StopCachingAccessibilityTrees() override {}

  TreeChangeObserverFilter ParseTreeChangeObserverFilter(
      const std::string& filter) const override {
    return TreeChangeObserverFilter::kAllTreeChanges;
  }

  std::string GetMarkerTypeString(ax::mojom::MarkerType type) const override {
    return ui::ToString(type);
  }

  std::string GetFocusedStateString() const override { return "focused"; }

  std::string GetOffscreenStateString() const override { return "offscreen"; }

  std::string GetLocalizedStringForImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus status) const override {
    return ui::ToString(status);
  }

  std::string GetTreeChangeTypeString(
      ax::mojom::Mutation change_type) const override {
    return ui::ToString(change_type);
  }

  std::string GetEventTypeString(
      const std::tuple<ax::mojom::Event, AXEventGenerator::Event>& event_type)
      const override {
    std::string first = ui::ToString(std::get<0>(event_type));
    std::string second = ui::ToString(std::get<1>(event_type));
    return first + " " + second;
  }

  void RouteHandlerFunction(const std::string& name,
                            scoped_refptr<V8HandlerFunctionWrapper>
                                handler_function_wrapper) override {}

  void DispatchEvent(const std::string& event_name,
                     const base::Value::List& event_args) const override {
    if (!notify_event_ ||
        event_name != "automationInternal.onAccessibilityEvent")
      return;

    const base::Value::Dict* dict = event_args[0].GetIfDict();
    DCHECK(dict);
    const std::string* event_type_string = dict->FindString("eventType");
    DCHECK(event_type_string);
    notify_event_.Run(*event_type_string);
  }

  // For tests.
  void AddEventCallback(
      base::RepeatingCallback<void(const std::string&)> callback) {
    notify_event_ = std::move(callback);
  }

 private:
  std::unique_ptr<gin::IsolateHolder> isolate_holder_;
  std::unique_ptr<gin::ContextHolder> context_holder_;
  base::RepeatingCallback<void(const std::string&)> notify_event_;
};

// Tests for AutomationTreeManagerOwner.
class AutomationTreeManagerOwnerTest : public testing::Test {
 public:
  AutomationTreeManagerOwnerTest() = default;
  AutomationTreeManagerOwnerTest(const AutomationTreeManagerOwnerTest&) =
      delete;
  AutomationTreeManagerOwnerTest& operator=(
      const AutomationTreeManagerOwnerTest&) = delete;
  ~AutomationTreeManagerOwnerTest() override = default;

  void SetUp() override {
    tree_manager_owner_ = std::make_unique<FakeAutomationTreeManagerOwner>();
    router_ = std::make_unique<FakeAutomationV8Router>();
    bindings_ = std::make_unique<AutomationV8Bindings>(
        tree_manager_owner_.get(), router_.get());
    tree_manager_owner_->SetAutomationV8Bindings(bindings_.get());
  }

  void TearDown() override {
    tree_manager_owner_->SetAutomationV8Bindings(nullptr);
  }

 protected:
  std::map<ui::AXTreeID, std::unique_ptr<ui::AutomationAXTreeWrapper>>&
  GetTreeIDToTreeMap() {
    return tree_manager_owner_->tree_id_to_tree_wrapper_map_;
  }

  void SendOnAccessibilityEvents(const ui::AXTreeID& tree_id,
                                 const std::vector<ui::AXTreeUpdate>& updates,
                                 const gfx::Point& mouse_location,
                                 const std::vector<ui::AXEvent>& events) {
    tree_manager_owner_->OnAccessibilityEvents(tree_id, events, updates,
                                               mouse_location, true);
  }

  bool CallGetFocusInternal(ui::AutomationAXTreeWrapper* top_wrapper,
                            ui::AutomationAXTreeWrapper** focused_wrapper,
                            ui::AXNode** focused_node) {
    return tree_manager_owner_->GetFocusInternal(top_wrapper, focused_wrapper,
                                                 focused_node);
  }

  gfx::Rect CallComputeGlobalNodeBounds(ui::AutomationAXTreeWrapper* wrapper,
                                        ui::AXNode* node) {
    return tree_manager_owner_->ComputeGlobalNodeBounds(wrapper, node);
  }

  std::vector<ui::AXNode*> CallGetRootsOfChildTree(ui::AXNode* node) {
    return tree_manager_owner_->GetRootsOfChildTree(node);
  }

  void AddEventCallback(
      base::RepeatingCallback<void(const std::string&)> callback) {
    router_->AddEventCallback(std::move(callback));
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeAutomationTreeManagerOwner> tree_manager_owner_;
  std::unique_ptr<FakeAutomationV8Router> router_;
  std::unique_ptr<AutomationV8Bindings> bindings_;
};

TEST_F(AutomationTreeManagerOwnerTest, GetDesktop) {
  EXPECT_TRUE(GetTreeIDToTreeMap().empty());

  std::vector<ui::AXTreeUpdate> updates;
  updates.emplace_back();
  auto& tree_update = updates.back();
  auto& tree_data = tree_update.tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  tree_update.root_id = 1;
  tree_update.nodes.emplace_back();
  auto& node_data = tree_update.nodes.back();
  node_data.role = ax::mojom::Role::kDesktop;
  node_data.id = 1;
  std::vector<ui::AXEvent> events;
  SendOnAccessibilityEvents(tree_data.tree_id, updates, gfx::Point(), events);

  ASSERT_EQ(1U, GetTreeIDToTreeMap().size());

  ui::AutomationAXTreeWrapper* desktop =
      GetTreeIDToTreeMap().begin()->second.get();
  ASSERT_TRUE(desktop);
  EXPECT_TRUE(desktop->IsDesktopTree());
}

TEST_F(AutomationTreeManagerOwnerTest, GetFocusOneTree) {
  // A desktop tree with focus on a button.
  std::vector<ui::AXTreeUpdate> updates;
  updates.emplace_back();
  auto& tree_update = updates.back();
  tree_update.has_tree_data = true;
  tree_update.root_id = 1;
  auto& tree_data = tree_update.tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  tree_data.focus_id = 2;
  tree_update.nodes.emplace_back();
  auto& node_data1 = tree_update.nodes.back();
  node_data1.id = 1;
  node_data1.role = ax::mojom::Role::kDesktop;
  node_data1.child_ids.push_back(2);
  tree_update.nodes.emplace_back();
  auto& node_data2 = tree_update.nodes.back();
  node_data2.id = 2;
  node_data2.role = ax::mojom::Role::kButton;
  std::vector<ui::AXEvent> events;
  SendOnAccessibilityEvents(tree_data.tree_id, updates, gfx::Point(), events);

  ASSERT_EQ(1U, GetTreeIDToTreeMap().size());

  ui::AutomationAXTreeWrapper* desktop =
      GetTreeIDToTreeMap().begin()->second.get();
  ASSERT_TRUE(desktop);

  ui::AutomationAXTreeWrapper* focused_wrapper = nullptr;
  ui::AXNode* focused_node = nullptr;
  CallGetFocusInternal(desktop, &focused_wrapper, &focused_node);
  ASSERT_TRUE(focused_wrapper);
  ASSERT_TRUE(focused_node);
  EXPECT_EQ(desktop, focused_wrapper);
  EXPECT_EQ(ax::mojom::Role::kButton, focused_node->GetRole());

  // Push an update where we change the focus.
  focused_wrapper = nullptr;
  focused_node = nullptr;
  tree_data.focus_id = 1;
  SendOnAccessibilityEvents(tree_data.tree_id, updates, gfx::Point(), events);
  CallGetFocusInternal(desktop, &focused_wrapper, &focused_node);
  ASSERT_TRUE(focused_wrapper);
  ASSERT_TRUE(focused_node);
  EXPECT_EQ(desktop, focused_wrapper);
  EXPECT_EQ(ax::mojom::Role::kDesktop, focused_node->GetRole());

  // Push an update where we change the focus to nothing.
  focused_wrapper = nullptr;
  focused_node = nullptr;
  tree_data.focus_id = 100;
  SendOnAccessibilityEvents(tree_data.tree_id, updates, gfx::Point(), events);
  CallGetFocusInternal(desktop, &focused_wrapper, &focused_node);
  ASSERT_FALSE(focused_wrapper);
  ASSERT_FALSE(focused_node);
}

TEST_F(AutomationTreeManagerOwnerTest,
       GetFocusMultipleTreesChildTreeConstruction) {
  // Three trees each with a button and link.
  std::vector<std::vector<ui::AXTreeUpdate>> updates_list;
  for (int i = 0; i < 3; i++) {
    std::vector<ui::AXTreeUpdate>& updates = updates_list.emplace_back();
    updates.emplace_back();
    auto& tree_update = updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;

    // This is a point of inconsistency as the mojo representation allows
    // updates from multiple trees.
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    tree_data.focus_id = 2;
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role = ax::mojom::Role::kRootWebArea;
    node_data1.child_ids.push_back(2);
    node_data1.child_ids.push_back(3);
    tree_update.nodes.emplace_back();
    auto& node_data2 = tree_update.nodes.back();
    node_data2.id = 2;
    node_data2.role = ax::mojom::Role::kButton;
    tree_update.nodes.emplace_back();
    auto& node_data3 = tree_update.nodes.back();
    node_data3.id = 3;
    node_data3.role = ax::mojom::Role::kLink;
  }

  // Link up the trees so that the first is a parent of the other two using
  // child tree id.
  ui::AXTreeID tree_0_id = updates_list[0][0].tree_data.tree_id;
  ui::AXTreeID tree_1_id = updates_list[1][0].tree_data.tree_id;
  ui::AXTreeID tree_2_id = updates_list[2][0].tree_data.tree_id;
  updates_list[0][0].nodes[1].AddChildTreeId(tree_1_id);
  updates_list[0][0].nodes[2].AddChildTreeId(tree_2_id);

  std::vector<ui::AXEvent> empty_events;
  for (auto& updates : updates_list) {
    SendOnAccessibilityEvents(updates[0].tree_data.tree_id, updates,
                              gfx::Point(), empty_events);
  }

  ASSERT_EQ(3U, GetTreeIDToTreeMap().size());

  ui::AutomationAXTreeWrapper* wrapper_0 =
      GetTreeIDToTreeMap()[tree_0_id].get();
  ASSERT_TRUE(wrapper_0);
  ui::AutomationAXTreeWrapper* wrapper_1 =
      GetTreeIDToTreeMap()[tree_1_id].get();
  ASSERT_TRUE(wrapper_1);
  ui::AutomationAXTreeWrapper* wrapper_2 =
      GetTreeIDToTreeMap()[tree_2_id].get();
  ASSERT_TRUE(wrapper_2);

  ui::AutomationAXTreeWrapper* focused_wrapper = nullptr;
  ui::AXNode* focused_node = nullptr;
  CallGetFocusInternal(wrapper_0, &focused_wrapper, &focused_node);
  ASSERT_TRUE(focused_wrapper);
  ASSERT_TRUE(focused_node);
  EXPECT_EQ(wrapper_1, focused_wrapper);
  EXPECT_EQ(tree_1_id, focused_node->tree()->GetAXTreeID());
  EXPECT_EQ(ax::mojom::Role::kButton, focused_node->GetRole());

  // Push an update where we change the focus.
  focused_wrapper = nullptr;
  focused_node = nullptr;

  // The link in wrapper 0 which has a child tree id pointing to wrapper 2.
  updates_list[0][0].tree_data.focus_id = 3;
  SendOnAccessibilityEvents(updates_list[0][0].tree_data.tree_id,
                            updates_list[0], gfx::Point(), empty_events);
  CallGetFocusInternal(wrapper_0, &focused_wrapper, &focused_node);
  ASSERT_TRUE(focused_wrapper);
  ASSERT_TRUE(focused_node);
  EXPECT_EQ(wrapper_2, focused_wrapper);
  EXPECT_EQ(tree_2_id, focused_node->tree()->GetAXTreeID());
  EXPECT_EQ(ax::mojom::Role::kButton, focused_node->GetRole());
}

TEST_F(AutomationTreeManagerOwnerTest, GetFocusMultipleTreesAppIdConstruction) {
  // Three trees each with a button and link.
  std::vector<std::vector<ui::AXTreeUpdate>> updates_list;
  for (int i = 0; i < 3; i++) {
    std::vector<ui::AXTreeUpdate>& updates = updates_list.emplace_back();
    updates.emplace_back();
    auto& tree_update = updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;

    // This is a point of inconsistency as the mojo representation allows
    // updates from ultiple trees.
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    tree_data.focus_id = 2;
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role = ax::mojom::Role::kRootWebArea;
    node_data1.child_ids.push_back(2);
    node_data1.child_ids.push_back(3);
    tree_update.nodes.emplace_back();
    auto& node_data2 = tree_update.nodes.back();
    node_data2.id = 2;
    node_data2.role = ax::mojom::Role::kButton;
    tree_update.nodes.emplace_back();
    auto& node_data3 = tree_update.nodes.back();
    node_data3.id = 3;
    node_data3.role = ax::mojom::Role::kLink;
  }

  // Link up the trees so that the first is a parent of the other two using app
  // ids.
  ui::AXTreeID tree_0_id = updates_list[0][0].tree_data.tree_id;
  ui::AXTreeID tree_1_id = updates_list[1][0].tree_data.tree_id;
  ui::AXTreeID tree_2_id = updates_list[2][0].tree_data.tree_id;
  auto& wrapper0_button_data = updates_list[0][0].nodes[1];
  auto& wrapper0_link_data = updates_list[0][0].nodes[2];
  auto& wrapper1_link_data = updates_list[1][0].nodes[2];
  auto& wrapper2_button_data = updates_list[2][0].nodes[1];

  // This construction requires the hosting and client nodes annotate with the
  // same app id.
  wrapper0_button_data.AddStringAttribute(
      ax::mojom::StringAttribute::kChildTreeNodeAppId, "app1");
  wrapper1_link_data.AddStringAttribute(ax::mojom::StringAttribute::kAppId,
                                        "app1");
  wrapper0_link_data.AddStringAttribute(
      ax::mojom::StringAttribute::kChildTreeNodeAppId, "app2");
  wrapper2_button_data.AddStringAttribute(ax::mojom::StringAttribute::kAppId,
                                          "app2");

  std::vector<ui::AXEvent> empty_events;
  for (auto& updates : updates_list) {
    SendOnAccessibilityEvents(updates[0].tree_data.tree_id, updates,
                              gfx::Point(), empty_events);
  }

  ASSERT_EQ(3U, GetTreeIDToTreeMap().size());

  ui::AutomationAXTreeWrapper* wrapper_0 =
      GetTreeIDToTreeMap()[tree_0_id].get();
  ASSERT_TRUE(wrapper_0);
  ui::AutomationAXTreeWrapper* wrapper_1 =
      GetTreeIDToTreeMap()[tree_1_id].get();
  ASSERT_TRUE(wrapper_1);
  ui::AutomationAXTreeWrapper* wrapper_2 =
      GetTreeIDToTreeMap()[tree_2_id].get();
  ASSERT_TRUE(wrapper_2);

  ui::AutomationAXTreeWrapper* focused_wrapper = nullptr;
  ui::AXNode* focused_node = nullptr;
  CallGetFocusInternal(wrapper_0, &focused_wrapper, &focused_node);
  ASSERT_TRUE(focused_wrapper);
  ASSERT_TRUE(focused_node);
  EXPECT_EQ(wrapper_1, focused_wrapper);
  EXPECT_EQ(tree_1_id, focused_node->tree()->GetAXTreeID());

  // This is an interesting inconsistency as this node is technically not in the
  // app (which starts at the link in wrapper 1).
  EXPECT_EQ(ax::mojom::Role::kButton, focused_node->GetRole());

  // Push an update where we change the focus.
  focused_wrapper = nullptr;
  focused_node = nullptr;

  // The link in wrapper 0 which has a child tree id pointing to wrapper 2.
  updates_list[0][0].tree_data.focus_id = 3;
  SendOnAccessibilityEvents(updates_list[0][0].tree_data.tree_id,
                            updates_list[0], gfx::Point(), empty_events);
  CallGetFocusInternal(wrapper_0, &focused_wrapper, &focused_node);
  ASSERT_TRUE(focused_wrapper);
  ASSERT_TRUE(focused_node);
  EXPECT_EQ(wrapper_2, focused_wrapper);
  EXPECT_EQ(tree_2_id, focused_node->tree()->GetAXTreeID());
  EXPECT_EQ(ax::mojom::Role::kButton, focused_node->GetRole());
}

TEST_F(AutomationTreeManagerOwnerTest, GetBoundsAppIdConstruction) {
  // two trees each with a button.
  std::vector<std::vector<ui::AXTreeUpdate>> updates_list;
  for (int i = 0; i < 2; i++) {
    std::vector<ui::AXTreeUpdate>& updates = updates_list.emplace_back();
    updates.emplace_back();
    auto& tree_update = updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role =
        i == 0 ? ax::mojom::Role::kDesktop : ax::mojom::Role::kRootWebArea;
    node_data1.child_ids.push_back(2);
    node_data1.relative_bounds.bounds = gfx::RectF(100, 100, 100, 100);
    tree_update.nodes.emplace_back();
    auto& node_data2 = tree_update.nodes.back();
    node_data2.id = 2;
    node_data2.role = ax::mojom::Role::kButton;
    node_data2.relative_bounds.bounds = gfx::RectF(0, 0, 200, 200);
  }

  // Link up the trees by app id.
  ui::AXTreeID tree_0_id = updates_list[0][0].tree_data.tree_id;
  ui::AXTreeID tree_1_id = updates_list[1][0].tree_data.tree_id;
  auto& wrapper0_button_data = updates_list[0][0].nodes[1];
  auto& wrapper1_button_data = updates_list[1][0].nodes[1];

  // This construction requires the hosting and client nodes annotate with the
  // same app id.
  wrapper0_button_data.AddStringAttribute(
      ax::mojom::StringAttribute::kChildTreeNodeAppId, "app1");
  wrapper1_button_data.AddStringAttribute(ax::mojom::StringAttribute::kAppId,
                                          "app1");

  wrapper0_button_data.AddFloatAttribute(
      ax::mojom::FloatAttribute::kChildTreeScale, 2.0);

  std::vector<ui::AXEvent> empty_events;
  for (auto& updates : updates_list) {
    SendOnAccessibilityEvents(updates[0].tree_data.tree_id, updates,
                              gfx::Point(), empty_events);
  }

  ASSERT_EQ(2U, GetTreeIDToTreeMap().size());

  ui::AutomationAXTreeWrapper* wrapper_0 =
      GetTreeIDToTreeMap()[tree_0_id].get();
  ASSERT_TRUE(wrapper_0);
  ui::AutomationAXTreeWrapper* wrapper_1 =
      GetTreeIDToTreeMap()[tree_1_id].get();
  ASSERT_TRUE(wrapper_1);

  ui::AXNode* wrapper1_button = wrapper_1->ax_tree()->GetFromId(2);
  ASSERT_TRUE(wrapper1_button);

  // The button in wrapper 1 is scaled by .5 (200 * .5). It's root is also
  // scaled (100 * .5). In wrapper 0, it is *not* offset by the tree's root
  // bounds.
  EXPECT_EQ(gfx::Rect(50, 50, 100, 100),
            CallComputeGlobalNodeBounds(wrapper_1, wrapper1_button));
}

TEST_F(AutomationTreeManagerOwnerTest, GetBoundsNestedAppIdConstruction) {
  // two trees each with a button and a client node.
  std::vector<std::vector<ui::AXTreeUpdate>> updates_list;
  for (int i = 0; i < 2; i++) {
    std::vector<ui::AXTreeUpdate>& updates = updates_list.emplace_back();
    updates.emplace_back();
    auto& tree_update = updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role =
        i == 0 ? ax::mojom::Role::kDesktop : ax::mojom::Role::kRootWebArea;
    node_data1.child_ids.push_back(2);
    node_data1.child_ids.push_back(3);
    node_data1.relative_bounds.bounds = gfx::RectF(100, 100, 100, 100);
    tree_update.nodes.emplace_back();
    auto& node_data2 = tree_update.nodes.back();
    node_data2.id = 2;
    node_data2.role = ax::mojom::Role::kButton;
    node_data2.relative_bounds.bounds = gfx::RectF(0, 0, 200, 200);
    tree_update.nodes.emplace_back();
    auto& node_data3 = tree_update.nodes.back();
    node_data3.id = 3;
    node_data3.role = ax::mojom::Role::kClient;
    node_data3.relative_bounds.bounds = gfx::RectF(0, 0, 200, 200);
  }

  // Link up the trees by app id. One button -> child button; client -> child
  // root.
  ui::AXTreeID tree_0_id = updates_list[0][0].tree_data.tree_id;
  ui::AXTreeID tree_1_id = updates_list[1][0].tree_data.tree_id;
  auto& wrapper0_button_data = updates_list[0][0].nodes[1];
  auto& wrapper0_client_data = updates_list[0][0].nodes[2];
  auto& wrapper1_root_data = updates_list[1][0].nodes[0];
  auto& wrapper1_button_data = updates_list[1][0].nodes[1];

  // This construction requires the hosting and client nodes annotate with the
  // same app id.
  wrapper0_button_data.AddStringAttribute(
      ax::mojom::StringAttribute::kChildTreeNodeAppId, "app1");
  wrapper1_button_data.AddStringAttribute(ax::mojom::StringAttribute::kAppId,
                                          "app1");

  wrapper0_button_data.AddFloatAttribute(
      ax::mojom::FloatAttribute::kChildTreeScale, 2.0);

  // Adding this app id should not impact the above bounds computation.
  wrapper0_client_data.AddStringAttribute(
      ax::mojom::StringAttribute::kChildTreeNodeAppId, "lacrosHost");
  wrapper1_root_data.AddStringAttribute(ax::mojom::StringAttribute::kAppId,
                                        "lacrosHost");

  std::vector<ui::AXEvent> empty_events;
  for (auto& updates : updates_list) {
    SendOnAccessibilityEvents(updates[0].tree_data.tree_id, updates,
                              gfx::Point(), empty_events);
  }

  ASSERT_EQ(2U, GetTreeIDToTreeMap().size());

  ui::AutomationAXTreeWrapper* wrapper_0 =
      GetTreeIDToTreeMap()[tree_0_id].get();
  ASSERT_TRUE(wrapper_0);
  ui::AutomationAXTreeWrapper* wrapper_1 =
      GetTreeIDToTreeMap()[tree_1_id].get();
  ASSERT_TRUE(wrapper_1);

  ui::AXNode* wrapper1_button = wrapper_1->ax_tree()->GetFromId(2);
  ASSERT_TRUE(wrapper1_button);

  // The button in wrapper 1 is scaled by .5 (200 * .5). It's root is also
  // scaled (100 * .5). In wrapper 0, it is *not* offset by the tree's root
  // bounds.
  EXPECT_EQ(gfx::Rect(50, 50, 100, 100),
            CallComputeGlobalNodeBounds(wrapper_1, wrapper1_button));

  ui::AXNode* wrapper1_root = wrapper_1->ax_tree()->GetFromId(1);
  ASSERT_TRUE(wrapper1_root);

  // Similar to the button, but not scaled. This does not cross an app id
  // boundary, so is also offset by the parent tree's root (100 + 100).
  EXPECT_EQ(gfx::Rect(200, 200, 100, 100),
            CallComputeGlobalNodeBounds(wrapper_1, wrapper1_root));
}

TEST_F(AutomationTreeManagerOwnerTest, IgnoredAncestorTrees) {
  // Three trees each with a button and link.
  std::vector<std::vector<ui::AXTreeUpdate>> updates_list;
  for (int i = 0; i < 3; i++) {
    std::vector<ui::AXTreeUpdate>& updates = updates_list.emplace_back();
    updates.emplace_back();
    auto& tree_update = updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;

    // This is a point of inconsistency as the mojo representation allows
    // updates from multiple trees.
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    tree_data.focus_id = 2;
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role = ax::mojom::Role::kRootWebArea;
    node_data1.child_ids.push_back(2);
    node_data1.child_ids.push_back(3);
    tree_update.nodes.emplace_back();
    auto& node_data2 = tree_update.nodes.back();
    node_data2.id = 2;
    node_data2.role = ax::mojom::Role::kButton;
    tree_update.nodes.emplace_back();
    auto& node_data3 = tree_update.nodes.back();
    node_data3.id = 3;
    node_data3.role = ax::mojom::Role::kLink;
  }

  // Link up the trees so that the first is a parent of the second and the
  // second a parent of the third.
  ui::AXTreeID tree_0_id = updates_list[0][0].tree_data.tree_id;
  ui::AXTreeID tree_1_id = updates_list[1][0].tree_data.tree_id;
  ui::AXTreeID tree_2_id = updates_list[2][0].tree_data.tree_id;
  updates_list[0][0].nodes[1].AddChildTreeId(tree_1_id);

  // Make the hosting node ignored.
  updates_list[0][0].nodes[1].AddState(ax::mojom::State::kInvisible);

  updates_list[1][0].nodes[1].AddChildTreeId(tree_2_id);

  std::vector<ui::AXEvent> empty_events;
  for (auto& updates : updates_list) {
    SendOnAccessibilityEvents(updates[0].tree_data.tree_id, updates,
                              gfx::Point(), empty_events);
  }

  ASSERT_EQ(3U, GetTreeIDToTreeMap().size());

  ui::AutomationAXTreeWrapper* wrapper_0 =
      GetTreeIDToTreeMap()[tree_0_id].get();
  ASSERT_TRUE(wrapper_0);
  ui::AutomationAXTreeWrapper* wrapper_1 =
      GetTreeIDToTreeMap()[tree_1_id].get();
  ASSERT_TRUE(wrapper_1);
  ui::AutomationAXTreeWrapper* wrapper_2 =
      GetTreeIDToTreeMap()[tree_2_id].get();
  ASSERT_TRUE(wrapper_2);

  // The root tree isn't ignored.
  EXPECT_FALSE(wrapper_0->IsTreeIgnored());

  // However, since the hosting node in |wrapper_0| is ignored, both of the
  // descendant trees should be ignored.
  EXPECT_TRUE(wrapper_1->IsTreeIgnored());
  EXPECT_TRUE(wrapper_2->IsTreeIgnored());

  // No longer invisible.
  ui::AXNode* button = wrapper_0->ax_tree()->GetFromId(2);
  ui::AXNodeData data = button->TakeData();
  data.RemoveState(ax::mojom::State::kInvisible);
  button->SetData(data);

  EXPECT_FALSE(wrapper_0->IsTreeIgnored());
  EXPECT_FALSE(wrapper_1->IsTreeIgnored());
  EXPECT_FALSE(wrapper_2->IsTreeIgnored());
}

TEST_F(AutomationTreeManagerOwnerTest, GetMultipleChildRootsAppIdConstruction) {
  // Two trees each with a button and a client node.
  std::vector<std::vector<ui::AXTreeUpdate>> updates_list;
  for (int i = 0; i < 2; i++) {
    std::vector<ui::AXTreeUpdate>& updates = updates_list.emplace_back();
    updates.emplace_back();
    auto& tree_update = updates.back();
    tree_update.has_tree_data = true;
    tree_update.root_id = 1;
    auto& tree_data = tree_update.tree_data;
    tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    tree_update.nodes.emplace_back();
    auto& node_data1 = tree_update.nodes.back();
    node_data1.id = 1;
    node_data1.role =
        i == 0 ? ax::mojom::Role::kDesktop : ax::mojom::Role::kRootWebArea;
    node_data1.child_ids.push_back(2);
    node_data1.child_ids.push_back(3);
    node_data1.relative_bounds.bounds = gfx::RectF(100, 100, 100, 100);
    tree_update.nodes.emplace_back();
    auto& node_data2 = tree_update.nodes.back();
    node_data2.id = 2;
    node_data2.role = ax::mojom::Role::kButton;
    node_data2.relative_bounds.bounds = gfx::RectF(0, 0, 200, 200);
    tree_update.nodes.emplace_back();
    auto& node_data3 = tree_update.nodes.back();
    node_data3.id = 3;
    node_data3.role = ax::mojom::Role::kClient;
    node_data3.relative_bounds.bounds = gfx::RectF(0, 0, 200, 200);
  }

  // Link up the trees by using one app id. Tree 0's client has two children
  // from tree 1.
  ui::AXTreeID tree_0_id = updates_list[0][0].tree_data.tree_id;
  ui::AXTreeID tree_1_id = updates_list[1][0].tree_data.tree_id;
  auto& wrapper0_client_data = updates_list[0][0].nodes[2];
  auto& wrapper1_button_data = updates_list[1][0].nodes[1];
  auto& wrapper1_client_data = updates_list[1][0].nodes[2];

  // This construction requires the hosting and client nodes annotate with the
  // same app id.
  wrapper0_client_data.AddStringAttribute(
      ax::mojom::StringAttribute::kChildTreeNodeAppId, "app1");
  wrapper1_button_data.AddStringAttribute(ax::mojom::StringAttribute::kAppId,
                                          "app1");
  wrapper1_client_data.AddStringAttribute(ax::mojom::StringAttribute::kAppId,
                                          "app1");

  std::vector<ui::AXEvent> empty_events;
  for (auto& updates : updates_list) {
    SendOnAccessibilityEvents(updates[0].tree_data.tree_id, updates,
                              gfx::Point(), empty_events);
  }

  ASSERT_EQ(2U, GetTreeIDToTreeMap().size());

  ui::AutomationAXTreeWrapper* wrapper_0 =
      GetTreeIDToTreeMap()[tree_0_id].get();
  ASSERT_TRUE(wrapper_0);

  ui::AXNode* wrapper0_client = wrapper_0->ax_tree()->GetFromId(3);
  ASSERT_TRUE(wrapper0_client);

  std::vector<ui::AXNode*> child_roots =
      CallGetRootsOfChildTree(wrapper0_client);
  EXPECT_EQ(2U, child_roots.size());
  EXPECT_EQ(tree_1_id, child_roots[0]->tree()->GetAXTreeID());
  EXPECT_EQ(tree_1_id, child_roots[1]->tree()->GetAXTreeID());
  EXPECT_EQ(2, child_roots[0]->id());
  EXPECT_EQ(3, child_roots[1]->id());
}

TEST_F(AutomationTreeManagerOwnerTest, FireEventsWithListeners) {
  // A simple tree.
  std::vector<ui::AXTreeUpdate> updates;
  updates.emplace_back();
  auto& tree_update = updates.back();
  tree_update.has_tree_data = true;
  tree_update.root_id = 1;
  auto& tree_data = tree_update.tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  tree_update.nodes.emplace_back();
  auto& node_data1 = tree_update.nodes.back();
  node_data1.id = 1;
  node_data1.role = ax::mojom::Role::kRootWebArea;
  node_data1.child_ids.push_back(2);
  node_data1.relative_bounds.bounds = gfx::RectF(100, 100, 100, 100);
  tree_update.nodes.emplace_back();
  auto& node_data2 = tree_update.nodes.back();
  node_data2.id = 2;
  node_data2.role = ax::mojom::Role::kButton;
  node_data2.relative_bounds.bounds = gfx::RectF(0, 0, 200, 200);

  // Add a hook for events from automation.
  // Event names are the concatenation of the ax::mojom::Event string and the
  // generated event string.
  std::vector<std::string> events;
  AddEventCallback(base::BindLambdaForTesting(
      [&](const std::string& event) { events.push_back(event); }));

  std::vector<ui::AXEvent> ax_events;
  SendOnAccessibilityEvents(updates[0].tree_data.tree_id, updates, gfx::Point(),
                            ax_events);

  // We aren't listening for any events yet, but we should still get one that
  // gets fired on initial tree creation.
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ("none none", events[0]);
  events.clear();

  // Remove the root node data and don't update tree data.
  tree_update.nodes.erase(tree_update.nodes.begin());
  tree_update.has_tree_data = false;

  // Trigger a role change.
  tree_update.nodes[0].role = ax::mojom::Role::kSwitch;
  SendOnAccessibilityEvents(updates[0].tree_data.tree_id, updates, gfx::Point(),
                            ax_events);

  // There should be no events since there are no listeners and this isn't the
  // initial tree.
  ASSERT_TRUE(events.empty());

  // Add a role change listener and do trigger the role change again.
  auto* wrapper = GetTreeIDToTreeMap()[tree_data.tree_id].get();
  auto* tree = wrapper->ax_tree();
  // The button is id 2.
  std::tuple<ax::mojom::Event, ui::AXEventGenerator::Event> event_type(
      ax::mojom::Event::kNone, ui::AXEventGenerator::Event::ROLE_CHANGED);
  wrapper->EventListenerAdded(event_type, tree->GetFromId(2));
  EXPECT_EQ(1U, wrapper->EventListenerCount());
  EXPECT_TRUE(wrapper->HasEventListener(event_type, tree->GetFromId(2)));
  tree_update.nodes[0].role = ax::mojom::Role::kButton;
  SendOnAccessibilityEvents(updates[0].tree_data.tree_id, updates, gfx::Point(),
                            ax_events);

  // We should now have exactly one event.
  ASSERT_EQ(1U, events.size());
  EXPECT_EQ("none roleChanged", events[0]);
  events.clear();

  // Now, remove the listener and do the same as above.
  wrapper->EventListenerRemoved(event_type, tree->GetFromId(2));
  // We have to add another listener to ensure we don't shut down (no event
  // listeners means this renderer closes).
  wrapper->EventListenerAdded(
      std::tuple<ax::mojom::Event, ui::AXEventGenerator::Event>(
          ax::mojom::Event::kLoadComplete, ui::AXEventGenerator::Event::NONE),
      tree->GetFromId(1));
  tree_update.nodes[0].role = ax::mojom::Role::kSwitch;
  SendOnAccessibilityEvents(updates[0].tree_data.tree_id, updates, gfx::Point(),
                            ax_events);

  // We should have no events.
  ASSERT_TRUE(events.empty());

  // Finally, let's fire a non-generated event on the button, but add the
  // listener on the root. This will test both non-generated events and
  // respecting event listeners on ancestors of the target.

  // First, fire the event without the click listener.
  tree_update.nodes.clear();
  ax_events.emplace_back();
  auto& event = ax_events.back();
  event.event_type = ax::mojom::Event::kClicked;
  event.id = 2;
  SendOnAccessibilityEvents(updates[0].tree_data.tree_id, updates, gfx::Point(),
                            ax_events);

  // No event.
  ASSERT_TRUE(events.empty());

  // Now, add the click listener to the root, and fire the click event on the
  // button.
  wrapper->EventListenerAdded(
      std::tuple<ax::mojom::Event, ui::AXEventGenerator::Event>(
          ax::mojom::Event::kClicked, ui::AXEventGenerator::Event::NONE),
      tree->GetFromId(1));
  SendOnAccessibilityEvents(updates[0].tree_data.tree_id, updates, gfx::Point(),
                            ax_events);

  ASSERT_EQ(1U, events.size());
  EXPECT_EQ("clicked none", events[0]);
}

}  // namespace ui
