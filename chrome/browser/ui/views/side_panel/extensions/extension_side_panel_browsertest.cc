// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_api.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_actions.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry_observer.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/test_image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "ui/actions/actions.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace extensions {
namespace {

SidePanelEntry::Key GetKey(const ExtensionId& id) {
  return SidePanelEntry::Key(SidePanelEntry::Id::kExtension, id);
}

// A class which waits on various SidePanelEntryObserver events.
class TestSidePanelEntryWaiter : public SidePanelEntryObserver {
 public:
  explicit TestSidePanelEntryWaiter(SidePanelEntry* entry) {
    side_panel_entry_observation_.Observe(entry);
  }

  ~TestSidePanelEntryWaiter() override = default;
  TestSidePanelEntryWaiter(const TestSidePanelEntryWaiter& other) = delete;
  TestSidePanelEntryWaiter& operator=(const TestSidePanelEntryWaiter& other) =
      delete;

  void WaitForEntryShown() { entry_shown_run_loop_.Run(); }

  void WaitForEntryHidden() { entry_hidden_run_loop_.Run(); }

  void WaitForIconUpdated() { icon_updated_run_loop_.Run(); }

 private:
  void OnEntryShown(SidePanelEntry* entry) override {
    entry_shown_run_loop_.QuitWhenIdle();
  }

  void OnEntryHidden(SidePanelEntry* entry) override {
    entry_hidden_run_loop_.QuitWhenIdle();
  }

  void OnEntryIconUpdated(SidePanelEntry* entry) override {
    icon_updated_run_loop_.QuitWhenIdle();
  }

  base::RunLoop entry_shown_run_loop_;
  base::RunLoop entry_hidden_run_loop_;
  base::RunLoop icon_updated_run_loop_;
  base::ScopedObservation<SidePanelEntry, SidePanelEntryObserver>
      side_panel_entry_observation_{this};
};

// A class which waits for an extension's SidePanelEntry to be registered and/or
// deregistered.
class ExtensionSidePanelRegistryWaiter : public SidePanelRegistryObserver {
 public:
  explicit ExtensionSidePanelRegistryWaiter(SidePanelRegistry* registry,
                                            const ExtensionId& extension_id)
      : extension_id_(extension_id) {
    side_panel_registry_observation_.Observe(registry);
  }

  ~ExtensionSidePanelRegistryWaiter() override = default;
  ExtensionSidePanelRegistryWaiter(
      const ExtensionSidePanelRegistryWaiter& other) = delete;
  ExtensionSidePanelRegistryWaiter& operator=(
      const ExtensionSidePanelRegistryWaiter& other) = delete;

  // Waits until the entry for `extension_id_` is registered.
  void WaitForRegistration() { registration_run_loop_.Run(); }

  // Waits until the entry for `extension_id_` is deregistered.
  void WaitForDeregistration() { deregistration_run_loop_.Run(); }

 private:
  // SidePanelRegistryObserver implementation.
  void OnEntryRegistered(SidePanelRegistry* registry,
                         SidePanelEntry* entry) override {
    if (entry->key() == GetKey(extension_id_)) {
      registration_run_loop_.QuitWhenIdle();
    }
  }

  void OnEntryWillDeregister(SidePanelRegistry* registry,
                             SidePanelEntry* entry) override {
    if (entry->key() == GetKey(extension_id_)) {
      deregistration_run_loop_.QuitWhenIdle();
    }
  }

  ExtensionId extension_id_;
  base::RunLoop registration_run_loop_;
  base::RunLoop deregistration_run_loop_;
  base::ScopedObservation<SidePanelRegistry, SidePanelRegistryObserver>
      side_panel_registry_observation_{this};
};

class ExtensionSidePanelBrowserTest : public ExtensionBrowserTest {
 public:
  explicit ExtensionSidePanelBrowserTest(bool enable_open_panel = false) {
    std::vector<base::test::FeatureRef> features;
    features.push_back(extensions_features::kExtensionSidePanelIntegration);
    features.push_back(features::kSidePanelPinning);
    if (enable_open_panel) {
      features.push_back(extensions_features::kApiSidePanelOpen);
    }
    feature_list_.InitWithFeatures(features, {});
  }

