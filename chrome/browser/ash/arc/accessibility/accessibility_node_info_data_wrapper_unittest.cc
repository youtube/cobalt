// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/accessibility_node_info_data_wrapper.h"

#include <map>
#include <memory>
#include <utility>

#include "ash/components/arc/mojom/accessibility_helper.mojom.h"
#include "chrome/browser/ash/arc/accessibility/accessibility_window_info_data_wrapper.h"
#include "chrome/browser/ash/arc/accessibility/arc_accessibility_test_util.h"
#include "chrome/browser/ash/arc/accessibility/arc_accessibility_util.h"
#include "chrome/browser/ash/arc/accessibility/ax_tree_source_arc.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/platform/ax_android_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace arc {

using AXActionType = mojom::AccessibilityActionType;
using AXBooleanProperty = mojom::AccessibilityBooleanProperty;
using AXCollectionInfoData = mojom::AccessibilityCollectionInfoData;
using AXCollectionItemInfoData = mojom::AccessibilityCollectionItemInfoData;
using AXIntListProperty = mojom::AccessibilityIntListProperty;
using AXIntProperty = mojom::AccessibilityIntProperty;
using AXNodeInfoData = mojom::AccessibilityNodeInfoData;
using AXRangeInfoData = mojom::AccessibilityRangeInfoData;
using AXStringProperty = mojom::AccessibilityStringProperty;

class AccessibilityNodeInfoDataWrapperTest : public testing::Test,
                                             public AXTreeSourceArc::Delegate {
 public:
  class TestAXTreeSourceArc : public AXTreeSourceArc {
   public:
    explicit TestAXTreeSourceArc(AXTreeSourceArc::Delegate* delegate)
        : AXTreeSourceArc(delegate, /*window=*/nullptr) {}

    // AXTreeSourceArc overrides.
    bool IsRootOfNodeTree(int32_t id) const override {
      return id == node_root_id_;
    }

    AccessibilityInfoDataWrapper* GetFromId(int32_t id) const override {
      auto itr = wrapper_map_.find(id);
      if (itr == wrapper_map_.end())
        return nullptr;
      return itr->second;
    }

    AccessibilityInfoDataWrapper* GetParent(
        AccessibilityInfoDataWrapper* info_data) const override {
      auto itr = parent_map_.find(info_data->GetId());
      if (itr == parent_map_.end())
        return nullptr;
      return GetFromId(itr->second);
    }

    void set_node_root_id(int32_t id) { node_root_id_ = id; }

    void SetIdToWrapper(AccessibilityInfoDataWrapper* wrapper) {
      wrapper_map_[wrapper->GetId()] = wrapper;
    }
    void SetParentId(int32_t child_id, int32_t parent_id) {
      parent_map_[child_id] = parent_id;
    }

   private:
    int32_t node_root_id_ = -1;
    std::map<int32_t, AccessibilityInfoDataWrapper*> wrapper_map_;
    std::map<int32_t, int32_t> parent_map_;
  };

  AccessibilityNodeInfoDataWrapperTest()
      : tree_source_(new TestAXTreeSourceArc(this)) {}

  ui::AXNodeData CallSerialize(
      const AccessibilityInfoDataWrapper& wrapper) const {
    ui::AXNodeData data;
    wrapper.Serialize(&data);
    return data;
  }

  void SetNodeRootId(int32_t id) { tree_source_->set_node_root_id(id); }
  void SetIdToWrapper(AccessibilityInfoDataWrapper* wrapper) {
    tree_source_->SetIdToWrapper(wrapper);
  }
  void SetParentId(int32_t child_id, int32_t parent_id) {
    tree_source_->SetParentId(child_id, parent_id);
  }

  // AXTreeSourceArc::Delegate overrides.
  bool UseFullFocusMode() const override { return full_focus_mode_; }
  void OnAction(const ui::AXActionData& data) const override {}

  void set_full_focus_mode(bool enabled) { full_focus_mode_ = enabled; }

  AXTreeSourceArc* tree_source() { return tree_source_.get(); }

 private:
  const std::unique_ptr<TestAXTreeSourceArc> tree_source_;
  bool full_focus_mode_ = true;
};

