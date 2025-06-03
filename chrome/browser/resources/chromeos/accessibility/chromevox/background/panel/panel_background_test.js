// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);
GEN_INCLUDE(['../../../common/testing/documents.js']);

/**
 * Test fixture for PanelBackground.
 */
ChromeVoxPanelBackgroundTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    // Alphabetical based on file path.
    await Promise.all([
      importModule(
          'PanelBackground', '/chromevox/background/panel/panel_background.js'),
      importModule(
          'PanelNodeMenuBackground',
          '/chromevox/background/panel/panel_node_menu_background.js'),
    ]);
  }
};

AX_TEST_F('ChromeVoxPanelBackgroundTest', 'OnTutorialReady', async function() {
  const callbackPromise = new Promise(
      resolve => PanelBackground.instance.tutorialReadyCallback_ = resolve);

  assertFalse(PanelBackground.instance.tutorialReadyForTesting_);

  PanelBackground.instance.onTutorialReady_();
  await callbackPromise;

  assertTrue(PanelBackground.instance.tutorialReadyForTesting_);
});

AX_TEST_F(
    'ChromeVoxPanelBackgroundTest', 'WaitForMenusLoaded', async function() {
      const root = await this.runWithLoadedTree(Documents.link);
      const link = root.find({role: 'link'});
      assertNotNullNorUndefined(link);

      // Mock out the method waitForFinish() so we can control when the menu
      // finishes loading.
      let resolve;
      const promise = new Promise(r => resolve = r);
      PanelNodeMenuBackground.prototype.waitForFinish = () => promise;

      PanelBackground.instance.createAllNodeMenuBackgrounds_();

      let menusLoaded = false;
      PanelBackground.waitForMenusLoaded().then(() => menusLoaded = true);
      assertFalse(menusLoaded);

      resolve();

      await PanelBackground.waitForMenusLoaded();

      assertTrue(menusLoaded);
    });