 protected:
  int GetCurrentTabId() {
    return ExtensionTabUtil::GetTabId(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  SidePanelRegistry* GetCurrentTabRegistry() {
    return SidePanelRegistry::Get(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  void OpenNewForegroundTab() {
    int tab_count = browser()->tab_strip_model()->count();
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("http://example.com"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    ASSERT_EQ(tab_count + 1, browser()->tab_strip_model()->count());
  }

  // Calls chrome.sidePanel.setOptions() for the given `extension`, `path` and
  // `enabled` and returns when the API call is complete.
  void RunSetOptions(const Extension& extension,
                     absl::optional<int> tab_id,
                     absl::optional<std::string> path,
                     bool enabled) {
    auto function = base::MakeRefCounted<SidePanelSetOptionsFunction>();
    function->set_extension(&extension);

    std::string tab_id_arg =
        tab_id.has_value() ? base::StringPrintf(R"("tabId":%d,)", *tab_id) : "";
    std::string path_arg =
        path.has_value() ? base::StringPrintf(R"("path":"%s",)", path->c_str())
                         : "";
    std::string args =
        base::StringPrintf(R"([{%s%s"enabled":%s}])", tab_id_arg.c_str(),
                           path_arg.c_str(), enabled ? "true" : "false");
    EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile()))
        << function->GetError();
  }

  // Calls chrome.sidePanel.setPanelBehavior() for the given `extension` and
  // `openPanelOnActionClick`, and returns when the API call is complete.
  void RunSetPanelBehavior(const Extension& extension,
                           bool openPanelOnActionClick) {
    auto function = base::MakeRefCounted<SidePanelSetPanelBehaviorFunction>();
    function->set_extension(&extension);

    std::string args =
        base::StringPrintf(R"([{"openPanelOnActionClick":%s}])",
                           openPanelOnActionClick ? "true" : "false");
    EXPECT_TRUE(api_test_utils::RunFunction(function.get(), args, profile()))
        << function->GetError();
  }

  // Disables the extension's side panel for the current tab.
  void DisableForCurrentTab(const Extension& extension) {
    ExtensionSidePanelRegistryWaiter waiter(global_registry(), extension.id());
    RunSetOptions(extension, GetCurrentTabId(), /*path=*/absl::nullopt,
                  /*enabled=*/false);
    waiter.WaitForDeregistration();
    EXPECT_FALSE(global_registry()->GetEntryForKey(GetKey(extension.id())));
    EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  }

  // Shows a side panel entry and waits for the entry to be shown.
  void ShowEntryAndWait(SidePanelRegistry& registry,
                        const SidePanelEntry::Key& key) {
    TestSidePanelEntryWaiter extension_entry_waiter(
        registry.GetEntryForKey(key));
    side_panel_coordinator()->Show(key);
    extension_entry_waiter.WaitForEntryShown();
    EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  }

  void ShowEntryAndWait(const SidePanelEntry::Key& key) {
    ShowEntryAndWait(*global_registry(), key);
  }

  // Displays the contextual entry correspodning to `key` in the currently-
  // active tab.
  void ShowContextualEntryAndWait(const SidePanelEntry::Key& key) {
    ShowEntryAndWait(*SidePanelRegistry::Get(
                         browser()->tab_strip_model()->GetActiveWebContents()),
                     key);
  }

  actions::ActionItem* GetActionItemForExtension(
      const extensions::Extension* extension,
      BrowserActions* browser_actions) {
    absl::optional<actions::ActionId> extension_action_id =
        actions::ActionIdMap::StringToActionId(
            GetKey(extension->id()).ToString());
    EXPECT_TRUE(extension_action_id.has_value());
    actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
        extension_action_id.value(), browser_actions->root_action_item());
    return action_item;
  }

  // Runs a script in the extension's side panel WebContents to retrieve the
  // value of document.sidePanelTemp.
  std::string GetGlobalVariableInExtensionSidePanel(
      const ExtensionId& extension_id,
      content::WebContents* web_contents) {
    auto* extension_coordinator =
        web_contents
            ? extensions::ExtensionSidePanelManager::GetOrCreateForWebContents(
                  browser()->profile(), web_contents)
                  ->GetExtensionCoordinatorForTesting(extension_id)
            : extensions::ExtensionSidePanelManager::GetOrCreateForBrowser(
                  browser())
                  ->GetExtensionCoordinatorForTesting(extension_id);

    static constexpr char kScript[] = R"(
      document.sidePanelTemp ? document.sidePanelTemp : 'undefined';
    )";

    return content::EvalJs(
               extension_coordinator->GetHostWebContentsForTesting(), kScript)
        .ExtractString();
  }

  // Runs a script in the extension's side panel WebContents to set the value of
  // document.sidePanelTemp to `value`.
  void SetGlobalVariableInExtensionSidePanel(const ExtensionId& extension_id,
                                             content::WebContents* web_contents,
                                             const std::string& value) {
    auto* extension_coordinator =
        web_contents
            ? extensions::ExtensionSidePanelManager::GetOrCreateForWebContents(
                  browser()->profile(), web_contents)
                  ->GetExtensionCoordinatorForTesting(extension_id)
            : extensions::ExtensionSidePanelManager::GetOrCreateForBrowser(
                  browser())
                  ->GetExtensionCoordinatorForTesting(extension_id);

    std::string script =
        base::StringPrintf(R"(document.sidePanelTemp = "%s";)", value.c_str());
    ASSERT_TRUE(content::ExecJs(
        extension_coordinator->GetHostWebContentsForTesting(), script.c_str()));
  }

  SidePanelRegistry* global_registry() {
    return SidePanelCoordinator::GetGlobalSidePanelRegistry(browser());
  }

  SidePanelCoordinator* side_panel_coordinator() {
    return SidePanelUtil::GetSidePanelCoordinatorForBrowser(browser());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Test that only extensions with side panel content will have a SidePanelEntry
// registered.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest,
                       ExtensionEntryVisibleInSidePanel) {
  // Load two extensions: one with a side panel entry in its manifest and one
  // without.
  scoped_refptr<const extensions::Extension> no_side_panel_extension =
      LoadExtension(test_data_dir_.AppendASCII("common/background_script"));
  ASSERT_TRUE(no_side_panel_extension);

  scoped_refptr<const extensions::Extension> side_panel_extension =
      LoadExtension(
          test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(side_panel_extension);

  // Check if ActionItem is created.
  BrowserActions* browser_actions = BrowserActions::FromBrowser(browser());
  actions::ActionItem* action_item =
      GetActionItemForExtension(side_panel_extension.get(), browser_actions);
  EXPECT_EQ(action_item->GetText(),
            base::UTF8ToUTF16(side_panel_extension->short_name()));
  EXPECT_FALSE(action_item->GetImage().IsEmpty());

  absl::optional<actions::ActionId> no_side_panel_extension_action_id =
      actions::ActionIdMap::StringToActionId(
          GetKey(no_side_panel_extension->id()).ToString());

  EXPECT_FALSE(no_side_panel_extension_action_id.has_value());

  // Check that only the extension with the side panel entry in its manifest is
  // shown as an entry in the global side panel registry.
  EXPECT_TRUE(global_registry()->GetEntryForKey(SidePanelEntry::Key(
      SidePanelEntry::Id::kExtension, side_panel_extension->id())));
  EXPECT_FALSE(global_registry()->GetEntryForKey(SidePanelEntry::Key(
      SidePanelEntry::Id::kExtension, no_side_panel_extension->id())));

  // Unloading the extension should remove it from the registry.
  UnloadExtension(side_panel_extension->id());
  EXPECT_FALSE(global_registry()->GetEntryForKey(SidePanelEntry::Key(
      SidePanelEntry::Id::kExtension, side_panel_extension->id())));

  // Check if ActionItem is deleted.
  action_item =
      GetActionItemForExtension(side_panel_extension.get(), browser_actions);
  EXPECT_FALSE(action_item);
  // The other ActionItems should not be deleted.
  EXPECT_GE(
      browser_actions->root_action_item()->GetChildren().children().size(),
      1UL);
}

// Test that an extension's view is shown/behaves correctly in the side panel.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest,
                       ExtensionViewVisibleInsideSidePanel) {
  ExtensionTestMessageListener default_path_listener("default_path");

  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);

  SidePanelEntry::Key extension_key = GetKey(extension->id());
  SidePanelEntry* extension_entry =
      global_registry()->GetEntryForKey(extension_key);
  ASSERT_TRUE(extension_entry);

  // The key for the extension should be registered, but the side panel isn't
  // shown yet.
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());

  side_panel_coordinator()->Show(extension_key);

  // Wait until the view in the side panel is active by listening for the
  // message sent from the view's script.
  ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  // Reset the `default_path_listener`.
  default_path_listener.Reset();

  // Close and reopen the side panel. The extension's view should be recreated.
  side_panel_coordinator()->Close();
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  side_panel_coordinator()->Show(extension_key);

  ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  // Now unload the extension. The key should no longer exist in the global
  // registry and the side panel should close as a result.
  UnloadExtension(extension->id());
  EXPECT_FALSE(global_registry()->GetEntryForKey(extension_key));
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
}

// Test that an extension's SidePanelEntry is registered for new browser
// windows.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest, MultipleBrowsers) {
  // Load an extension and verify that its SidePanelEntry is registered.
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);
  SidePanelEntry::Key extension_key = GetKey(extension->id());

  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));
  BrowserActions* browser_actions = BrowserActions::FromBrowser(browser());
  actions::ActionItem* browser_one_action_item =
      GetActionItemForExtension(extension.get(), browser_actions);
  EXPECT_EQ(browser_one_action_item->GetText(),
            base::UTF8ToUTF16(extension->short_name()));

  // Open a new browser window. The extension's SidePanelEntry should also be
  // registered for the new window's global SidePanelRegistry.
  Browser* second_browser = CreateBrowser(browser()->profile());
  BrowserActions* browser_actions_second_browser =
      BrowserActions::FromBrowser(second_browser);

  SidePanelRegistry* second_global_registry =
      SidePanelCoordinator::GetGlobalSidePanelRegistry(second_browser);
  EXPECT_TRUE(second_global_registry->GetEntryForKey(extension_key));
  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));

  actions::ActionItem* browser_two_action_item = GetActionItemForExtension(
      extension.get(), browser_actions_second_browser);

  // Validate the state of the action items are still correct.
  EXPECT_EQ(browser_one_action_item->GetText(),
            base::UTF8ToUTF16(extension->short_name()));
  EXPECT_EQ(browser_two_action_item->GetText(),
            base::UTF8ToUTF16(extension->short_name()));
  // Unloading the extension should remove it from the registry.
  UnloadExtension(extension->id());
  EXPECT_FALSE(global_registry()->GetEntryForKey(
      SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension->id())));
  EXPECT_FALSE(SidePanelCoordinator::GetGlobalSidePanelRegistry(second_browser)
                   ->GetEntryForKey(SidePanelEntry::Key(
                       SidePanelEntry::Id::kExtension, extension->id())));

  browser_one_action_item =
      GetActionItemForExtension(extension.get(), browser_actions);
  browser_two_action_item = GetActionItemForExtension(
      extension.get(), browser_actions_second_browser);

  EXPECT_FALSE(browser_one_action_item);
  EXPECT_FALSE(browser_two_action_item);
}

// Test that if the side panel is closed while the extension's side panel view
// is still loading, there will not be a crash. Regression for
// crbug.com/1403168.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest, SidePanelQuicklyClosed) {
  // Load an extension and verify that its SidePanelEntry is registered.
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);
  SidePanelEntry::Key extension_key = GetKey(extension->id());

  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());

  // Quickly open the side panel showing the extension's side panel entry then
  // close it. The test should not cause any crashes after it is complete.
  side_panel_coordinator()->Show(extension_key);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  side_panel_coordinator()->Close();
}