TEST_F(AccessibilityNodeInfoDataWrapperTest, Name) {
  AXNodeInfoData node;
  SetProperty(node, AXStringProperty::CLASS_NAME, "");

  AccessibilityNodeInfoDataWrapper wrapper(tree_source(), &node);

  // No attributes.
  ui::AXNodeData data = CallSerialize(wrapper);
  std::string name;
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));

  // Text (empty).
  SetProperty(node, AXStringProperty::TEXT, "");

  data = CallSerialize(wrapper);
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));

  // Text (non-empty).
  SetProperty(node, AXStringProperty::TEXT, "label text");

  data = CallSerialize(wrapper);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("label text", name);

  // Content description (empty), text (non-empty).
  SetProperty(node, AXStringProperty::CONTENT_DESCRIPTION, "");

  data = CallSerialize(wrapper);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("label text", name);

  // Content description (non-empty), text (empty).
  SetProperty(node, AXStringProperty::TEXT, "");
  SetProperty(node, AXStringProperty::CONTENT_DESCRIPTION,
              "label content description");

  data = CallSerialize(wrapper);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("label content description", name);

  // Content description (non-empty), text (non-empty).
  SetProperty(node, AXStringProperty::TEXT, "label text");

  data = CallSerialize(wrapper);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("label content description label text", name);
}

TEST_F(AccessibilityNodeInfoDataWrapperTest, NameFromDescendants) {
  AXNodeInfoData root;
  root.id = 10;
  AccessibilityNodeInfoDataWrapper root_wrapper(tree_source(), &root);
  SetIdToWrapper(&root_wrapper);
  SetProperty(root, AXStringProperty::CLASS_NAME, "");
  SetProperty(root, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({1, 2}));

  AXNodeInfoData child1;
  child1.id = 1;
  AccessibilityNodeInfoDataWrapper child1_wrapper(tree_source(), &child1);
  SetIdToWrapper(&child1_wrapper);
  SetProperty(child1, AXBooleanProperty::IMPORTANCE, true);

  AXNodeInfoData child2;
  child2.id = 2;
  AccessibilityNodeInfoDataWrapper child2_wrapper(tree_source(), &child2);
  SetIdToWrapper(&child2_wrapper);
  SetProperty(child2, AXBooleanProperty::IMPORTANCE, true);

  SetParentId(child1.id, root.id);
  SetParentId(child2.id, root.id);

  // Root node has no name, but has descendants with name.
  // Name from contents can happen if a node is focusable.
  SetProperty(root, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(child1, AXStringProperty::TEXT, "child1 label text");
  SetProperty(child2, AXStringProperty::TEXT, "child2 label text");

  // If the screen reader mode is off, do not compute from descendants.
  set_full_focus_mode(false);

  ui::AXNodeData data = CallSerialize(root_wrapper);
  std::string name;
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));

  data = CallSerialize(child1_wrapper);
  ASSERT_FALSE(data.IsIgnored());
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("child1 label text", name);

  data = CallSerialize(child2_wrapper);
  ASSERT_FALSE(data.IsIgnored());
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("child2 label text", name);

  // Enable screen reader.
  // Compute the name of the clickable node from descendants, and ignore them.
  set_full_focus_mode(true);

  data = CallSerialize(root_wrapper);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("child1 label text child2 label text", name);

  data = CallSerialize(child1_wrapper);
  ASSERT_TRUE(data.IsIgnored());
  data = CallSerialize(child2_wrapper);
  ASSERT_TRUE(data.IsIgnored());

  // Don't compute name from descendants for scrollable, e.g. ScrollView.
  SetProperty(root, AXBooleanProperty::SCROLLABLE, true);

  data = CallSerialize(root_wrapper);
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));

  SetProperty(root, AXBooleanProperty::SCROLLABLE, false);

  // Don't compute name from descendants for virtual views, e.g. WebView.
  root.is_virtual_node = true;
  child1.is_virtual_node = true;
  child2.is_virtual_node = true;

  data = CallSerialize(root_wrapper);
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));

  root.is_virtual_node = false;
  child1.is_virtual_node = false;
  child2.is_virtual_node = false;

  // If one child is clickable, do not use clickable child.
  SetProperty(child1, AXBooleanProperty::CLICKABLE, true);

  data = CallSerialize(root_wrapper);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("child2 label text", name);

  data = CallSerialize(child1_wrapper);
  ASSERT_FALSE(data.IsIgnored());
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("child1 label text", name);

  data = CallSerialize(child2_wrapper);
  ASSERT_TRUE(data.IsIgnored());

  // If both children are also clickable, do not use child properties.
  SetProperty(child2, AXBooleanProperty::CLICKABLE, true);

  data = CallSerialize(root_wrapper);
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));

  // If the node has a name, it should override the contents.
  child1.boolean_properties->clear();
  child2.boolean_properties->clear();
  SetProperty(root, AXStringProperty::TEXT, "root label text");

  data = CallSerialize(root_wrapper);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("root label text", name);

  // Clearing both clickable and name from root, the name should not be
  // populated.
  root.boolean_properties->clear();
  root.string_properties->clear();
  data = CallSerialize(root_wrapper);
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
}

