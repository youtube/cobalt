// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://shortcut-customization/js/accelerator_view.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {IronIconElement} from '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {AcceleratorViewElement, ViewState} from 'chrome://shortcut-customization/js/accelerator_view.js';
import {fakeAcceleratorConfig, fakeLayoutInfo} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutProvider} from 'chrome://shortcut-customization/js/fake_shortcut_provider.js';
import {InputKeyElement, KeyInputState} from 'chrome://shortcut-customization/js/input_key.js';
import {setShortcutProviderForTesting} from 'chrome://shortcut-customization/js/mojo_interface_provider.js';
import {AcceleratorConfigResult, AcceleratorSource, LayoutStyle, Modifier} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {AcceleratorResultData} from 'chrome://shortcut-customization/mojom-webui/ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createStandardAcceleratorInfo, createUserAcceleratorInfo} from './shortcut_customization_test_util.js';

export function initAcceleratorViewElement(): AcceleratorViewElement {
  const element = document.createElement('accelerator-view');
  // Set default acceleratorInfo and viewState
  element.acceleratorInfo = createUserAcceleratorInfo(
      Modifier.CONTROL | Modifier.SHIFT,
      /*key=*/ 71,
      /*keyDisplay=*/ 'g');
  element.viewState = ViewState.VIEW;
  document.body.appendChild(element);
  flush();
  return element;
}