// Test that the extension's side panel entry shows the extension's icon.
// TODO(crbug.com/1450850): Re-enable this test
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_EntryShowsExtensionIcon DISABLED_EntryShowsExtensionIcon
#else
#define MAYBE_EntryShowsExtensionIcon EntryShowsExtensionIcon
#endif
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest,
                       MAYBE_EntryShowsExtensionIcon) {
  // Load an extension and verify that its SidePanelEntry is registered.
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);

  SidePanelEntry::Key extension_key = GetKey(extension->id());
  SidePanelEntry* extension_entry =
      global_registry()->GetEntryForKey(extension_key);

  // Check that the entry's icon bitmap is identical to the bitmap of the
  // extension's icon scaled down to `extension_misc::EXTENSION_ICON_BITTY`.
  SkBitmap expected_icon_bitmap = TestImageLoader::LoadAndGetExtensionBitmap(
      extension.get(), "icon.png", extension_misc::EXTENSION_ICON_BITTY);
  const SkBitmap& actual_icon_bitmap =
      *extension_entry->icon().GetImage().ToSkBitmap();
  EXPECT_TRUE(
      gfx::test::AreBitmapsEqual(expected_icon_bitmap, actual_icon_bitmap));
}

// Test that sidePanel.setOptions() will register and deregister the extension's
// SidePanelEntry when called with enabled: true/false.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest, SetOptions_Enabled) {
  ExtensionTestMessageListener panel_2_listener("panel_2");

  // Load an extension without a default side panel path.
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/setoptions"));
  ASSERT_TRUE(extension);
  SidePanelEntry::Key extension_key = GetKey(extension->id());
  EXPECT_FALSE(global_registry()->GetEntryForKey(extension_key));

  {
    // Call setOptions({enabled: true}) and wait for the extension's
    // SidePanelEntry to be registered.
    ExtensionSidePanelRegistryWaiter waiter(global_registry(), extension->id());
    RunSetOptions(*extension, /*tab_id=*/absl::nullopt, "panel_1.html",
                  /*enabled=*/true);
    waiter.WaitForRegistration();
  }

  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));

  {
    // Call setOptions({enabled: false}) and wait for the extension's
    // SidePanelEntry to be deregistered.
    ExtensionSidePanelRegistryWaiter waiter(global_registry(), extension->id());
    RunSetOptions(*extension, /*tab_id=*/absl::nullopt, /*path=*/absl::nullopt,
                  /*enabled=*/false);
    waiter.WaitForDeregistration();
  }

  EXPECT_FALSE(global_registry()->GetEntryForKey(extension_key));

  {
    // Sanity check that re-enabling the side panel will register the entry
    // again and a view with the new side panel path can be shown.
    ExtensionSidePanelRegistryWaiter waiter(global_registry(), extension->id());
    RunSetOptions(*extension, /*tab_id=*/absl::nullopt, "panel_2.html",
                  /*enabled=*/true);
    waiter.WaitForRegistration();
  }

  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));
  side_panel_coordinator()->Show(extension_key);

  // Wait until the view in the side panel is active by listening for the
  // message sent from the view's script.
  ASSERT_TRUE(panel_2_listener.WaitUntilSatisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  {
    // Calling setOptions({enabled: false}) when the extension's SidePanelEntry
    // is shown should close the side panel.
    ExtensionSidePanelRegistryWaiter waiter(global_registry(), extension->id());
    RunSetOptions(*extension, /*tab_id=*/absl::nullopt, /*path=*/absl::nullopt,
                  /*enabled=*/false);
    waiter.WaitForDeregistration();
  }

  EXPECT_FALSE(global_registry()->GetEntryForKey(extension_key));
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
}

// Test that sidePanel.setOptions() will change what is shown in the extension's
// SidePanelEntry's view when called with different paths.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest, SetOptions_Path) {
  ExtensionTestMessageListener default_path_listener("default_path");
  ExtensionTestMessageListener panel_1_listener("panel_1");

  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);
  auto* extension_coordinator =
      extensions::ExtensionSidePanelManager::GetOrCreateForBrowser(browser())
          ->GetExtensionCoordinatorForTesting(extension->id());

  SidePanelEntry::Key extension_key = GetKey(extension->id());
  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));

  // Check that the extension's side panel view shows the most recently set
  // path.
  RunSetOptions(*extension, /*tab_id=*/absl::nullopt, "panel_1.html",
                /*enabled=*/true);
  side_panel_coordinator()->Show(extension_key);
  ASSERT_TRUE(panel_1_listener.WaitUntilSatisfied());
  EXPECT_FALSE(default_path_listener.was_satisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  // Check that changing the path while the view is active will cause the view
  // to navigate to the new path.
  RunSetOptions(*extension, /*tab_id=*/absl::nullopt, "default_path.html",
                /*enabled=*/true);
  ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  // Switch to the reading list in the side panel and check that the extension
  // view is cached (i.e. the view exists but is not shown, and its web contents
  // still exists).
  ShowEntryAndWait(SidePanelEntry::Key(SidePanelEntry::Id::kReadingList));

  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key)->CachedView());

  panel_1_listener.Reset();
  content::WebContentsDestroyedWatcher destroyed_watcher(
      extension_coordinator->GetHostWebContentsForTesting());

  // Test calling setOptions with a different path when the extension's view is
  // cached. The cached view should then be invalidated and its web contents are
  // destroyed.
  RunSetOptions(*extension, /*tab_id=*/absl::nullopt, "panel_1.html",
                /*enabled=*/true);
  destroyed_watcher.Wait();

  // When the extension's entry is shown again, the view with the updated path
  // should be active.
  side_panel_coordinator()->Show(extension_key);
  ASSERT_TRUE(panel_1_listener.WaitUntilSatisfied());
}

// Test that calling window.close() from an extension side panel deletes the
// panel's web contents and closes the extension's side panel if it's also
// shown.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest, WindowCloseCalled) {
  // Install an extension and show its side panel.
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);
  SidePanelEntry::Key extension_key = GetKey(extension->id());
  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));

  {
    ExtensionTestMessageListener default_path_listener("default_path");
    side_panel_coordinator()->Show(extension_key);
    ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
    EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  }

  auto* extension_coordinator =
      extensions::ExtensionSidePanelManager::GetOrCreateForBrowser(browser())
          ->GetExtensionCoordinatorForTesting(extension->id());

  // Call window.close() from the extension's side panel page and wait for the
  // web contents to be destroyed.
  {
    content::WebContentsDestroyedWatcher destroyed_watcher(
        extension_coordinator->GetHostWebContentsForTesting());
    ASSERT_TRUE(
        content::ExecJs(extension_coordinator->GetHostWebContentsForTesting(),
                        "window.close();"));
    destroyed_watcher.Wait();
  }

  // The side panel should now be closed.
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());

  // Show the extension's side panel again.
  ExtensionTestMessageListener default_path_listener("default_path");
  side_panel_coordinator()->Show(extension_key);
  ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  // Show another side panel type so the extension's panel's view gets cached.
  ShowEntryAndWait(SidePanelEntry::Key(SidePanelEntry::Id::kReadingList));
  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key)->CachedView());

  // Calling window.close() from within the panel should invalidate the cached
  // view when the extension panel is not shown.
  content::WebContentsDestroyedWatcher destroyed_watcher(
      extension_coordinator->GetHostWebContentsForTesting());
  ASSERT_TRUE(
      content::ExecJs(extension_coordinator->GetHostWebContentsForTesting(),
                      "window.close();"));
  destroyed_watcher.Wait();

  // The side panel should be open because the reading list entry is still
  // shown.
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
}

