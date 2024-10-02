// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationStore, GooglePromotedDestinationId, LocalDestinationInfo, makeRecentDestination, NativeLayerImpl,
        // <if expr="is_chromeos">
        PrintPreviewDestinationDialogCrosElement,
        // </if>
        // <if expr="not is_chromeos">
        PrintPreviewDestinationDialogElement,
        // </if>
        PrintPreviewDestinationListItemElement} from 'chrome://print/print_preview.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

// <if expr="is_chromeos">
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
// </if>

import {NativeLayerStub} from './native_layer_stub.js';
import {createDestinationStore, getDestinations, getExtensionDestinations, setupTestListenerElement} from './print_preview_test_utils.js';

const destination_dialog_test = {
  suiteName: 'DestinationDialogTest',
  TestNames: {
    PrinterList: 'PrinterList',
    PrinterListPreloaded: 'PrinterListPreloaded',
  },
};

Object.assign(window, {destination_dialog_test: destination_dialog_test});

suite(destination_dialog_test.suiteName, function() {
  // <if expr="is_chromeos">
  let dialog: PrintPreviewDestinationDialogCrosElement;
  // </if>
  // <if expr="not is_chromeos">
  let dialog: PrintPreviewDestinationDialogElement;
  // </if>

  let destinationStore: DestinationStore;

  let nativeLayer: NativeLayerStub;

  let destinations: Destination[] = [];

  let extensionDestinations: Destination[] = [];

  const localDestinations: LocalDestinationInfo[] = [];

  suiteSetup(function() {
    setupTestListenerElement();
  });

  setup(function() {
    // Create data classes
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    // <if expr="is_chromeos">
    setNativeLayerCrosInstance();
    // </if>
    destinationStore = createDestinationStore();
    destinations = getDestinations(localDestinations);
    const extensionDestinationConfig = getExtensionDestinations();
    nativeLayer.setLocalDestinations(localDestinations);
    nativeLayer.setExtensionDestinations(extensionDestinationConfig.infoLists);
    extensionDestinations = extensionDestinationConfig.destinations;
  });

  function finishSetup() {
    // Set up dialog
    // <if expr="is_chromeos">
    dialog = document.createElement('print-preview-destination-dialog-cros');
    // </if>
    // <if expr="not is_chromeos">
    dialog = document.createElement('print-preview-destination-dialog');
    // </if>
    dialog.destinationStore = destinationStore;
    document.body.appendChild(dialog);
    destinationStore.startLoadAllDestinations();
    dialog.show();
  }

  function validatePrinterList() {
    const list =
        dialog.shadowRoot!.querySelector('print-preview-destination-list');
    const printerItems = list!.shadowRoot!.querySelectorAll(
        'print-preview-destination-list-item');
    const getDisplayedName = (item: PrintPreviewDestinationListItemElement) =>
        item.shadowRoot!.querySelector('.name')!.textContent;
    // 5 local printers + 3 extension printers + Save as PDF
    assertEquals(9, printerItems.length);
    // Save as PDF shows up first.
    assertEquals(
        GooglePromotedDestinationId.SAVE_AS_PDF,
        getDisplayedName(printerItems[0]!));
    Array.from(printerItems).slice(1, 5).forEach((item, index) => {
      assertEquals(destinations[index]!.displayName, getDisplayedName(item));
    });
    assertEquals('FooName', getDisplayedName(printerItems[5]!));
    Array.from(printerItems).slice(6, 9).forEach((item, index) => {
      assertEquals(
          extensionDestinations[index]!.displayName, getDisplayedName(item));
    });
  }

  // Test that destinations are correctly displayed in the lists.
  test(destination_dialog_test.TestNames.PrinterList, async () => {
    // Native printers are fetched at startup, since the recent printer is set
    // as native.
    let whenPrinterListReady = nativeLayer.waitForGetPrinters(1);
    destinationStore.init(
        false /* pdfPrinterDisabled */, true /* isDriveMounted */,
        'FooDevice' /* printerName */,
        '' /* serializedDefaultDestinationSelectionRulesStr */,
        [makeRecentDestination(destinations[4]!)] /* recentDestinations */);
    await whenPrinterListReady;
    whenPrinterListReady = nativeLayer.waitForGetPrinters(1);
    // This should trigger 1 new getPrinters() call, for extension printers.
    finishSetup();
    await whenPrinterListReady;
    flush();
    validatePrinterList();
  });

  // Test that destinations are correctly displayed in the lists when all
  // printers have been preloaded before the dialog is opened. Regression test
  // for https://crbug.com/1330678.
  test(
      destination_dialog_test.TestNames.PrinterListPreloaded, async () => {
        // All printers are fetched at startup since both native and extension
        // printers are recent.
        const whenAllPreloaded = nativeLayer.waitForGetPrinters(2);
        destinationStore.init(
            false /* pdfPrinterDisabled */, true /* isDriveMounted */,
            'FooDevice' /* printerName */,
            '' /* serializedDefaultDestinationSelectionRulesStr */, [
              makeRecentDestination(destinations[4]!),
              makeRecentDestination(extensionDestinations[0]!),
            ] /* recentDestinations */);
        await whenAllPreloaded;
        finishSetup();
        flush();
        validatePrinterList();
      });
});