TEST_F(AccessibilityNodeInfoDataWrapperTest, NameFromTextProperties) {
  AXNodeInfoData root;
  root.id = 10;
  AccessibilityNodeInfoDataWrapper root_wrapper(tree_source(), &root);
  SetIdToWrapper(&root_wrapper);
  SetProperty(root, AXStringProperty::CLASS_NAME, "");
  SetProperty(root, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(root, AXBooleanProperty::CLICKABLE, true);
  SetProperty(root, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({1}));

  AXNodeInfoData child1;
  child1.id = 1;
  AccessibilityNodeInfoDataWrapper child1_wrapper(tree_source(), &child1);
  SetIdToWrapper(&child1_wrapper);

  // Set all properties that will be used in name computation.
  SetProperty(child1, AXStringProperty::TEXT, "text");
  SetProperty(child1, AXStringProperty::CONTENT_DESCRIPTION,
              "content_description");
  SetProperty(child1, AXStringProperty::STATE_DESCRIPTION, "state_description");
  SetProperty(child1, AXBooleanProperty::IMPORTANCE, true);

  AccessibilityNodeInfoDataWrapper wrapper(tree_source(), &root);

  ui::AXNodeData data = CallSerialize(wrapper);

  std::string name;

  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("text", name);

  // Unset TEXT property, and confirm that CONTENT_DESCRIPTION is used as name.
  SetProperty(child1, AXStringProperty::TEXT, "");
  data = CallSerialize(wrapper);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("content_description", name);

  // Unset CONTENT_DESCRIPTION property, and confirm that STATE_DESCRIPTION is
  // used as name.
  SetProperty(child1, AXStringProperty::CONTENT_DESCRIPTION, "");
  data = CallSerialize(wrapper);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("state_description", name);

  // Don't use any text if the node doesn't have importance.
  SetProperty(child1, AXBooleanProperty::IMPORTANCE, false);
  SetProperty(child1, AXStringProperty::TEXT, "text");
  SetProperty(child1, AXStringProperty::CONTENT_DESCRIPTION,
              "content_description");
  data = CallSerialize(wrapper);
  ASSERT_FALSE(data.HasStringAttribute(ax::mojom::StringAttribute::kName));
}

TEST_F(AccessibilityNodeInfoDataWrapperTest, TextFieldNameAndValue) {
  AXNodeInfoData node;
  SetProperty(node, AXStringProperty::CLASS_NAME, "");
  SetProperty(node, AXBooleanProperty::EDITABLE, true);

  struct AndroidState {
    std::string content_description, text, hint_text;
    bool showingHint = false;
  };
  struct ChromeState {
    std::string name, value;
  };

  std::vector<std::pair<AndroidState, ChromeState>> test_cases = {
      {
          {"email", "editing_text", "", false},
          {"email", "editing_text"},
      },
      {
          {"email", "", "", false},
          {"email", ""},
      },
      {
          {"", "editing_text", "", false},
          {"", "editing_text"},
      },
      {
          // User input and hint text.
          {"", "editing_text", "hint@example.com", false},
          {"hint@example.com", "editing_text"},
      },
      {
          // No user input. Hint text is non-empty.
          {"", "hint@example.com", "hint@example.com", true},
          {"hint@example.com", ""},
      },
      {
          // User input is the same as hint text.
          {"", "example@example.com", "example@example.com", false},
          {"example@example.com", "example@example.com"},
      },
      {
          // No user input. Content description and hint tex are non-empty.
          {"email", "hint@example.com", "hint@example.com", true},
          {"email hint@example.com", ""},
      },
      {
          {"email", "editing_text", "hint@example.com", false},
          {"email hint@example.com", "editing_text"},
      },
      {
          {"", "", "", false},
          {"", ""},
      },
  };

  for (const auto& test_case : test_cases) {
    SetProperty(node, AXStringProperty::CONTENT_DESCRIPTION,
                test_case.first.content_description);
    SetProperty(node, AXStringProperty::TEXT, test_case.first.text);
    SetProperty(node, AXStringProperty::HINT_TEXT, test_case.first.hint_text);
    SetProperty(node, AXBooleanProperty::SHOWING_HINT_TEXT,
                test_case.first.showingHint);

    AccessibilityNodeInfoDataWrapper wrapper(tree_source(), &node);
    ui::AXNodeData data = CallSerialize(wrapper);

    std::string prop;
    ASSERT_EQ(
        !test_case.second.name.empty(),
        data.GetStringAttribute(ax::mojom::StringAttribute::kName, &prop));
    if (!test_case.second.name.empty())
      EXPECT_EQ(test_case.second.name, prop);

    ASSERT_EQ(
        !test_case.second.value.empty(),
        data.GetStringAttribute(ax::mojom::StringAttribute::kValue, &prop));
    if (!test_case.second.value.empty())
      EXPECT_EQ(test_case.second.value, prop);
  }
}

TEST_F(AccessibilityNodeInfoDataWrapperTest, StringProperties) {
  AXNodeInfoData node;
  node.id = 10;
  SetProperty(node, AXStringProperty::CLASS_NAME, "");
  SetProperty(node, AXStringProperty::PACKAGE_NAME, "com.android.vending");
  SetProperty(node, AXStringProperty::TOOLTIP, "tooltip text");

  SetNodeRootId(node.id);

  AccessibilityNodeInfoDataWrapper wrapper(tree_source(), &node);

  ui::AXNodeData data = CallSerialize(wrapper);

  std::string prop;
  // Url includes AXTreeId, which is unguessable. Just verifies the prefix.
  ASSERT_TRUE(data.GetStringAttribute(ax::mojom::StringAttribute::kUrl, &prop));
  EXPECT_EQ(0U, prop.find("com.android.vending/"));

  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kTooltip, &prop));
  ASSERT_EQ("tooltip text", prop);
}