// Test that calling window.close() from an extension's side panel deletes the
// panel's web contents and closes the extension's side panel if it's also
// shown.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest,
                       WindowCloseCalledFromTabSpecificPanel) {
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/setoptions"));
  ASSERT_TRUE(extension);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  SidePanelEntry::Key extension_key = GetKey(extension->id());
  // Call setOptions({enabled: true}) with a tab ID and new path, and wait for
  // the extension's SidePanelEntry to be registered.
  ExtensionSidePanelRegistryWaiter waiter(
      SidePanelRegistry::Get(active_web_contents), extension->id());
  RunSetOptions(*extension, GetCurrentTabId(), "panel_2.html",
                /*enabled=*/true);
  waiter.WaitForRegistration();

  ExtensionTestMessageListener panel_2_listener("panel_2");
  side_panel_coordinator()->Show(extension_key);

  // Wait until the view in the side panel is active by listening for the
  // message sent from the view's script.
  ASSERT_TRUE(panel_2_listener.WaitUntilSatisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  auto* extension_coordinator =
      extensions::ExtensionSidePanelManager::GetOrCreateForWebContents(
          browser()->profile(), active_web_contents)
          ->GetExtensionCoordinatorForTesting(extension->id());
  content::WebContentsDestroyedWatcher destroyed_watcher(
      extension_coordinator->GetHostWebContentsForTesting());
  ASSERT_TRUE(
      content::ExecJs(extension_coordinator->GetHostWebContentsForTesting(),
                      "window.close();"));
  destroyed_watcher.Wait();
}

// Test that calling window.close() from an extension side panel when it is
// shown closes the side panel even if another entry is loading and will be
// shown.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest,
                       WindowCloseCalledWhenLoading) {
  // Install an extension and show its side panel.
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);
  SidePanelEntry::Key extension_key = GetKey(extension->id());
  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));

  {
    ExtensionTestMessageListener default_path_listener("default_path");
    side_panel_coordinator()->Show(extension_key);
    ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
    EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  }

  auto* extension_coordinator =
      extensions::ExtensionSidePanelManager::GetOrCreateForBrowser(browser())
          ->GetExtensionCoordinatorForTesting(extension->id());

  // Start showing another entry and call window.close() from the extension's
  // side panel page while the other entry is still loading but not shown. The
  // extension's side panel web content should still be destroyed and the side
  // panel will close.
  {
    side_panel_coordinator()->Show(SidePanelEntry::Id::kReadingList);

    content::WebContentsDestroyedWatcher destroyed_watcher(
        extension_coordinator->GetHostWebContentsForTesting());
    ASSERT_TRUE(
        content::ExecJs(extension_coordinator->GetHostWebContentsForTesting(),
                        "window.close();"));
    destroyed_watcher.Wait();
  }

  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
}

// Test that calling sidePanel.setOptions({enabled: false}) for a specific tab
// will hide the extension's global side panel for that tab.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest, HideGlobalPanelForTab) {
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);

  SidePanelEntry::Key extension_key = GetKey(extension->id());
  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));

  // Show the extension's side panel and set a global variable to change the
  // state of the side panel's page.
  ExtensionTestMessageListener default_path_listener("default_path");
  side_panel_coordinator()->Show(extension_key);
  ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  SetGlobalVariableInExtensionSidePanel(
      extension->id(), /*web_contents=*/nullptr, "altered_state");
  EXPECT_EQ("altered_state", GetGlobalVariableInExtensionSidePanel(
                                 extension->id(), /*web_contents=*/nullptr));

  // Disable the extension's side panel for the current tab.
  DisableForCurrentTab(*extension);

  // Calling sidePanel.setOptions({enabled: true}) for the current tab should
  // re-register the entry.
  {
    ExtensionSidePanelRegistryWaiter waiter(global_registry(), extension->id());
    RunSetOptions(*extension, GetCurrentTabId(), /*path=*/absl::nullopt,
                  /*enabled=*/true);
    waiter.WaitForRegistration();
    EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));
    EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  }

  // Show the side panel entry and check its state to verify that it's the same
  // page as before.
  ShowEntryAndWait(extension_key);
  EXPECT_EQ("altered_state", GetGlobalVariableInExtensionSidePanel(
                                 extension->id(), /*web_contents=*/nullptr));

  // Disable the extension's side panel for the current tab again.
  DisableForCurrentTab(*extension);

  // Open a new tab and navigate to it. The extension's side panel should be
  // available again since it's not disabled for the new tab.
  {
    ExtensionSidePanelRegistryWaiter waiter(global_registry(), extension->id());
    OpenNewForegroundTab();
    ASSERT_TRUE(browser()->tab_strip_model()->IsTabSelected(1));

    waiter.WaitForRegistration();
    EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));
    EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  }

  // Show the side panel entry and check its state to verify that it's the same
  // page as before.
  ShowEntryAndWait(extension_key);
  EXPECT_EQ("altered_state", GetGlobalVariableInExtensionSidePanel(
                                 extension->id(), /*web_contents=*/nullptr));

  // Go back to the first tab where the side panel is disabled and verify the
  // extension's side panel is no longer there.
  {
    ExtensionSidePanelRegistryWaiter waiter(global_registry(), extension->id());
    browser()->tab_strip_model()->ActivateTabAt(0);
    waiter.WaitForDeregistration();
    EXPECT_FALSE(global_registry()->GetEntryForKey(extension_key));
    EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  }
}

// Test that the saved view state for the hidden global extension side panel is
// invalidated if setOptions({enabled: false}) is called without a tab ID.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest,
                       DisableGlobalPanelWhileHidden) {
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);
  auto* extension_coordinator =
      extensions::ExtensionSidePanelManager::GetOrCreateForBrowser(browser())
          ->GetExtensionCoordinatorForTesting(extension->id());

  SidePanelEntry::Key extension_key = GetKey(extension->id());
  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));

  // Show the extension's side panel.
  ExtensionTestMessageListener default_path_listener("default_path");
  side_panel_coordinator()->Show(extension_key);
  ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  // Disable the extension's side panel for the current tab.
  {
    ExtensionSidePanelRegistryWaiter waiter(global_registry(), extension->id());
    RunSetOptions(*extension, GetCurrentTabId(), /*path=*/absl::nullopt,
                  /*enabled=*/false);
    waiter.WaitForDeregistration();
    EXPECT_FALSE(global_registry()->GetEntryForKey(extension_key));
    EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  }

  // There should be web contents from the saved view.
  ASSERT_TRUE(extension_coordinator->GetHostWebContentsForTesting());
  content::WebContentsDestroyedWatcher destroyed_watcher(
      extension_coordinator->GetHostWebContentsForTesting());

  // Calling setOptions({enabled: false}) for all tabs should destroy the
  // contents.
  RunSetOptions(*extension, /*tab_id=*/absl::nullopt, /*path=*/absl::nullopt,
                /*enabled=*/false);
  destroyed_watcher.Wait();

  // Sanity check that calling setOptions({enabled: true}) for all tabs while on
  // a tab where the panel is disabled should be a no-op.
  RunSetOptions(*extension, /*tab_id=*/absl::nullopt, "default_path.html",
                /*enabled=*/true);
  EXPECT_FALSE(global_registry()->GetEntryForKey(extension_key));

  // Open a new tab and navigate to it. The extension's side panel should be
  // available again since it's not disabled for the new tab.
  {
    ExtensionSidePanelRegistryWaiter waiter(global_registry(), extension->id());
    OpenNewForegroundTab();
    ASSERT_TRUE(browser()->tab_strip_model()->IsTabSelected(1));

    waiter.WaitForRegistration();
    EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));
    EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  }
}

// Test that when the extension's side panel is shown, switching from a tab
// where the panel is enabled to one where it's disabled then back to the first
// tab will re-register the entry but not show it. This behavior is a little
// weird, but trying to have it reopen causes far more complexity than is
// worthwhile.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest, ReEnabledPanelNotShown) {
  // Open a second tab and switch back to the first tab.
  OpenNewForegroundTab();
  ASSERT_TRUE(browser()->tab_strip_model()->IsTabSelected(1));

  int second_tab_id = GetCurrentTabId();
  browser()->tab_strip_model()->ActivateTabAt(0);

  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);

  SidePanelEntry::Key extension_key = GetKey(extension->id());
  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));

  // Show the extension's side panel.
  ExtensionTestMessageListener default_path_listener("default_path");
  side_panel_coordinator()->Show(extension_key);
  ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  // Disable the extension's side panel for the second tab, which shouldn't do
  // anything here since we're on the first tab.
  RunSetOptions(*extension, second_tab_id, /*path=*/absl::nullopt,
                /*enabled=*/false);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  // Switch to the second tab and verify that the extension's entry is no longer
  // registered.
  {
    ExtensionSidePanelRegistryWaiter waiter(global_registry(), extension->id());
    browser()->tab_strip_model()->ActivateTabAt(1);
    waiter.WaitForDeregistration();
    EXPECT_FALSE(global_registry()->GetEntryForKey(extension_key));
    EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  }

  // Switch back to the first tab and verify that the extension's entry is
  // registered again but is not showing.
  {
    ExtensionSidePanelRegistryWaiter waiter(global_registry(), extension->id());
    browser()->tab_strip_model()->ActivateTabAt(0);
    waiter.WaitForRegistration();
    EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));
    EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  }
}

