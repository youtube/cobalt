// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/automation_internal_bindings.h"

#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "gin/public/context_holder.h"
#include "gin/public/isolate_holder.h"
#include "services/accessibility/fake_service_client.h"
#include "services/accessibility/features/bindings_isolate_holder.h"
#include "services/accessibility/features/v8_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-template.h"

namespace ax {

namespace {

// Single-threaded IsolateHolder for use in bindings tests.
// TODO(crbug.com/1357889): This is somewhat similar to
// chrome/test/base/v8_unit_test.h which is used by js2gtest, which would be a
// good test framework to use for all the bindings tests eventually, depending
// on Chrome dependencies.
class TestIsolateHolder : public BindingsIsolateHolder {
 public:
  TestIsolateHolder() = default;
  TestIsolateHolder(const TestIsolateHolder&) = delete;
  TestIsolateHolder& operator=(const TestIsolateHolder&) = delete;
  virtual ~TestIsolateHolder() = default;

  // BindingsIsolateHolder:
  v8::Isolate* GetIsolate() const override {
    return isolate_holder_->isolate();
  }
  v8::Local<v8::Context> GetContext() const override {
    return context_holder_->context();
  }
  void HandleError(const std::string& message) override {
    LOG(ERROR) << message;
    error_count_++;
  }

  void StartTestV8AndBindAutomation(
      AutomationInternalBindings* automation_bindings) {
    BindingsIsolateHolder::InitializeV8();

    // Test isolate uses the test main thread and will block.
    isolate_holder_ = std::make_unique<gin::IsolateHolder>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        gin::IsolateHolder::kSingleThread,
        gin::IsolateHolder::IsolateType::kUtility);

    v8::Isolate::Scope isolate_scope(isolate_holder_->isolate());
    v8::HandleScope handle_scope(isolate_holder_->isolate());
    v8::Local<v8::ObjectTemplate> global_template =
        v8::ObjectTemplate::New(isolate_holder_->isolate());

    v8::Local<v8::ObjectTemplate> automation_template =
        v8::ObjectTemplate::New(isolate_holder_->isolate());
    automation_bindings->AddAutomationRoutesToTemplate(&automation_template);
    global_template->Set(isolate_holder_->isolate(), "automation",
                         automation_template);

    v8::Local<v8::ObjectTemplate> automation_internal_template =
        v8::ObjectTemplate::New(isolate_holder_->isolate());
    automation_bindings->AddAutomationInternalRoutesToTemplate(
        &automation_internal_template);
    global_template->Set(isolate_holder_->isolate(), "automationInternal",
                         automation_internal_template);

    v8::Local<v8::Context> context =
        v8::Context::New(isolate_holder_->isolate(),
                         /*extensions=*/nullptr, global_template);
    context_holder_ =
        std::make_unique<gin::ContextHolder>(isolate_holder_->isolate());
    context_holder_->SetContext(context);
  }

  base::WeakPtr<TestIsolateHolder> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  int ErrorCount() const { return error_count_; }

 private:
  std::unique_ptr<gin::IsolateHolder> isolate_holder_;
  std::unique_ptr<gin::ContextHolder> context_holder_;
  int error_count_ = 0;

  base::WeakPtrFactory<TestIsolateHolder> weak_ptr_factory_{this};
};

}  // namespace

class AutomationInternalBindingsTest : public testing::Test {
 public:
  AutomationInternalBindingsTest() = default;
  AutomationInternalBindingsTest(const AutomationInternalBindingsTest&) =
      delete;
  AutomationInternalBindingsTest& operator=(
      const AutomationInternalBindingsTest&) = delete;
  ~AutomationInternalBindingsTest() override = default;