TEST_F(AccessibilityNodeInfoDataWrapperTest, States) {
  AXNodeInfoData node;
  AccessibilityNodeInfoDataWrapper wrapper(tree_source(), &node);

  // Node is checkable, but not checked.
  SetProperty(node, AXBooleanProperty::CHECKABLE, true);
  SetProperty(node, AXBooleanProperty::CHECKED, false);

  ui::AXNodeData data = CallSerialize(wrapper);
  EXPECT_EQ(ax::mojom::CheckedState::kFalse, data.GetCheckedState());

  // Make the node checked.
  SetProperty(node, AXBooleanProperty::CHECKED, true);

  data = CallSerialize(wrapper);
  EXPECT_EQ(ax::mojom::CheckedState::kTrue, data.GetCheckedState());

  // Make the node expandable (i.e. collapsed).
  AddStandardAction(&node, AXActionType::EXPAND);

  data = CallSerialize(wrapper);
  EXPECT_TRUE(data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_FALSE(data.HasState(ax::mojom::State::kExpanded));

  // Make the node collapsible (i.e. expanded).
  node.standard_actions = absl::nullopt;
  AddStandardAction(&node, AXActionType::COLLAPSE);

  data = CallSerialize(wrapper);
  EXPECT_FALSE(data.HasState(ax::mojom::State::kCollapsed));
  EXPECT_TRUE(data.HasState(ax::mojom::State::kExpanded));
}

TEST_F(AccessibilityNodeInfoDataWrapperTest, SelectedState) {
  AXNodeInfoData grid;
  grid.id = 1;
  SetProperty(grid, AXIntListProperty::CHILD_NODE_IDS,
              std::vector<int>({10, 11, 12, 13}));
  grid.collection_info = AXCollectionInfoData::New();
  grid.collection_info->row_count = 2;
  grid.collection_info->column_count = 2;

  AccessibilityNodeInfoDataWrapper grid_wrapper(tree_source(), &grid);
  SetIdToWrapper(&grid_wrapper);

  // Make child of grid have cell role, which supports selected state.
  std::vector<AXNodeInfoData> cells(4);
  for (int i = 0; i < 4; i++) {
    AXNodeInfoData& node = cells[i];
    node.id = 10 + i;
    node.collection_item_info = AXCollectionItemInfoData::New();
    node.collection_item_info->row_index = i % 2;
    node.collection_item_info->column_index = i / 2;
    SetProperty(node, AXBooleanProperty::SELECTED, true);

    SetParentId(node.id, grid.id);
  }

  AccessibilityNodeInfoDataWrapper cell_wrapper(tree_source(), &cells[0]);

  ui::AXNodeData data = CallSerialize(cell_wrapper);
  ASSERT_EQ(ax::mojom::Role::kCell, data.role);
  ASSERT_TRUE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  ASSERT_FALSE(
      data.HasStringAttribute(ax::mojom::StringAttribute::kDescription));

  // text_node is simple static text, which doesn't supports selected state in
  // the web.
  AXNodeInfoData text_node;
  SetProperty(text_node, AXStringProperty::TEXT, "text.");
  SetProperty(text_node, AXBooleanProperty::SELECTED, true);

  AccessibilityNodeInfoDataWrapper text_wrapper(tree_source(), &text_node);

  std::string description;
  data = CallSerialize(text_wrapper);
  ASSERT_EQ(ax::mojom::Role::kStaticText, data.role);
  ASSERT_FALSE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  ASSERT_TRUE(data.GetStringAttribute(ax::mojom::StringAttribute::kDescription,
                                      &description));
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_ARC_ACCESSIBILITY_SELECTED_STATUS),
            description);
}