// Test that calling setOptions on the current tab while the global entry is
// showing should show the new entry for the current tab.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest,
                       TabSpecificPanelShownOnOptionsUpdate) {
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);

  SidePanelEntry::Key extension_key = GetKey(extension->id());
  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));

  ShowEntryAndWait(extension_key);

  {
    ExtensionTestMessageListener panel_1_listener("panel_1");
    // Call setOptions({enabled: true}) with a tab ID and new path, and wait for
    // the extension's SidePanelEntry to be registered. The extension's side
    // panel should then show the new entry for the first tab which displays
    // `panel_1.html`.
    ExtensionSidePanelRegistryWaiter waiter(
        SidePanelRegistry::Get(
            browser()->tab_strip_model()->GetActiveWebContents()),
        extension->id());
    RunSetOptions(*extension, GetCurrentTabId(), "panel_1.html",
                  /*enabled=*/true);
    waiter.WaitForRegistration();
    ASSERT_TRUE(panel_1_listener.WaitUntilSatisfied());
    EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

    EXPECT_EQ(GetCurrentTabRegistry()->active_entry().value(),
              GetCurrentTabRegistry()->GetEntryForKey(extension_key));
  }

  // Sanity check that calling setOptions() for the current tab with a different
  // tab will change the view shown in the side panel's contextual entry.
  ExtensionTestMessageListener default_path_listener("default_path");
  RunSetOptions(*extension, GetCurrentTabId(), "default_path.html",
                /*enabled=*/true);
  ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
}

// Test that when switching tabs, the new tab shows the extension's contextual
// entry if one exists, or the global entry if there is no tab-specific entry
// specified for that tab.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest,
                       ShowTabSpecificPaneOnTabSwitch) {
  // Open a second tab and switch back to the first tab.
  OpenNewForegroundTab();
  ASSERT_TRUE(browser()->tab_strip_model()->IsTabSelected(1));

  int second_tab_id = GetCurrentTabId();
  browser()->tab_strip_model()->ActivateTabAt(0);

  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);

  SidePanelEntry::Key extension_key = GetKey(extension->id());
  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));

  // Call setOptions for the second tab.
  ExtensionSidePanelRegistryWaiter waiter(
      SidePanelRegistry::Get(browser()->tab_strip_model()->GetWebContentsAt(1)),
      extension->id());
  RunSetOptions(*extension, second_tab_id, "panel_1.html",
                /*enabled=*/true);
  waiter.WaitForRegistration();

  // Show the extension's side panel on the first tab.
  ShowEntryAndWait(extension_key);

  // Switch to the second tab: this should cause the extension's entry for that
  // tab to be shown.
  ExtensionTestMessageListener panel_1_listener("panel_1");
  browser()->tab_strip_model()->ActivateTabAt(1);
  ASSERT_TRUE(panel_1_listener.WaitUntilSatisfied());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  // Switch back to the first tab: the global entry should be shown.
  TestSidePanelEntryWaiter entry_shown_waiter(
      global_registry()->GetEntryForKey(extension_key));
  browser()->tab_strip_model()->ActivateTabAt(0);
  entry_shown_waiter.WaitForEntryShown();
}

// Test that the view state between the extension's global side panel entry and
// all of its tab-specific side panel entries are independent of each other.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest,
                       TabSpecificPanelsOwnViewState) {
  // Open a second tab and switch back to the first tab.
  OpenNewForegroundTab();
  ASSERT_TRUE(browser()->tab_strip_model()->IsTabSelected(1));

  int second_tab_id = GetCurrentTabId();
  browser()->tab_strip_model()->ActivateTabAt(0);
  int first_tab_id = GetCurrentTabId();

  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);

  // Set a local variable's value to "GLOBAL" for the extension's global side
  // panel's WebContents.
  SidePanelEntry::Key extension_key = GetKey(extension->id());

  {
    ExtensionTestMessageListener default_path_listener("default_path");
    side_panel_coordinator()->Show(extension_key);
    ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
    EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  }

  EXPECT_EQ("undefined", GetGlobalVariableInExtensionSidePanel(
                             extension->id(), /*web_contents=*/nullptr));
  SetGlobalVariableInExtensionSidePanel(extension->id(),
                                        /*web_contents=*/nullptr, "GLOBAL");
  EXPECT_EQ("GLOBAL", GetGlobalVariableInExtensionSidePanel(
                          extension->id(), /*web_contents=*/nullptr));

  auto* first_tab_contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  auto* second_tab_contents = browser()->tab_strip_model()->GetWebContentsAt(1);

  {
    // Set a local variable's value to "TAB 1" for the extension's side panel's
    // view on the first tab.
    ExtensionTestMessageListener default_path_listener("default_path");

    SidePanelRegistry* first_tab_registry =
        SidePanelRegistry::Get(first_tab_contents);
    ExtensionSidePanelRegistryWaiter waiter(first_tab_registry,
                                            extension->id());
    RunSetOptions(*extension, first_tab_id, "default_path.html",
                  /*enabled=*/true);
    waiter.WaitForRegistration();

    ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());

    // Despite sharing the same path, the instance of default_path.html that is
    // specifically registered for `first_tab_id` is a different entry/view than
    // default_path.html registered for all tabs.
    EXPECT_EQ("undefined", GetGlobalVariableInExtensionSidePanel(
                               extension->id(), first_tab_contents));
    SetGlobalVariableInExtensionSidePanel(extension->id(), first_tab_contents,
                                          "TAB 1");
  }

  {
    // Set a local variable's value to "TAB 2" for the extension's side panel's
    // view on the second tab.
    SidePanelRegistry* second_tab_registry =
        SidePanelRegistry::Get(second_tab_contents);
    ExtensionSidePanelRegistryWaiter waiter(second_tab_registry,
                                            extension->id());
    RunSetOptions(*extension, second_tab_id, "default_path.html",
                  /*enabled=*/true);
    waiter.WaitForRegistration();

    TestSidePanelEntryWaiter entry_shown_waiter(
        second_tab_registry->GetEntryForKey(extension_key));
    browser()->tab_strip_model()->ActivateTabAt(1);
    entry_shown_waiter.WaitForEntryShown();

    EXPECT_EQ("undefined", GetGlobalVariableInExtensionSidePanel(
                               extension->id(), second_tab_contents));
    SetGlobalVariableInExtensionSidePanel(extension->id(), second_tab_contents,
                                          "TAB 2");
  }

  // Check that the global variable's value for the extension's global and
  // contextual (first tab) entries are not affected.
  EXPECT_EQ("GLOBAL", GetGlobalVariableInExtensionSidePanel(
                          extension->id(), /*web_contents=*/nullptr));
  EXPECT_EQ("TAB 1", GetGlobalVariableInExtensionSidePanel(extension->id(),
                                                           first_tab_contents));
}

// Test that unloading an extension after its tab-specific side panel is moved
// to another browser does not crash. This tests a rare use case where the
// extension's contextual SidePanelEntry is deregistered before its global one,
// all while the extension itself is being unloaded. See
// ExtensionSidePanelCoordinator::CreateVIew for more details.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest,
                       UnloadExtensionAfterMovingTab) {
  OpenNewForegroundTab();
  ASSERT_TRUE(browser()->tab_strip_model()->IsTabSelected(1));
  auto* second_tab_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  int second_tab_id = GetCurrentTabId();

  // Load an extension and verify that its SidePanelEntry is registered.
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);
  SidePanelEntry::Key extension_key = GetKey(extension->id());
  EXPECT_TRUE(global_registry()->GetEntryForKey(extension_key));

  {
    // Register a SidePanelEntry for the extension for the second tab.
    SidePanelRegistry* second_tab_registry =
        SidePanelRegistry::Get(second_tab_contents);
    ExtensionSidePanelRegistryWaiter waiter(second_tab_registry,
                                            extension->id());
    RunSetOptions(*extension, second_tab_id, "panel_1.html",
                  /*enabled=*/true);
    waiter.WaitForRegistration();

    ExtensionTestMessageListener panel_1_listener("panel_1");
    side_panel_coordinator()->Show(extension_key);
    ASSERT_TRUE(panel_1_listener.WaitUntilSatisfied());
  }

  // Detach the second tab from `browser()`
  std::unique_ptr<content::WebContents> detached_contents =
      browser()->tab_strip_model()->DetachWebContentsAtForInsertion(
          /*index=*/1);
  ASSERT_EQ(second_tab_contents, detached_contents.get());

  // Open a new browser window and add `detached_contents` to a new tab.
  Browser* second_browser = CreateBrowser(browser()->profile());
  TabStripModel* target_tab_strip =
      ExtensionTabUtil::GetEditableTabStripModel(second_browser);
  target_tab_strip->InsertWebContentsAt(
      /*index=*/1, std::move(detached_contents), AddTabTypes::ADD_NONE);

  // Switch to the newly moved tab.
  ASSERT_EQ(2, second_browser->tab_strip_model()->count());
  second_browser->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());

  // Unloading the extension at this point should not crash the browser.
  UnloadExtension(extension->id());
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
}

