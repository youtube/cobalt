// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeLayerImpl, PrintPreviewDestinationDialogCrosElement, State} from 'chrome://print/print_preview.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
import {NativeLayerStub} from './native_layer_stub.js';
import {setupTestListenerElement} from './print_preview_test_utils.js';

const destination_dialog_cros_interactive_test = {
  suiteName: 'DestinationDialogCrosInteractiveTest',
  TestNames: {
    FocusSearchBox: 'focus search box',
    EscapeSearchBox: 'escape search box',
  },
};

Object.assign(window, {
  destination_dialog_cros_interactive_test:
      destination_dialog_cros_interactive_test,
});

suite(destination_dialog_cros_interactive_test.suiteName, function() {
  let dialog: PrintPreviewDestinationDialogCrosElement;

  let nativeLayer: NativeLayerStub;

  suiteSetup(function() {
    setupTestListenerElement();
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Create destinations.
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    setNativeLayerCrosInstance();

    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    // Create destination settings, so  that the user manager is created.
    const destinationSettings =
        document.createElement('print-preview-destination-settings');
    destinationSettings.settings = model.settings;
    destinationSettings.state = State.READY;
    destinationSettings.disabled = false;
    fakeDataBind(model, destinationSettings, 'settings');
    document.body.appendChild(destinationSettings);

    // Initialize
    destinationSettings.init(
        'FooDevice' /* printerName */, false /* pdfPrinterDisabled */,
        true /* isDriveMounted */,
        '' /* serializedDefaultDestinationSelectionRulesStr */);
    return nativeLayer.whenCalled('getPrinterCapabilities').then(() => {
      // Retrieve a reference to dialog
      dialog = destinationSettings.$.destinationDialog.get();
    });
  });

  // Tests that the search input text field is automatically focused when the
  // dialog is shown.
  test(
      destination_dialog_cros_interactive_test.TestNames.FocusSearchBox,
      function() {
        const searchInput = dialog.$.searchBox.getSearchInput();
        assertTrue(!!searchInput);
        const whenFocusDone = eventToPromise('focus', searchInput);
        dialog.destinationStore.startLoadAllDestinations();
        dialog.show();
        return whenFocusDone;
      });

  // Tests that pressing the escape key while the search box is focused
  // closes the dialog if and only if the query is empty.
  test(
      destination_dialog_cros_interactive_test.TestNames.EscapeSearchBox,
      function() {
        const searchBox = dialog.$.searchBox;
        const searchInput = searchBox.getSearchInput();
        assertTrue(!!searchInput);
        const whenFocusDone = eventToPromise('focus', searchInput);
        dialog.destinationStore.startLoadAllDestinations();
        dialog.show();
        return whenFocusDone
            .then(() => {
              assertTrue(dialog.$.dialog.open);

              // Put something in the search box.
              const whenSearchChanged =
                  eventToPromise('search-changed', searchBox);
              searchBox.setValue('query');
              return whenSearchChanged;
            })
            .then(() => {
              assertEquals('query', searchInput.value);

              // Simulate escape
              const whenKeyDown = eventToPromise('keydown', dialog);
              keyDownOn(searchInput, 19, [], 'Escape');
              return whenKeyDown;
            })
            .then(() => {
              // Dialog should still be open.
              assertTrue(dialog.$.dialog.open);

              // Clear the search box.
              const whenSearchChanged =
                  eventToPromise('search-changed', searchBox);
              searchBox.setValue('');
              return whenSearchChanged;
            })
            .then(() => {
              assertEquals('', searchInput.value);

              // Simulate escape
              const whenKeyDown = eventToPromise('keydown', dialog);
              keyDownOn(searchInput, 19, [], 'Escape');
              return whenKeyDown;
            })
            .then(() => {
              // Dialog is closed.
              assertFalse(dialog.$.dialog.open);
            });
      });
});