TEST_F(AccessibilityNodeInfoDataWrapperTest, EditTextRole) {
  AXNodeInfoData node;
  node.id = 1;
  AccessibilityNodeInfoDataWrapper wrapper(tree_source(), &node);

  // Editable node is textField.
  SetProperty(node, AXStringProperty::CLASS_NAME, ui::kAXEditTextClassname);
  SetProperty(node, AXBooleanProperty::EDITABLE, true);

  ui::AXNodeData data = CallSerialize(wrapper);
  EXPECT_EQ(ax::mojom::Role::kTextField, data.role);

  // Non-editable node is not textField even if it has EditTextClassname.
  // When it has text and no children, it is staticText. Otherwise, it's
  // genericContainer.
  SetProperty(node, AXBooleanProperty::EDITABLE, false);
  SetProperty(node, AXStringProperty::TEXT, "text");

  data = CallSerialize(wrapper);
  EXPECT_EQ(ax::mojom::Role::kStaticText, data.role);

  // Add a child.
  SetProperty(node, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({2}));
  AXNodeInfoData child;
  child.id = 2;
  AccessibilityNodeInfoDataWrapper child_wrapper(tree_source(), &child);
  SetIdToWrapper(&child_wrapper);
  SetParentId(child.id, node.id);

  data = CallSerialize(wrapper);
  EXPECT_EQ(ax::mojom::Role::kGenericContainer, data.role);
}