  void SetUp() override {
    service_client_ = std::make_unique<FakeServiceClient>(nullptr);
    test_isolate_holder_ = std::make_unique<TestIsolateHolder>();
    automation_bindings_ = std::make_unique<AutomationInternalBindings>(
        test_isolate_holder_->GetWeakPtr(), service_client_->GetWeakPtr(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    test_isolate_holder_->StartTestV8AndBindAutomation(
        automation_bindings_.get());
  }

  void DispatchAccessibilityEvents(const ui::AXTreeID& tree_id,
                                   const std::vector<ui::AXTreeUpdate>& updates,
                                   const gfx::Point& mouse_location,
                                   const std::vector<ui::AXEvent>& events) {
    automation_bindings_->DispatchAccessibilityEvents(tree_id, updates,
                                                      mouse_location, events);
  }

  void DispatchLocationChange(const ui::AXTreeID& tree_id,
                              int node_id,
                              const ui::AXRelativeBounds& bounds) {
    automation_bindings_->DispatchAccessibilityLocationChange(tree_id, node_id,
                                                              bounds);
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestIsolateHolder> test_isolate_holder_;
  std::unique_ptr<AutomationInternalBindings> automation_bindings_;
  std::unique_ptr<FakeServiceClient> service_client_;
};

// A test to ensure that the testing framework can catch exceptions.
TEST_F(AutomationInternalBindingsTest, CatchesExceptions) {
  std::string script = R"JS(
    throw 'catch me if you can';
  )JS";
  EXPECT_FALSE(test_isolate_holder_->ExecuteScriptInContext(script));
  EXPECT_EQ(1, test_isolate_holder_->ErrorCount());
}

// Spot checks that bindings were added to the correct namespace.
TEST_F(AutomationInternalBindingsTest, SpotCheckBindingsAdded) {
  // This should succeed because automation and automationInternal
  // bindings were added.
  std::string script =
      base::StringPrintf(R"JS(
    automation.GetFocus();
    automation.SetDesktopID('%s');
    automation.StartCachingAccessibilityTrees();
    automationInternal.enableDesktop();
  )JS",
                         ui::AXTreeID::CreateNewAXTreeID().ToString().c_str());
  EXPECT_TRUE(test_isolate_holder_->ExecuteScriptInContext(script));
  EXPECT_EQ(0, test_isolate_holder_->ErrorCount());

  // Lowercase getFocus does not exist.
  script = R"JS(
    automation.getFocus();
  )JS";
  EXPECT_FALSE(test_isolate_holder_->ExecuteScriptInContext(script));
  EXPECT_EQ(1, test_isolate_holder_->ErrorCount());
}

TEST_F(AutomationInternalBindingsTest, GetsFocusAndNodeData) {
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
  DispatchAccessibilityEvents(tree_data.tree_id, updates, gfx::Point(), events);

  // TODO(crbug.com/1357889): Use JS test framework assert/expect instead of
  // throwing exceptions in these tests.
  std::string script = base::StringPrintf(R"JS(
    const treeId = '%s';
    automation.SetDesktopID(treeId);
    const focusedNodeInfo = automation.GetFocus();
    if (!focusedNodeInfo) {
      throw 'Expected to find a focused object';
    }
    if (focusedNodeInfo.nodeId !== 2) {
      throw 'Expected focused node ID 2, got ' + focusedNodeInfo.nodeId;
    }
    const role = automation.GetRole(treeId, focusedNodeInfo.nodeId);
    if (role !== 'button') {
      throw 'Expected role button, got ' + role;
    }
  )JS",
                                          tree_data.tree_id.ToString().c_str());
  EXPECT_TRUE(test_isolate_holder_->ExecuteScriptInContext(script));
  EXPECT_EQ(0, test_isolate_holder_->ErrorCount());
}

TEST_F(AutomationInternalBindingsTest, GetsLocation) {
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
  node_data1.relative_bounds.bounds = gfx::RectF(100, 100, 200, 200);
  node_data1.child_ids.push_back(2);
  tree_update.nodes.emplace_back();
  auto& node_data2 = tree_update.nodes.back();
  node_data2.id = 2;
  node_data2.role = ax::mojom::Role::kImage;
  node_data2.relative_bounds.bounds = gfx::RectF(10, 10, 50, 50);
  std::vector<ui::AXEvent> events;
  DispatchAccessibilityEvents(tree_data.tree_id, updates, gfx::Point(), events);

  // TODO(crbug.com/1357889): Use JS test framework assert/expect instead of
  // throwing exceptions in these tests.
  std::string script = base::StringPrintf(R"JS(
    const treeId = '%s';
    automation.SetDesktopID(treeId);
    let bounds = automation.GetLocation(treeId, 2);
    if (!bounds) {
      throw 'Could not get bounds for node';
    }
    if (bounds.left !== 110 || bounds.top !== 110 ||
        bounds.width !== 50 || bounds.height !== 50) {
      throw 'Bounds should be (110, 110, 50, 50), got (' +
        bounds.left + ', ' + bounds.top + ', ' + bounds.width +
        ', ' + bounds.height + ')';
    }
  )JS",
                                          tree_data.tree_id.ToString().c_str());
  EXPECT_TRUE(test_isolate_holder_->ExecuteScriptInContext(script));
  EXPECT_EQ(0, test_isolate_holder_->ErrorCount());

  ui::AXRelativeBounds new_bounds;
  new_bounds.bounds = gfx::RectF(32, 42, 200, 200);
  DispatchLocationChange(tree_data.tree_id, 1, new_bounds);

  script = R"JS(
    bounds = automation.GetLocation(treeId, 2);
    if (!bounds) {
      throw 'Could not get bounds for node';
    }
    if (bounds.left !== 42 || bounds.top !== 52 ||
        bounds.width !== 50 || bounds.height !== 50) {
      throw 'Bounds should be (42, 52, 50, 50), got (' +
        bounds.left + ', ' + bounds.top + ', ' + bounds.width +
        ', ' + bounds.height + ')';
    }
  )JS";
  EXPECT_TRUE(test_isolate_holder_->ExecuteScriptInContext(script));
  EXPECT_EQ(0, test_isolate_holder_->ErrorCount());
}

}  // namespace ax