suite('acceleratorViewTest', function() {
  let viewElement: AcceleratorViewElement|null = null;

  let manager: AcceleratorLookupManager|null = null;
  let provider: FakeShortcutProvider;

  setup(() => {
    provider = new FakeShortcutProvider();
    setShortcutProviderForTesting(provider);

    manager = AcceleratorLookupManager.getInstance();
    manager.setAcceleratorLookup(fakeAcceleratorConfig);
    manager.setAcceleratorLayoutLookup(fakeLayoutInfo);
  });

  teardown(() => {
    if (manager) {
      manager.reset();
    }

    if (viewElement) {
      viewElement.remove();
    }
    viewElement = null;
  });

  function getInputKey(selector: string): InputKeyElement {
    const element = viewElement!.shadowRoot!.querySelector(selector);
    assertTrue(!!element);
    return element as InputKeyElement;
  }

  function getLockIcon(): HTMLDivElement {
    return strictQuery(
        '.lock-icon-container', viewElement!.shadowRoot, HTMLDivElement);
  }

  function getEditIcon(): HTMLDivElement {
    return strictQuery(
        '.edit-icon-container', viewElement!.shadowRoot, HTMLDivElement);
  }

  test('LoadsBasicAccelerator', async () => {
    viewElement = initAcceleratorViewElement();
    await flushTasks();

    const keys = viewElement.shadowRoot!.querySelectorAll('input-key');
    // Three keys: shift, control, g
    assertEquals(3, keys.length);

    assertEquals(
        'ctrl',
        keys[0]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'shift',
        keys[1]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'g', keys[2]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
  });

  test('EditableAccelerator', async () => {
    viewElement = initAcceleratorViewElement();
    await flushTasks();

    viewElement.source = AcceleratorSource.kAsh;
    viewElement.action = 1;
    await flush();
    // Enable the edit view.
    viewElement.viewState = ViewState.EDIT;

    await flush();

    let ctrlKey = getInputKey('#ctrlKey');
    let altKey = getInputKey('#altKey');
    let shiftKey = getInputKey('#shiftKey');
    let metaKey = getInputKey('#searchKey');
    let pendingKey = getInputKey('#pendingKey');

    // By default, no keys should be registered.
    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, pendingKey.keyState);
    assertEquals('key', pendingKey.key);

    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflict,
      shortcutName: {data: [1]},
    };

    provider.setFakeReplaceAcceleratorResult(fakeResult);

    // Simulate Ctrl.
    viewElement.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Control',
      keyCode: 17,
      code: 'Control',
      ctrlKey: true,
      altKey: false,
      shiftKey: false,
      metaKey: false,
    }));

    await flush();

    assertEquals(KeyInputState.MODIFIER_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, pendingKey.keyState);

    // Release Ctrl, expect it to not be selected.
    viewElement.dispatchEvent(new KeyboardEvent('keyup', {
      key: 'Control',
      keyCode: 17,
      code: 'Control',
      ctrlKey: true,
      altKey: false,
      shiftKey: false,
      metaKey: false,
    }));

    await flush();
    ctrlKey = getInputKey('#ctrlKey');
    altKey = getInputKey('#altKey');
    shiftKey = getInputKey('#shiftKey');
    metaKey = getInputKey('#searchKey');
    pendingKey = getInputKey('#pendingKey');

    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, pendingKey.keyState);

    viewElement.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'e',
      keyCode: 69,
      code: 'KeyE',
      ctrlKey: false,
      altKey: false,
      shiftKey: false,
      metaKey: false,
    }));
    await flush();
    pendingKey = getInputKey('#pendingKey');

    assertEquals(KeyInputState.ALPHANUMERIC_SELECTED, pendingKey.keyState);
    assertEquals('e', pendingKey.key);

    // Release `e`, expect it to not be selected.
    viewElement.dispatchEvent(new KeyboardEvent('keyup', {
      key: '',
      keyCode: 69,
      code: 'KeyE',
      ctrlKey: false,
      altKey: false,
      shiftKey: false,
      metaKey: false,
    }));

    await flush();
    ctrlKey = getInputKey('#ctrlKey');
    altKey = getInputKey('#altKey');
    shiftKey = getInputKey('#shiftKey');
    metaKey = getInputKey('#searchKey');
    pendingKey = getInputKey('#pendingKey');

    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, pendingKey.keyState);
    assertEquals('key', pendingKey.key);
  });

  test('EditWithFunctionKeyAsOnlyKey', async () => {
    viewElement = initAcceleratorViewElement();
    await flushTasks();

    viewElement.source = AcceleratorSource.kAsh;
    viewElement.action = 1;
    await flushTasks();
    // Enable the edit view.
    viewElement.viewState = ViewState.EDIT;

    await flushTasks();

    const ctrlKey = getInputKey('#ctrlKey');
    const altKey = getInputKey('#altKey');
    const shiftKey = getInputKey('#shiftKey');
    const metaKey = getInputKey('#searchKey');
    const pendingKey = getInputKey('#pendingKey');

    // By default, no keys should be registered.
    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, pendingKey.keyState);
    assertEquals('key', pendingKey.key);

    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflict,
      shortcutName: {data: [1]},
    };

    provider.setFakeReplaceAcceleratorResult(fakeResult);

    // Simulate F3.
    viewElement.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'F3',
      keyCode: 114,
      code: 'F3',
      ctrlKey: false,
      altKey: false,
      shiftKey: false,
      metaKey: false,
    }));

    await flush();

    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);
    assertEquals(KeyInputState.ALPHANUMERIC_SELECTED, pendingKey.keyState);
    assertEquals('f3', pendingKey.key);
  });

  test('LockIconVisibilityBasedOnProperties', async () => {
    viewElement = initAcceleratorViewElement();
    const scenarios = [
      {customizationEnabled: true, locked: true, sourceIsLocked: true},
      {customizationEnabled: true, locked: true, sourceIsLocked: false},
      {customizationEnabled: true, locked: false, sourceIsLocked: true},
      {customizationEnabled: true, locked: false, sourceIsLocked: false},
      {customizationEnabled: false, locked: true, sourceIsLocked: true},
      {customizationEnabled: false, locked: true, sourceIsLocked: false},
      {customizationEnabled: false, locked: false, sourceIsLocked: true},
      {customizationEnabled: false, locked: false, sourceIsLocked: false},
    ];

    // Prepare all test cases by looping the fakeLayoutInfo.
    const testCases = [];
    for (const layoutInfo of fakeLayoutInfo) {
      // If it's text accelerator, break the loop early.
      if (layoutInfo.style !== LayoutStyle.kDefault) {
        continue;
      }
      for (const scenario of scenarios) {
        // replicate getCategory() logic.
        const category = manager!.getAcceleratorCategory(
            layoutInfo.source, layoutInfo.action);
        const categoryIsLocked = manager!.isCategoryLocked(category);
        // replicate shouldShowLockIcon() logic.
        const expectLockIconVisible = scenario.customizationEnabled &&
            !categoryIsLocked && (scenario.locked || scenario.sourceIsLocked);
        testCases.push({
          ...scenario,
          layoutInfo: layoutInfo,
          categoryIsLocked: categoryIsLocked,
          expectLockIconVisible: expectLockIconVisible,
        });
      }
    }
    // Verify lock icon show/hide based on properties.
    for (const testCase of testCases) {
      loadTimeData.overrideValues(
          {isCustomizationAllowed: testCase.customizationEnabled});
      viewElement.source = testCase.layoutInfo.source;
      viewElement.action = testCase.layoutInfo.action;
      viewElement.categoryIsLocked = testCase.categoryIsLocked;
      const acceleratorInfo = createStandardAcceleratorInfo(
          Modifier.CONTROL | Modifier.SHIFT,
          /*key=*/ 71,
          /*keyDisplay=*/ 'g');
      viewElement.acceleratorInfo = acceleratorInfo;
      viewElement.set('acceleratorInfo.locked', testCase.locked);
      viewElement.sourceIsLocked = testCase.sourceIsLocked;

      await flush();
      assertEquals(testCase.expectLockIconVisible, isVisible(getLockIcon()));
    }
  });

  test('EditIconVisibilityBasedOnProperties', async () => {
    viewElement = initAcceleratorViewElement();
    // Mainly test on customizationEnabled and accelerator is not locked.
    const scenarios = [
      {
        customizationEnabled: true,
        locked: false,
        sourceIsLocked: false,
        isAcceleratorRow: false,
        isFirstAccelerator: true,
      },
      {
        customizationEnabled: true,
        locked: false,
        sourceIsLocked: false,
        isAcceleratorRow: true,
        isFirstAccelerator: true,
      },
      {
        customizationEnabled: true,
        locked: true,
        sourceIsLocked: false,
        isAcceleratorRow: false,
        isFirstAccelerator: true,
      },
      {
        customizationEnabled: true,
        locked: false,
        sourceIsLocked: true,
        isAcceleratorRow: true,
        isFirstAccelerator: false,
      },
      {
        customizationEnabled: false,
        locked: false,
        sourceIsLocked: false,
        isAcceleratorRow: false,
        isFirstAccelerator: true,
      },
    ];

    // Prepare all test cases by looping the fakeLayoutInfo.
    const testCases = [];
    for (const layoutInfo of fakeLayoutInfo) {
      // If it's text accelerator, break the loop early.
      if (layoutInfo.style !== LayoutStyle.kDefault) {
        continue;
      }
      for (const scenario of scenarios) {
        // replicate getCategory() logic.
        const category = manager!.getAcceleratorCategory(
            layoutInfo.source, layoutInfo.action);
        const categoryIsLocked = manager!.isCategoryLocked(category);
        // replicate shouldShowLockIcon() logic.
        const expectEditIconVisible = scenario.customizationEnabled &&
            scenario.isAcceleratorRow && !categoryIsLocked &&
            !scenario.locked && !scenario.sourceIsLocked &&
            scenario.isFirstAccelerator;
        testCases.push({
          ...scenario,
          layoutInfo: layoutInfo,
          categoryIsLocked: categoryIsLocked,
          expectEditIconVisible: expectEditIconVisible,
        });
      }
    }
    for (const testCase of testCases) {
      loadTimeData.overrideValues(
          {isCustomizationAllowed: testCase.customizationEnabled});
      viewElement.source = testCase.layoutInfo.source;
      viewElement.action = testCase.layoutInfo.action;
      viewElement.categoryIsLocked = testCase.categoryIsLocked;
      viewElement.showEditIcon = testCase.isAcceleratorRow;
      viewElement.isFirstAccelerator = testCase.isFirstAccelerator;
      const acceleratorInfo = createStandardAcceleratorInfo(
          Modifier.CONTROL | Modifier.SHIFT,
          /*key=*/ 71,
          /*keyDisplay=*/ 'g');
      viewElement.acceleratorInfo = acceleratorInfo;
      viewElement.set('acceleratorInfo.locked', testCase.locked);
      viewElement.sourceIsLocked = testCase.sourceIsLocked;

      await flush();
      assertEquals(
          testCase.expectEditIconVisible,
          !getEditIcon().hasAttribute('hidden'));
    }
  });

  test('KeyDisplayAndIconDuringEdit', async () => {
    viewElement = initAcceleratorViewElement();
    await flushTasks();
    viewElement.source = AcceleratorSource.kAsh;
    viewElement.action = 1;
    await flush();

    // Enable the edit view.
    viewElement.viewState = ViewState.EDIT;
    await flush();

    const pendingKey = getInputKey('#pendingKey');

    const fakeResult: AcceleratorResultData = {
      result: AcceleratorConfigResult.kConflict,
      shortcutName: {data: [1]},
    };
    provider.setFakeReplaceAcceleratorResult(fakeResult);

    // Simulate SHIFT + SPACE, expect the key display to be 'space'.
    viewElement.dispatchEvent(new KeyboardEvent('keydown', {
      key: ' ',
      code: 'Space',
      keyCode: 32,
      shiftKey: true,
    }));

    await flush();
    assertEquals('space', pendingKey.key);

    // Simulate SHIFT + OVERVIEW, expect the key display to be
    // 'LaunchApplication1' and the icon to be 'overview'.
    viewElement.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'F4',
      code: 'ShowAllWindows',
      keyCode: 182,
      shiftKey: true,
    }));
    await flush();

    assertEquals('LaunchApplication1', pendingKey.key);
    const keyIconElement =
        pendingKey.shadowRoot!.querySelector('#key-icon') as IronIconElement;
    assertEquals('shortcut-customization-keys:overview', keyIconElement.icon);

    // Simulate SHIFT + BRIGHTNESS_UP, expect the key display to be
    // 'BrightnessUp' and the icon to be 'display-brightness-up'.
    viewElement.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'BrightnessUp',
      code: 'BrightnessUp',
      keyCode: 217,
      shiftKey: true,
    }));
    await flush();

    assertEquals('BrightnessUp', pendingKey.key);
    const keyIconElement2 =
        pendingKey.shadowRoot!.querySelector('#key-icon') as IronIconElement;
    assertEquals(
        'shortcut-customization-keys:display-brightness-up',
        keyIconElement2.icon);

    // Simulate SHIFT + MUTE_MICROPHONE.
    viewElement.dispatchEvent(new KeyboardEvent('keydown', {
      key: '',
      code: '',
      keyCode: 159,
      shiftKey: true,
    }));
    await flush();

    assertEquals('MicrophoneMuteToggle', pendingKey.key);
    const keyIconElement3 =
        pendingKey.shadowRoot!.querySelector('#key-icon') as IronIconElement;
    assertEquals(
        'shortcut-customization-keys:microphone-mute', keyIconElement3.icon);

    // Simulate CONTROL + BACKQUOTE.
    viewElement.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Unidentified',
      code: 'Backquote',
      keyCode: 192,
      ctrlKey: true,
    }));
    await flush();

    assertEquals('`', pendingKey.key);
  });

  test('GetAriaLabels', async () => {
    viewElement = initAcceleratorViewElement();
    await flushTasks();

    const acceleratorInfo = createStandardAcceleratorInfo(
        Modifier.SHIFT | Modifier.ALT,
        /*key=*/ 221,
        /*keyDisplay=*/ 's');
    viewElement.acceleratorInfo = acceleratorInfo;
    viewElement.source = AcceleratorSource.kAsh;
    viewElement.action = 1;
    viewElement.viewState = ViewState.VIEW;
    await flush();

    let viewContainer =
        strictQuery('#container', viewElement.shadowRoot, HTMLDivElement);
    assertEquals('alt shift s', viewContainer.ariaLabel);

    // Aria label is empty during editing process.
    viewElement.viewState = ViewState.EDIT;
    await flush();

    viewContainer =
        strictQuery('#container', viewElement.shadowRoot, HTMLDivElement);
    assertEquals('', viewContainer.ariaLabel);
  });

  test('GetAriaLabelsWithIcon', async () => {
    viewElement = initAcceleratorViewElement();
    await flushTasks();

    const acceleratorInfo = createStandardAcceleratorInfo(
        Modifier.SHIFT | Modifier.ALT | Modifier.COMMAND,
        /*key=*/ 220,
        /*keyDisplay=*/ 'LaunchApplication1');
    viewElement.acceleratorInfo = acceleratorInfo;
    viewElement.source = AcceleratorSource.kAsh;
    viewElement.action = 1;
    await flush();

    const viewContainer =
        viewElement.shadowRoot!.querySelector('#container') as HTMLDivElement;
    // The icon name is 'overview' in keyToIconNameMap.
    const regex = /^(search|launcher) alt shift overview$/;
    assertTrue(!!viewContainer.ariaLabel);
    assertTrue(regex.test(viewContainer.ariaLabel));
  });

  test('GetAriaLabelsWithLwinKey', async () => {
    viewElement = initAcceleratorViewElement();
    await flushTasks();
    // Open/close launcher -> Lwin key.
    const acceleratorInfo = createStandardAcceleratorInfo(
        Modifier.NONE,
        /*key=*/ 224,
        /*keyDisplay=*/ 'Meta');
    viewElement.acceleratorInfo = acceleratorInfo;
    viewElement.source = AcceleratorSource.kAsh;
    viewElement.action = 1;
    await flush();

    const viewContainer =
        viewElement.shadowRoot!.querySelector('#container') as HTMLDivElement;
    const regex = /^(search|launcher)$/;
    assertTrue(!!viewContainer.ariaLabel);
    assertTrue(regex.test(viewContainer.ariaLabel));
  });

  test('CancelInputWithShortcut', async () => {
    viewElement = initAcceleratorViewElement();
    await flushTasks();

    viewElement.source = AcceleratorSource.kAsh;
    viewElement.action = 1;
    // Enable the edit view.
    viewElement.viewState = ViewState.EDIT;

    await flush();

    // Assert that this is in the EDIT state.
    assertEquals(ViewState.EDIT, viewElement.viewState);

    let ctrlKey = getInputKey('#ctrlKey');
    let altKey = getInputKey('#altKey');
    let shiftKey = getInputKey('#shiftKey');
    let metaKey = getInputKey('#searchKey');
    let pendingKey = getInputKey('#pendingKey');

    // By default, no keys should be registered.
    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, pendingKey.keyState);
    assertEquals('key', pendingKey.key);

    // Simulate Alt.
    viewElement.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Alt',
      keyCode: 18,
      code: 'Alt',
      ctrlKey: false,
      altKey: true,
      shiftKey: false,
      metaKey: false,
    }));

    await flush();

    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.MODIFIER_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, pendingKey.keyState);

    // Now press Escape.
    viewElement.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Escape',
      keyCode: 27,
      code: 'Escape',
      ctrlKey: false,
      altKey: true,
      shiftKey: false,
      metaKey: false,
    }));

    await flush();
    ctrlKey = getInputKey('#ctrlKey');
    altKey = getInputKey('#altKey');
    shiftKey = getInputKey('#shiftKey');
    metaKey = getInputKey('#searchKey');
    pendingKey = getInputKey('#pendingKey');

    assertEquals(KeyInputState.NOT_SELECTED, ctrlKey.keyState);
    assertEquals(KeyInputState.MODIFIER_SELECTED, altKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, shiftKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, metaKey.keyState);
    assertEquals(KeyInputState.NOT_SELECTED, pendingKey.keyState);

    // Expect that press Alt + Esc will cancel the edit state.
    assertEquals(ViewState.VIEW, viewElement.viewState);
    assertFalse(viewElement.hasError);
    assertEquals('', viewElement.statusMessage);
  });
});