TEST_F(AccessibilityNodeInfoDataWrapperTest, StateDescription) {
  AXNodeInfoData node;
  AccessibilityNodeInfoDataWrapper wrapper(tree_source(), &node);
  node.id = 10;

  // No attributes.
  ui::AXNodeData data = CallSerialize(wrapper);
  std::string description;
  ASSERT_FALSE(data.GetStringAttribute(ax::mojom::StringAttribute::kDescription,
                                       &description));
  std::string value;
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kValue, &value));

  std::string checked_state_description;
  ASSERT_FALSE(data.GetStringAttribute(
      ax::mojom::StringAttribute::kCheckedStateDescription,
      &checked_state_description));

  // State Description without Range Value should be stored as kDescription
  SetProperty(node, AXStringProperty::STATE_DESCRIPTION, "state description");

  data = CallSerialize(wrapper);
  ASSERT_TRUE(data.GetStringAttribute(ax::mojom::StringAttribute::kDescription,
                                      &description));
  EXPECT_EQ("state description", description);
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kValue, &value));
  ASSERT_FALSE(data.GetStringAttribute(
      ax::mojom::StringAttribute::kCheckedStateDescription,
      &checked_state_description));

  // State Description with Range Value should be stored as kValue
  node.range_info = AXRangeInfoData::New();

  data = CallSerialize(wrapper);
  ASSERT_FALSE(data.GetStringAttribute(ax::mojom::StringAttribute::kDescription,
                                       &description));
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kValue, &value));
  EXPECT_EQ("state description", value);
  ASSERT_FALSE(data.GetStringAttribute(
      ax::mojom::StringAttribute::kCheckedStateDescription,
      &checked_state_description));

  // State Description for compound button should be stores as
  // checkedDescription.
  node.range_info.reset();
  SetProperty(node, AXBooleanProperty::CHECKABLE, true);

  data = CallSerialize(wrapper);
  ASSERT_FALSE(data.GetStringAttribute(ax::mojom::StringAttribute::kDescription,
                                       &description));
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kValue, &value));
  ASSERT_TRUE(data.GetStringAttribute(
      ax::mojom::StringAttribute::kCheckedStateDescription,
      &checked_state_description));
  EXPECT_EQ("state description", checked_state_description);
}

TEST_F(AccessibilityNodeInfoDataWrapperTest, LabeledByLoop) {
  AXNodeInfoData root;
  root.id = 1;
  SetProperty(root, AXIntProperty::LABELED_BY, 2);
  AccessibilityNodeInfoDataWrapper wrapper(tree_source(), &root);
  SetIdToWrapper(&wrapper);

  AXNodeInfoData node2;
  node2.id = 2;
  AccessibilityNodeInfoDataWrapper child1_wrapper(tree_source(), &node2);
  SetIdToWrapper(&child1_wrapper);
  SetProperty(node2, AXStringProperty::CONTENT_DESCRIPTION, "node2");
  SetProperty(node2, AXIntProperty::LABELED_BY, 1);

  ui::AXNodeData data = CallSerialize(wrapper);
  std::string name;
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("node2", name);

  data = CallSerialize(child1_wrapper);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  EXPECT_EQ("node2", name);
}