// Test that when the openSidePanelOnClick pref is true, clicking the extension
// icon will show the extension's entry if it's not shown, or close
// the side panel if the extension's entry is shown.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest,
                       ToggleExtensionEntryOnUserAction) {
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);

  // Create a helper that will click the extension's icon from the menu to
  // trigger an extension action.
  std::unique_ptr<ExtensionActionTestHelper> action_helper =
      ExtensionActionTestHelper::Create(browser());

  SidePanelEntry::Key extension_key = GetKey(extension->id());

  RunSetPanelBehavior(*extension, /*openPanelOnActionClick=*/true);
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());

  {
    ExtensionTestMessageListener default_path_listener("default_path");
    // Clicking the icon should show the extension's entry.
    action_helper->Press(extension->id());
    ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
    EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  }

  // Switch over to another side panel entry.
  ShowEntryAndWait(SidePanelEntry::Key(SidePanelEntry::Id::kReadingList));

  {
    TestSidePanelEntryWaiter entry_shown_waiter(
        global_registry()->GetEntryForKey(extension_key));
    // Since the extension's entry is not shown, clicking the icon should show
    // it.
    action_helper->Press(extension->id());
    entry_shown_waiter.WaitForEntryShown();
    EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  }

  {
    TestSidePanelEntryWaiter entry_hidden_waiter(
        global_registry()->GetEntryForKey(extension_key));
    // Clicking the icon when the extension's entry is shown should close the
    // side panel.
    action_helper->Press(extension->id());
    entry_hidden_waiter.WaitForEntryHidden();
  }

  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
}

// Test that extension action behavior falls back to defaults if the extension
// has no side panel panel for the current tab (global or contextual) or if the
// openSidePanelOnClick pref is false.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelBrowserTest,
                       FallbackActionWithoutSidePanel) {
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/with_action_onclick"));
  ASSERT_TRUE(extension);

  // Create a helper that will click the extension's icon from the menu to
  // trigger an extension action.
  std::unique_ptr<ExtensionActionTestHelper> action_helper =
      ExtensionActionTestHelper::Create(browser());

  SidePanelEntry::Key extension_key = GetKey(extension->id());

  RunSetPanelBehavior(*extension, /*openPanelOnActionClick=*/true);
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());

  {
    ExtensionTestMessageListener default_path_listener("default_path");
    // Clicking the icon should show the extension's entry.
    action_helper->Press(extension->id());
    ASSERT_TRUE(default_path_listener.WaitUntilSatisfied());
    EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  }

  // Set the pref to false.
  RunSetPanelBehavior(*extension, /*openPanelOnActionClick=*/false);

  {
    ExtensionTestMessageListener action_clicked_listener("action_clicked");
    // Since the pref is false, clicking the icon will fall back to triggering
    // chrome.action.onClicked, which satisfies `action_clicked_listener`.
    action_helper->Press(extension->id());
    ASSERT_TRUE(action_clicked_listener.WaitUntilSatisfied());
    EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  }

  // Set the pref to true but disable the extension's side panel for the current
  // tab.
  RunSetPanelBehavior(*extension, /*openPanelOnActionClick=*/true);
  DisableForCurrentTab(*extension);

  {
    ExtensionTestMessageListener default_path_listener("default_path");
    ExtensionTestMessageListener action_clicked_listener("action_clicked");
    // Clicking the icon will fall back to triggering chrome.action.onClicked,
    action_helper->Press(extension->id());
    ASSERT_TRUE(action_clicked_listener.WaitUntilSatisfied());
    EXPECT_FALSE(default_path_listener.was_satisfied());
  }
}

class ExtensionOpenSidePanelBrowserTest : public ExtensionSidePanelBrowserTest {
 public:
  ExtensionOpenSidePanelBrowserTest()
      : ExtensionSidePanelBrowserTest(/*enable_open_panel=*/true) {}
  ~ExtensionOpenSidePanelBrowserTest() override = default;

 protected:
  // Loads up a stub side panel extension.
  const Extension* LoadSidePanelExtension() {
    TestExtensionDir test_dir;
    static constexpr char kManifest[] =
        R"({
             "name": "Side Panel Extension",
             "manifest_version": 3,
             "version": "0.1",
             "permissions": ["sidePanel"]
           })";
    test_dir.WriteManifest(kManifest);
    test_dir.WriteFile(FILE_PATH_LITERAL("panel.html"), "<html>hello</html>");
    const Extension* extension = LoadExtension(test_dir.UnpackedPath());
    test_dirs_.push_back(std::move(test_dir));
    return extension;
  }

  void RunOpenPanelForTab(const Extension& extension, int tab_id) {
    RunOpenPanel(extension, tab_id, /*window_id=*/absl::nullopt);
  }
  void RunOpenPanelForWindow(const Extension& extension, int window_id) {
    RunOpenPanel(extension, /*tab_id=*/absl::nullopt, window_id);
  }

  int GetCurrentWindowId() { return ExtensionTabUtil::GetWindowId(browser()); }

 private:
  void RunOpenPanel(const Extension& extension,
                    absl::optional<int> tab_id,
                    absl::optional<int> window_id) {
    auto function = base::MakeRefCounted<SidePanelOpenFunction>();
    function->set_extension(&extension);

    base::Value::Dict options;
    if (tab_id) {
      options.Set("tabId", *tab_id);
    }
    if (window_id) {
      options.Set("windowId", *window_id);
    }
    std::string args_str;
    base::JSONWriter::Write(base::Value::List().Append(std::move(options)),
                            &args_str);
    function->set_user_gesture(true);
    EXPECT_TRUE(
        api_test_utils::RunFunction(function.get(), args_str, profile()))
        << function->GetError();
  }

  std::vector<TestExtensionDir> test_dirs_;
};

// Tests that calling `sidePanel.open()` for an extension with a global panel
// registered opens the panel on the specified tab.
IN_PROC_BROWSER_TEST_F(ExtensionOpenSidePanelBrowserTest,
                       OpenSidePanel_OpenGlobalPanelOnActiveTab) {
  const Extension* extension = LoadSidePanelExtension();
  ASSERT_TRUE(extension);
  // Register a global side panel.
  RunSetOptions(*extension, /*tab_id=*/absl::nullopt, "panel.html",
                /*enabled=*/true);

  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  // Run `sidePanel.open()`. The panel should open.
  RunOpenPanelForTab(*extension, GetCurrentTabId());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelEntryShowing(
      GetKey(extension->id())));
}

// Tests that calling `sidePanel.open()` for an extension with a global panel
// registered opens the panel on all tabs (since the registration is global,
// rather than contextual).
IN_PROC_BROWSER_TEST_F(ExtensionOpenSidePanelBrowserTest,
                       OpenSidePanel_OpenGlobalPanelOnInactiveTab) {
  const Extension* extension = LoadSidePanelExtension();
  ASSERT_TRUE(extension);
  // Register a global side panel.
  RunSetOptions(*extension, /*tab_id=*/absl::nullopt, "panel.html",
                /*enabled=*/true);

  int tab_id = GetCurrentTabId();
  // Open a new tab.
  OpenNewForegroundTab();
  int new_tab_id = GetCurrentTabId();
  ASSERT_NE(new_tab_id, tab_id);

  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());

  // Open the side panel on the original tab.
  RunOpenPanelForTab(*extension, tab_id);

  // Because it's a global side panel, it should be displaying in both the
  // original and the new tab.
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelEntryShowing(
      GetKey(extension->id())));
}

