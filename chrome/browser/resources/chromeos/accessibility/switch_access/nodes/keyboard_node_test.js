// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../switch_access_e2e_test_base.js']);

/** Test fixture for the keyboard node. */
SwitchAccessKeyboardNodeTest = class extends SwitchAccessE2ETest {
  async setUpDeferred() {
    await super.setUpDeferred();
    globalThis.RoleType = chrome.automation.RoleType;
  }
};

TEST_F('SwitchAccessKeyboardNodeTest', 'GetKeyboardObjectAndPredicate', function() {
  this.runWithLoadedDesktop(() => {
    // Save original desktopNode.
    const originalDesktop = Navigator.byItem.desktopNode;

    function createMockNode(properties) {
      const node = {
        role: null,
        root: null,
        firstChild: null,
        nextSibling: null,
        matches: function(params) {
          for (const key in params) {
            if (this[key] !== params[key]) {
              return false;
            }
          }
          return true;
        },
        ...properties
      };
      return node;
    }

    const desktop = createMockNode({ role: RoleType.DESKTOP });
    desktop.root = desktop;

    const webArea = createMockNode({ role: RoleType.ROOT_WEB_AREA });
    webArea.root = webArea;

    const webKeyboard = createMockNode({ role: RoleType.KEYBOARD });
    webKeyboard.root = webArea;

    const systemKeyboard = createMockNode({ role: RoleType.KEYBOARD });
    systemKeyboard.root = desktop;

    // Scenario 1: Both web keyboard and system keyboard are present.
    // Web keyboard is ahead in pre-order traversal.
    desktop.firstChild = webArea;
    webArea.nextSibling = systemKeyboard;
    webArea.firstChild = webKeyboard;

    Object.defineProperty(Navigator.byItem, 'desktopNode', {
      get: () => desktop,
      configurable: true,
    });
    // Clear cached keyboard object.
    KeyboardRootNode.object_ = undefined;

    try {
      let keyboard = KeyboardRootNode.getKeyboardObject();
      assertEquals(systemKeyboard, keyboard, 'Should resolve the system keyboard');

      // Scenario 2: Only web keyboard is present.
      webArea.nextSibling = null;
      KeyboardRootNode.object_ = undefined;

      keyboard = KeyboardRootNode.getKeyboardObject();
      assertEquals(undefined, keyboard, 'Should not resolve the web keyboard');

      // Test the BasicRootNode builder predicate.
      const keyboardBuilder = BasicRootNode.builders.find(
          b => b.builder === KeyboardRootNode.buildTree);
      assertNotEquals(undefined, keyboardBuilder, 'Keyboard builder should exist');
      const predicate = keyboardBuilder.predicate;

      assertTrue(predicate(systemKeyboard), 'Predicate should accept system keyboard');
      assertFalse(predicate(webKeyboard), 'Predicate should reject web keyboard');
    } finally {
      // Restore original desktopNode.
      delete Navigator.byItem.desktopNode;
      KeyboardRootNode.object_ = undefined;
    }
  });
});