TEST_F(AccessibilityNodeInfoDataWrapperTest, AppendkDescription) {
  AXNodeInfoData node;
  AccessibilityNodeInfoDataWrapper wrapper(tree_source(), &node);
  node.id = 10;

  // No attributes.
  ui::AXNodeData data = CallSerialize(wrapper);
  std::string description;
  ASSERT_FALSE(data.GetStringAttribute(ax::mojom::StringAttribute::kDescription,
                                       &description));

  SetProperty(node, AXStringProperty::STATE_DESCRIPTION, "state description");
  SetProperty(node, AXBooleanProperty::SELECTED, true);
  SetProperty(node, AXStringProperty::TEXT, "text");

  data = CallSerialize(wrapper);
  ASSERT_TRUE(data.GetStringAttribute(ax::mojom::StringAttribute::kDescription,
                                      &description));
  EXPECT_EQ("state description " +
                l10n_util::GetStringUTF8(IDS_ARC_ACCESSIBILITY_SELECTED_STATUS),
            description);
}

TEST_F(AccessibilityNodeInfoDataWrapperTest, ControlIsFocusable) {
  AXNodeInfoData root;
  root.id = 1;
  SetProperty(root, AXStringProperty::CLASS_NAME, ui::kAXSeekBarClassname);
  SetProperty(root, AXStringProperty::TEXT, "");
  SetProperty(root, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(root, AXBooleanProperty::IMPORTANCE, true);
  AccessibilityNodeInfoDataWrapper wrapper(tree_source(), &root);

  // Check the pre conditions required, before checking whether this
  // control is focusable.
  ui::AXNodeData data = CallSerialize(wrapper);
  std::string name;
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ(ax::mojom::Role::kSlider, data.role);

  ASSERT_TRUE(wrapper.IsFocusableInFullFocusMode());
}

TEST_F(AccessibilityNodeInfoDataWrapperTest, FocusAndClickAction) {
  AXNodeInfoData root;
  root.id = 10;
  AccessibilityNodeInfoDataWrapper root_wrapper(tree_source(), &root);
  SetIdToWrapper(&root_wrapper);
  SetProperty(root, AXStringProperty::CLASS_NAME, "");
  SetProperty(root, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(root, AXBooleanProperty::FOCUSABLE, true);
  SetProperty(root, AXBooleanProperty::CLICKABLE, true);
  SetProperty(root, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({1}));

  AXNodeInfoData child1;
  child1.id = 1;
  AccessibilityNodeInfoDataWrapper child1_wrapper(tree_source(), &child1);
  SetIdToWrapper(&child1_wrapper);
  SetProperty(child1, AXBooleanProperty::IMPORTANCE, true);
  SetProperty(child1, AXIntListProperty::CHILD_NODE_IDS, std::vector<int>({2}));
  SetParentId(child1.id, root.id);

  AXNodeInfoData child2;
  child2.id = 2;
  AccessibilityNodeInfoDataWrapper child2_wrapper(tree_source(), &child2);
  SetIdToWrapper(&child2_wrapper);
  SetProperty(child2, AXBooleanProperty::IMPORTANCE, true);
  SetParentId(child2.id, child1.id);

  SetProperty(child2, AXStringProperty::CONTENT_DESCRIPTION, "test text");

  set_full_focus_mode(true);

  ui::AXNodeData data = CallSerialize(root_wrapper);
  std::string name;
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("test text", name);
  ASSERT_TRUE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kClickable));
  EXPECT_TRUE(data.HasState(ax::mojom::State::kFocusable));

  data = CallSerialize(child1_wrapper);
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_FALSE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kClickable));
  EXPECT_FALSE(data.HasState(ax::mojom::State::kFocusable));

  // Set click and focus action to child1. child1 will be clickable and
  // focusable, and gets ax name from descendants.
  AddStandardAction(&child1, AXActionType::CLICK);
  AddStandardAction(&child1, AXActionType::FOCUS);

  data = CallSerialize(root_wrapper);
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_TRUE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kClickable));
  EXPECT_TRUE(data.HasState(ax::mojom::State::kFocusable));

  data = CallSerialize(child1_wrapper);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("test text", name);
  ASSERT_TRUE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kClickable));
  EXPECT_TRUE(data.HasState(ax::mojom::State::kFocusable));

  // Same for clear_focus action instead of focus action.
  child1.standard_actions = absl::nullopt;
  AddStandardAction(&child1, AXActionType::CLICK);
  AddStandardAction(&child1, AXActionType::CLEAR_FOCUS);

  data = CallSerialize(root_wrapper);
  ASSERT_FALSE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_TRUE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kClickable));
  EXPECT_TRUE(data.HasState(ax::mojom::State::kFocusable));

  data = CallSerialize(child1_wrapper);
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kName, &name));
  ASSERT_EQ("test text", name);
  ASSERT_TRUE(data.GetBoolAttribute(ax::mojom::BoolAttribute::kClickable));
  EXPECT_TRUE(data.HasState(ax::mojom::State::kFocusable));
}