// Tests that calling `sidePanel.open()` will override a different, active
// global side panel in the tab when the active tab's tab ID is provided.
IN_PROC_BROWSER_TEST_F(
    ExtensionOpenSidePanelBrowserTest,
    OpenSidePanel_OverridesGlobalPanelWithActiveTabIdProvided) {
  const Extension* extension = LoadSidePanelExtension();
  ASSERT_TRUE(extension);
  // Register a global side panel.
  RunSetOptions(*extension, /*tab_id=*/absl::nullopt, "panel.html",
                /*enabled=*/true);

  // Open a different global side panel (reading list).
  ShowEntryAndWait(SidePanelEntry::Key(SidePanelEntry::Id::kReadingList));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(SidePanelEntry::Id::kReadingList,
            side_panel_coordinator()->GetCurrentEntryId());

  // Call `sidePanel.open()` on the current tab.
  RunOpenPanelForTab(*extension, GetCurrentTabId());

  // The extension side panel should be able to override the currently-open
  // side panel.
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelEntryShowing(
      GetKey(extension->id())));
}

// Tests that calling `sidePanel.open()` will override a different, active
// global side panel in the tab when an inactive tab ID is provided.
IN_PROC_BROWSER_TEST_F(
    ExtensionOpenSidePanelBrowserTest,
    OpenSidePanel_OverridesGlobalPanelWithInactiveTabIdProvided) {
  const Extension* extension = LoadSidePanelExtension();
  ASSERT_TRUE(extension);
  // Register a global side panel.
  RunSetOptions(*extension, /*tab_id=*/absl::nullopt, "panel.html",
                /*enabled=*/true);

  int first_tab_id = GetCurrentTabId();
  OpenNewForegroundTab();

  // Open a different global side panel (reading list).
  ShowEntryAndWait(SidePanelEntry::Key(SidePanelEntry::Id::kReadingList));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(SidePanelEntry::Id::kReadingList,
            side_panel_coordinator()->GetCurrentEntryId());

  // Call `sidePanel.open()` on the inactive tab.
  RunOpenPanelForTab(*extension, first_tab_id);

  // Even though the tab ID provided was for an inactive tab, the extension side
  // panel should be able to override the currently-open side panel in the
  // active tab since they are both global entries.
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelEntryShowing(
      GetKey(extension->id())));
}

// Tests that calling `sidePanel.open()` with a contextual panel on the active
// tab will open that contextual panel and will not override a global panel
// that's open in a different tab.
IN_PROC_BROWSER_TEST_F(ExtensionOpenSidePanelBrowserTest,
                       OpenSidePanel_OpenContextualPanelInActiveTab) {
  const Extension* extension = LoadSidePanelExtension();
  ASSERT_TRUE(extension);

  // Open a global side panel (reading list) on the first tab.
  ShowEntryAndWait(SidePanelEntry::Key(SidePanelEntry::Id::kReadingList));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(SidePanelEntry::Id::kReadingList,
            side_panel_coordinator()->GetCurrentEntryId());

  // Open a new tab.
  OpenNewForegroundTab();
  int new_tab_id = GetCurrentTabId();

  // Register a contextual side panel in the new tab.
  RunSetOptions(*extension, new_tab_id, "panel.html", /*enabled=*/true);

  // Call `sidePanel.open()` on the current tab.
  RunOpenPanelForTab(*extension, GetCurrentTabId());

  // The contextual side panel should show on the current tab.
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelEntryShowing(
      GetKey(extension->id())));

  // Switching back to the first tab, the global side panel (reading list)
  // should be active.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelShowing());
  EXPECT_EQ(SidePanelEntry::Id::kReadingList,
            side_panel_coordinator()->GetCurrentEntryId());
}

// Tests that calling `sidePanel.open()` for a different tab will not override
// an active contextual panel.
IN_PROC_BROWSER_TEST_F(
    ExtensionOpenSidePanelBrowserTest,
    OpenSidePanel_DoesNotOverrideActiveContextualPanelIfOtherTabIdProvided) {
  // Load two side panel extensions.
  const Extension* extension1 = LoadSidePanelExtension();
  ASSERT_TRUE(extension1);
  const Extension* extension2 = LoadSidePanelExtension();
  ASSERT_TRUE(extension2);

  // Create three tabs (the initial tab + two more).
  int first_tab_id = GetCurrentTabId();
  OpenNewForegroundTab();
  OpenNewForegroundTab();
  int third_tab_id = GetCurrentTabId();

  // Register a global side panel in the first extension.
  RunSetOptions(*extension1, /*tab_id=*/absl::nullopt, "panel.html",
                /*enabled=*/true);
  // Register a contextual side panel in the second extension.
  RunSetOptions(*extension2, third_tab_id, "panel.html", /*enabled=*/true);

  SidePanelEntry::Key extension1_key = GetKey(extension1->id());
  SidePanelEntry::Key extension2_key = GetKey(extension2->id());

  // Show the contextual entry for the second extension on the active (third)
  // tab.
  ShowContextualEntryAndWait(extension2_key);
  EXPECT_TRUE(
      side_panel_coordinator()->IsSidePanelEntryShowing(extension2_key));

  // Now, run `sidePanel.open()` from the first extension. This is a global
  // panel, and shouldn't override the current tab's contextual panel.
  RunOpenPanelForTab(*extension1, first_tab_id);
  EXPECT_TRUE(
      side_panel_coordinator()->IsSidePanelEntryShowing(extension2_key));

  // However, the global panel of the first extension should be displayed in the
  // other two tabs (both the one explicitly specified and the second tab).
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(
      side_panel_coordinator()->IsSidePanelEntryShowing(extension1_key));
  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(
      side_panel_coordinator()->IsSidePanelEntryShowing(extension1_key));
}

// Tests that calling `sidePanel.open()` will override an open contextual panel
// in an inactive tab if the tab ID provided matches.
IN_PROC_BROWSER_TEST_F(
    ExtensionOpenSidePanelBrowserTest,
    OpenSidePanel_OverridesContextualEntryInInactiveTabIfTabIdMatches) {
  // Load two side panel extensions.
  const Extension* extension1 = LoadSidePanelExtension();
  ASSERT_TRUE(extension1);
  const Extension* extension2 = LoadSidePanelExtension();
  ASSERT_TRUE(extension2);

  int first_tab_id = GetCurrentTabId();
  OpenNewForegroundTab();

  // Register a global side panel in the first extension.
  RunSetOptions(*extension1, /*tab_id=*/absl::nullopt, "panel.html",
                /*enabled=*/true);
  // Register a contextual side panel in the second extension.
  RunSetOptions(*extension2, first_tab_id, "panel.html", /*enabled=*/true);

  SidePanelEntry::Key extension1_key = GetKey(extension1->id());
  SidePanelEntry::Key extension2_key = GetKey(extension2->id());

  // Show the contextual entry for the second extension on the inactive (first)
  // tab. The panel shouldn't be displayed on the active tab since it's
  // contextual.
  RunOpenPanelForTab(*extension2, first_tab_id);
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());

  // Now, run `sidePanel.open()` from the first extension on the inactive
  // (first) tab. Even though this is a global side panel, in this case, it
  // *should* override the contextual panel because the tab ID was explicitly
  // specified.
  RunOpenPanelForTab(*extension1, first_tab_id);

  // As a result, the panel should be showing on the active tab (since it's
  // global)...
  EXPECT_TRUE(
      side_panel_coordinator()->IsSidePanelEntryShowing(extension1_key));

  // ... As well as on the inactive (first) tab.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(
      side_panel_coordinator()->IsSidePanelEntryShowing(extension1_key));
}

// Tests that calling `sidePanel.open()` can override an active contextual
// panel if the `tabId` of that tab is specified.
IN_PROC_BROWSER_TEST_F(ExtensionOpenSidePanelBrowserTest,
                       OpenSidePanel_OverridesActiveContextualPanelOnSameTab) {
  // Load two side panel extensions.
  const Extension* extension1 = LoadSidePanelExtension();
  ASSERT_TRUE(extension1);
  const Extension* extension2 = LoadSidePanelExtension();
  ASSERT_TRUE(extension2);

  int current_tab_id = GetCurrentTabId();

  // Register a global side panel in the first extension.
  RunSetOptions(*extension1, /*tab_id=*/absl::nullopt, "panel.html",
                /*enabled=*/true);
  // Register a contextual side panel in the second extension.
  RunSetOptions(*extension2, current_tab_id, "panel.html", /*enabled=*/true);

  SidePanelEntry::Key extension1_key = GetKey(extension1->id());
  SidePanelEntry::Key extension2_key = GetKey(extension2->id());

  // Show the contextual entry for the second extension on the active (third)
  // tab.
  ShowContextualEntryAndWait(extension2_key);
  EXPECT_TRUE(
      side_panel_coordinator()->IsSidePanelEntryShowing(extension2_key));

  // Now, run `sidePanel.open()` from the first extension. This should override
  // the current (contextual) panel since the active tab ID was provided.
  RunOpenPanelForTab(*extension1, current_tab_id);
  EXPECT_TRUE(
      side_panel_coordinator()->IsSidePanelEntryShowing(extension1_key));
}

// Tests that calling `sidePanel.open()` on an inactive tab with a contextual
// side panel sets that panel as the active entry for that tab, but does not
// open the side panel in the active tab.
IN_PROC_BROWSER_TEST_F(ExtensionOpenSidePanelBrowserTest,
                       OpenSidePanel_OpenContextualPanelInInactiveTab) {
  const Extension* extension = LoadSidePanelExtension();
  ASSERT_TRUE(extension);

  int first_tab_id = GetCurrentTabId();

  // Open a new tab.
  OpenNewForegroundTab();

  // Register a contextual side panel in the first tab.
  RunSetOptions(*extension, first_tab_id, "panel.html", true);

  // Call `sidePanel.open()` on the first tab.
  RunOpenPanelForTab(*extension, first_tab_id);

  // The contextual side panel should not show on the current tab.
  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());

  // Switch to the first tab; the contextual panel should be shown.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelEntryShowing(
      GetKey(extension->id())));
}

// Tests calling `sidePanel.open()` with a given window ID will open the
// side panel in that window when there is no active side panel.
IN_PROC_BROWSER_TEST_F(ExtensionOpenSidePanelBrowserTest,
                       OpenSidePanel_WindowId_OpenWithNoActivePanel) {
  const Extension* extension = LoadSidePanelExtension();
  ASSERT_TRUE(extension);
  // Register a global side panel.
  RunSetOptions(*extension, /*tab_id=*/absl::nullopt, "panel.html",
                /*enabled=*/true);

  EXPECT_FALSE(side_panel_coordinator()->IsSidePanelShowing());
  // Run `sidePanel.open()`. The panel should open.
  RunOpenPanelForWindow(*extension, GetCurrentWindowId());
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelEntryShowing(
      GetKey(extension->id())));
}

// Tests calling `sidePanel.open()` with a given window ID will override an
// active global side panel in that window.
IN_PROC_BROWSER_TEST_F(ExtensionOpenSidePanelBrowserTest,
                       OpenSidePanel_WindowId_OverridesActiveGlobalPanel) {
  const Extension* extension = LoadSidePanelExtension();
  ASSERT_TRUE(extension);
  // Register a global side panel.
  RunSetOptions(*extension, /*tab_id=*/absl::nullopt, "panel.html",
                /*enabled=*/true);

  // Open a different global side panel (reading list).
  ShowEntryAndWait(SidePanelEntry::Key(SidePanelEntry::Id::kReadingList));
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntry::Id::kReadingList)));

  // Call `sidePanel.open()` on the current tab.
  RunOpenPanelForWindow(*extension, GetCurrentWindowId());

  // The extension side panel should be able to override the currently-open
  // side panel.
  EXPECT_TRUE(side_panel_coordinator()->IsSidePanelEntryShowing(
      GetKey(extension->id())));
}

// Tests calling `sidePanel.open()` with a given window ID will not override an
// active contextual panel.
IN_PROC_BROWSER_TEST_F(
    ExtensionOpenSidePanelBrowserTest,
    OpenSidePanel_WindowId_DoesNotOverrideActiveContextualPanel) {
  // Load two side panel extensions.
  const Extension* extension1 = LoadSidePanelExtension();
  ASSERT_TRUE(extension1);
  const Extension* extension2 = LoadSidePanelExtension();
  ASSERT_TRUE(extension2);

  OpenNewForegroundTab();
  int second_tab_id = GetCurrentTabId();

  // Register a global side panel in the first extension.
  RunSetOptions(*extension1, /*tab_id=*/absl::nullopt, "panel.html",
                /*enabled=*/true);
  // Register a contextual side panel in the second extension.
  RunSetOptions(*extension2, second_tab_id, "panel.html", /*enabled=*/true);

  SidePanelEntry::Key extension1_key = GetKey(extension1->id());
  SidePanelEntry::Key extension2_key = GetKey(extension2->id());

  // Show the contextual entry for the second extension on the active tab.
  ShowContextualEntryAndWait(extension2_key);
  EXPECT_TRUE(
      side_panel_coordinator()->IsSidePanelEntryShowing(extension2_key));

  // Now, run `sidePanel.open()` from the first extension. This is a global
  // panel, and shouldn't override the current tab's contextual panel.
  RunOpenPanelForWindow(*extension1, GetCurrentWindowId());
  EXPECT_TRUE(
      side_panel_coordinator()->IsSidePanelEntryShowing(extension2_key));

  // However, the global panel of the first extension should be displayed in
  // the other tab.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(
      side_panel_coordinator()->IsSidePanelEntryShowing(extension1_key));
}

// Tests calling `sidePanel.open()` with a given window ID will not override an
// inactive contextual panel.
IN_PROC_BROWSER_TEST_F(
    ExtensionOpenSidePanelBrowserTest,
    OpenSidePanel_WindowId_DoesNotOverrideInactiveContextualPanel) {
  // Load two side panel extensions.
  const Extension* extension1 = LoadSidePanelExtension();
  ASSERT_TRUE(extension1);
  const Extension* extension2 = LoadSidePanelExtension();
  ASSERT_TRUE(extension2);

  int first_tab_id = GetCurrentTabId();
  OpenNewForegroundTab();

  // Register a global side panel in the first extension.
  RunSetOptions(*extension1, /*tab_id=*/absl::nullopt, "panel.html",
                /*enabled=*/true);
  // Register a contextual side panel in the second extension.
  RunSetOptions(*extension2, first_tab_id, "panel.html", /*enabled=*/true);

  SidePanelEntry::Key extension1_key = GetKey(extension1->id());
  SidePanelEntry::Key extension2_key = GetKey(extension2->id());

  // Show the contextual entry for the second extension on the inactive tab.
  RunOpenPanelForTab(*extension2, first_tab_id);

  // Now, run `sidePanel.open()` from the first extension. This is a global
  // panel, and shouldn't override the inactive tab's contextual panel, but
  // it should be displayed in the active tab.
  RunOpenPanelForWindow(*extension1, GetCurrentWindowId());
  EXPECT_TRUE(
      side_panel_coordinator()->IsSidePanelEntryShowing(extension1_key));

  // The first tab should still show the contextual panel.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(
      side_panel_coordinator()->IsSidePanelEntryShowing(extension2_key));
}

// TODO(crbug.com/1378048): Add a test here which requires a browser in
// ExtensionViewHost for both global and contextual extension entries. One
// example of this is having a link in the page that the user can open in a new
// tab.

class ExtensionSidePanelDisabledBrowserTest : public ExtensionBrowserTest {
 public:
  ExtensionSidePanelDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(
        extensions_features::kExtensionSidePanelIntegration);
  }

 protected:
  SidePanelRegistry* global_registry() {
    return SidePanelCoordinator::GetGlobalSidePanelRegistry(browser());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that an extension's SidePanelEntry is not registered if the
// `kExtensionSidePanelIntegration` feature flag is not enabled.
IN_PROC_BROWSER_TEST_F(ExtensionSidePanelDisabledBrowserTest,
                       NoSidePanelEntry) {
  // Load an extension and verify that it does not have a registered
  // SidePanelEntry as the feature is disabled.
  scoped_refptr<const extensions::Extension> extension = LoadExtension(
      test_data_dir_.AppendASCII("api_test/side_panel/simple_default"));
  ASSERT_TRUE(extension);
  SidePanelEntry::Key extension_key = GetKey(extension->id());

  EXPECT_FALSE(global_registry()->GetEntryForKey(extension_key));
}

}  // namespace
}  // namespace extensions