TEST_F(AccessibilityNodeInfoDataWrapperTest, LiveRegionStatus) {
  AXNodeInfoData root;
  root.id = 1;
  mojom::AccessibilityLiveRegionType politeLiveRegion =
      mojom::AccessibilityLiveRegionType::POLITE;
  SetProperty(root, AXIntProperty::LIVE_REGION,
              static_cast<int32_t>(politeLiveRegion));
  AccessibilityNodeInfoDataWrapper wrapper(tree_source(), &root);

  // Check that live region status was set on node.
  ui::AXNodeData data = CallSerialize(wrapper);
  std::string val;
  ASSERT_TRUE(
      data.GetStringAttribute(ax::mojom::StringAttribute::kLiveStatus, &val));
  ASSERT_EQ("polite", val);

  ASSERT_TRUE(data.GetStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, &val));
  ASSERT_EQ("polite", val);
}

TEST_F(AccessibilityNodeInfoDataWrapperTest, CustomActions) {
  AXNodeInfoData node;
  AccessibilityNodeInfoDataWrapper wrapper(tree_source(), &node);

  // Check if a custom action is properly serialized.
  AddCustomAction(&node, 300, "This is label");

  ui::AXNodeData data = CallSerialize(wrapper);
  std::vector<int> result_ids;
  std::vector<std::string> result_labels;
  EXPECT_TRUE(data.HasAction(ax::mojom::Action::kCustomAction));
  EXPECT_TRUE(data.GetIntListAttribute(
      ax::mojom::IntListAttribute::kCustomActionIds, &result_ids));
  EXPECT_EQ(std::vector<int>({300}), result_ids);
  EXPECT_TRUE(data.GetStringListAttribute(
      ax::mojom::StringListAttribute::kCustomActionDescriptions,
      &result_labels));
  EXPECT_EQ(std::vector<std::string>({"This is label"}), result_labels);
}

TEST_F(AccessibilityNodeInfoDataWrapperTest, ActionLabel) {
  AXNodeInfoData root;
  root.id = 1;
  AccessibilityNodeInfoDataWrapper wrapper(tree_source(), &root);

  // Check if labels for click and long click are serialized as kDoDefaultLabel
  // and kLongClickLabel.
  AddStandardAction(&root, AXActionType::CLICK, "click label");
  AddStandardAction(&root, AXActionType::LONG_CLICK, "long click label");

  ui::AXNodeData data = CallSerialize(wrapper);
  std::string val;
  EXPECT_TRUE(data.GetStringAttribute(
      ax::mojom::StringAttribute::kDoDefaultLabel, &val));
  EXPECT_EQ("click label", val);
  EXPECT_TRUE(data.GetStringAttribute(
      ax::mojom::StringAttribute::kLongClickLabel, &val));
  EXPECT_EQ("long click label", val);
}
}  // namespace arc
