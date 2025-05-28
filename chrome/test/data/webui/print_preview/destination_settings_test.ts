// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {LocalDestinationInfo, PrintPreviewDestinationSettingsElement, RecentDestination} from 'chrome://print/print_preview.js';
import {Destination, DestinationErrorType, DestinationOrigin, DestinationState, DestinationStoreEventType, Error, GooglePromotedDestinationId, makeRecentDestination, NativeLayerImpl, NUM_PERSISTED_DESTINATIONS, State} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {fakeDataBind, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {NativeLayerStub} from './native_layer_stub.js';
import {getDestinations, getSaveAsPdfDestination, setupTestListenerElement} from './print_preview_test_utils.js';

suite('DestinationSettingsTest', function() {
  let destinationSettings: PrintPreviewDestinationSettingsElement;

  let nativeLayer: NativeLayerStub;

  let recentDestinations: RecentDestination[] = [];

  let localDestinations: LocalDestinationInfo[] = [];

  let destinations: Destination[] = [];

  const extraDestinations: Destination[] = [];

  let pdfPrinterDisabled: boolean = false;

  suiteSetup(function() {
    setupTestListenerElement();
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    // Stub out native layer.
    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.setInstance(nativeLayer);
    localDestinations = [];
    destinations = getDestinations(localDestinations);
    // Add some extra destinations.
    for (let i = 0; i < NUM_PERSISTED_DESTINATIONS; i++) {
      const id = `e${i}`;
      const name = `n${i}`;
      localDestinations.push({deviceName: id, printerName: name});
      extraDestinations.push(new Destination(id, getLocalOrigin(), name));
    }
    nativeLayer.setLocalDestinations(localDestinations);

    const model = document.createElement('print-preview-model');
    document.body.appendChild(model);

    destinationSettings =
        document.createElement('print-preview-destination-settings');
    destinationSettings.settings = model.settings;
    destinationSettings.state = State.NOT_READY;
    destinationSettings.disabled = true;
    fakeDataBind(model, destinationSettings, 'settings');
    document.body.appendChild(destinationSettings);
  });

  // Tests that the dropdown is enabled or disabled correctly based on
  // the state.
  test(
      'ChangeDropdownState', function() {
        const dropdown = destinationSettings.$.destinationSelect;
        // Initial state: No destination store means that there is no
        // destination yet.
        assertFalse(dropdown.loaded);

        // Set up the destination store, but no destination yet. Dropdown is
        // still not loaded.
        destinationSettings.init(
            'FooDevice' /* printerName */, false /* pdfPrinterDisabled */,
            '' /* serializedDefaultDestinationSelectionRulesStr */);
        assertFalse(dropdown.loaded);

        return eventToPromise(
                   DestinationStoreEventType
                       .SELECTED_DESTINATION_CAPABILITIES_READY,
                   destinationSettings.getDestinationStoreForTest())
            .then(() => {
              // The capabilities ready event results in |destinationState|
              // changing to SELECTED, which enables and shows the dropdown even
              // though |state| has not yet transitioned to READY. This is to
              // prevent brief losses of focus when the destination changes.
              assertFalse(dropdown.disabled);
              assertTrue(dropdown.loaded);
              destinationSettings.state = State.READY;
              destinationSettings.disabled = false;

              // Simulate setting a setting to an invalid value. Dropdown is
              // disabled due to validation error on another control.
              destinationSettings.state = State.ERROR;
              destinationSettings.disabled = true;
              assertTrue(dropdown.disabled);

              // Simulate the user fixing the validation error, and then
              // selecting an invalid printer. Dropdown is enabled, so that the
              // user can fix the error.
              destinationSettings.state = State.READY;
              destinationSettings.disabled = false;
              destinationSettings.getDestinationStoreForTest().dispatchEvent(
                  new CustomEvent(
                      DestinationStoreEventType.ERROR,
                      {detail: DestinationErrorType.INVALID}));
              flush();

              assertEquals(
                  DestinationState.ERROR, destinationSettings.destinationState);
              assertEquals(Error.INVALID_PRINTER, destinationSettings.error);
              destinationSettings.state = State.ERROR;
              destinationSettings.disabled = true;
              assertFalse(dropdown.disabled);

              // Simulate the user having no printers.
              destinationSettings.getDestinationStoreForTest().dispatchEvent(
                  new CustomEvent(
                      DestinationStoreEventType.ERROR,
                      {detail: DestinationErrorType.NO_DESTINATIONS}));
              flush();

              assertEquals(
                  DestinationState.ERROR, destinationSettings.destinationState);
              assertEquals(Error.NO_DESTINATIONS, destinationSettings.error);
              destinationSettings.state = State.FATAL_ERROR;
              destinationSettings.disabled = true;
              assertTrue(dropdown.disabled);
            });
      });

  function getLocalOrigin(): DestinationOrigin {
    return DestinationOrigin.LOCAL;
  }

  /**
   * Initializes the destination store and destination settings using
   * |destinations| and |recentDestinations|.
   */
  function initialize() {
    // Initialize destination settings.
    destinationSettings.setSetting('recentDestinations', recentDestinations);
    destinationSettings.init(
        '' /* printerName */, pdfPrinterDisabled,
        '' /* serializedDefaultDestinationSelectionRulesStr */);
    destinationSettings.state = State.READY;
    destinationSettings.disabled = false;
  }

  /**
   * @param id The id of the local destination.
   * @return The key corresponding to the local destination, with the
   *     origin set correctly based on the platform.
   */
  function makeLocalDestinationKey(id: string): string {
    return id + '/' + getLocalOrigin() + '/';
  }

  /**
   * @param expectedDestinations An array of the expected
   *     destinations in the dropdown.
   */
  function assertDropdownItems(expectedDestinations: string[]) {
    const options =
        destinationSettings.$.destinationSelect.getVisibleItemsForTest();
    assertEquals(expectedDestinations.length + 1, options.length);
    expectedDestinations.forEach((expectedValue, index) => {
      assertEquals(expectedValue, options[index]!.value);
    });
    assertEquals('seeMore', options[expectedDestinations.length]!.value);
  }

  // Tests that the dropdown contains the appropriate destinations when there
  // are no recent destinations.
  test(
      'NoRecentDestinations', function() {
        initialize();
        return nativeLayer.whenCalled('getPrinterCapabilities').then(() => {
          // This will result in the destination store setting the Save as
          // PDF destination.
          assertEquals(
              GooglePromotedDestinationId.SAVE_AS_PDF,
              destinationSettings.destination.id);
          assertFalse(destinationSettings.$.destinationSelect.disabled);
          assertDropdownItems(['Save as PDF/local/']);
        });
      });

  // Tests that the dropdown contains the appropriate destinations when there
  // are 5 recent destinations.
  test(
      'RecentDestinations', function() {
        recentDestinations = destinations.slice(0, 5).map(
            destination => makeRecentDestination(destination));

        const whenCapabilitiesDone =
            nativeLayer.whenCalled('getPrinterCapabilities');
        initialize();

        // Wait for the destinations to be inserted into the store.
        return whenCapabilitiesDone
            .then(() => {
              return waitBeforeNextRender(destinationSettings);
            })
            .then(() => {
              // This will result in the destination store setting the most
              // recent destination.
              assertEquals('ID1', destinationSettings.destination.id);
              assertFalse(destinationSettings.$.destinationSelect.disabled);
              const dropdownItems = [
                makeLocalDestinationKey('ID1'), makeLocalDestinationKey('ID2'),
                makeLocalDestinationKey('ID3'), 'Save as PDF/local/',
              ];
              assertDropdownItems(dropdownItems);
            });
      });

  // Tests that the dropdown contains the appropriate destinations when one of
  // the destinations can no longer be found.
  test(
      'RecentDestinationsMissing', function() {
        recentDestinations = destinations.slice(0, 5).map(
            destination => makeRecentDestination(destination));
        localDestinations.splice(1, 1);
        nativeLayer.setLocalDestinations(localDestinations);
        const whenCapabilitiesDone =
            nativeLayer.whenCalled('getPrinterCapabilities');

        initialize();

        // Wait for the destinations to be inserted into the store.
        return whenCapabilitiesDone
            .then(() => {
              return waitBeforeNextRender(destinationSettings);
            })
            .then(() => {
              // This will result in the destination store setting the most
              // recent destination.
              assertEquals('ID1', destinationSettings.destination.id);
              assertFalse(destinationSettings.$.destinationSelect.disabled);
              const dropdownItems = [
                makeLocalDestinationKey('ID1'), makeLocalDestinationKey('ID3'),
                'Save as PDF/local/',
              ];
              assertDropdownItems(dropdownItems);
            });
      });

  // Tests that the dropdown contains the appropriate destinations when Save
  // as PDF is one of the recent destinations.
  test('SaveAsPdfRecent', function() {
    recentDestinations = destinations.slice(0, 5).map(
        destination => makeRecentDestination(destination));
    recentDestinations.splice(
        1, 1, makeRecentDestination(getSaveAsPdfDestination()));
    const whenCapabilitiesDone =
        nativeLayer.whenCalled('getPrinterCapabilities');
    initialize();

    return whenCapabilitiesDone
        .then(() => {
          return waitBeforeNextRender(destinationSettings);
        })
        .then(() => {
          // This will result in the destination store setting the most recent
          // destination.
          assertEquals('ID1', destinationSettings.destination.id);
          assertFalse(destinationSettings.$.destinationSelect.disabled);
          const dropdownItems = [
            makeLocalDestinationKey('ID1'), makeLocalDestinationKey('ID3'),
            makeLocalDestinationKey('ID4'), 'Save as PDF/local/',
          ];
          assertDropdownItems(dropdownItems);
        });
  });

  // Tests that selecting the Save as PDF destination results in the
  // DESTINATION_SELECT event firing, with Save as PDF set as the current
  // destination.
  test('SelectSaveAsPdf', function() {
    recentDestinations = destinations.slice(0, 5).map(
        destination => makeRecentDestination(destination));
    recentDestinations.splice(
        1, 1, makeRecentDestination(getSaveAsPdfDestination()));
    const whenCapabilitiesDone =
        nativeLayer.whenCalled('getPrinterCapabilities');
    initialize();

    const dropdown = destinationSettings.$.destinationSelect;

    return whenCapabilitiesDone
        .then(() => {
          return waitBeforeNextRender(destinationSettings);
        })
        .then(() => {
          // This will result in the destination store setting the most recent
          // destination.
          assertEquals('ID1', destinationSettings.destination.id);
          assertFalse(dropdown.disabled);
          const dropdownItems = [
            makeLocalDestinationKey('ID1'), makeLocalDestinationKey('ID3'),
            makeLocalDestinationKey('ID4'), 'Save as PDF/local/',
          ];
          assertDropdownItems(dropdownItems);
          // Most recent destination is selected by default.
          assertEquals('ID1', destinationSettings.destination.id);

          // Simulate selection of Save as PDF printer.
          const whenDestinationSelect = eventToPromise(
              DestinationStoreEventType.DESTINATION_SELECT,
              destinationSettings.getDestinationStoreForTest());
          dropdown.dispatchEvent(new CustomEvent(
              'selected-option-change',
              {bubbles: true, composed: true, detail: 'Save as PDF/local/'}));

          // Ensure this fires the destination select event.
          return whenDestinationSelect;
        })
        .then(() => {
          assertEquals(
              GooglePromotedDestinationId.SAVE_AS_PDF,
              destinationSettings.destination.id);
        });
  });

  // Tests that selecting a recent destination results in the
  // DESTINATION_SELECT event firing, with the recent destination set as the
  // current destination.
  test(
      'SelectRecentDestination', function() {
        recentDestinations = destinations.slice(0, 5).map(
            destination => makeRecentDestination(destination));
        const whenCapabilitiesDone =
            nativeLayer.whenCalled('getPrinterCapabilities');
        initialize();
        const dropdown = destinationSettings.$.destinationSelect;

        return whenCapabilitiesDone
            .then(() => {
              return waitBeforeNextRender(destinationSettings);
            })
            .then(() => {
              // This will result in the destination store setting the most
              // recent destination.
              assertEquals('ID1', destinationSettings.destination.id);
              assertFalse(dropdown.disabled);
              const dropdownItems = [
                makeLocalDestinationKey('ID1'), makeLocalDestinationKey('ID2'),
                makeLocalDestinationKey('ID3'), 'Save as PDF/local/',
              ];
              assertDropdownItems(dropdownItems);

              // Simulate selection of Save as PDF printer.
              const whenDestinationSelect = eventToPromise(
                  DestinationStoreEventType.DESTINATION_SELECT,
                  destinationSettings.getDestinationStoreForTest());
              dropdown.dispatchEvent(new CustomEvent('selected-option-change', {
                bubbles: true,
                composed: true,
                detail: makeLocalDestinationKey('ID2'),
              }));
              return whenDestinationSelect;
            })
            .then(() => {
              assertEquals('ID2', destinationSettings.destination.id);
            });
      });

  // Tests that selecting the 'see more' option opens the dialog.
  test('OpenDialog', function() {
    recentDestinations = destinations.slice(0, 5).map(
        destination => makeRecentDestination(destination));
    const whenCapabilitiesDone =
        nativeLayer.whenCalled('getPrinterCapabilities');
    initialize();
    const dropdown = destinationSettings.$.destinationSelect;

    return whenCapabilitiesDone
        .then(() => {
          return waitBeforeNextRender(destinationSettings);
        })
        .then(() => {
          // This will result in the destination store setting the most recent
          // destination.
          assertEquals('ID1', destinationSettings.destination.id);
          assertFalse(dropdown.disabled);
          const dropdownItems = [
            makeLocalDestinationKey('ID1'), makeLocalDestinationKey('ID2'),
            makeLocalDestinationKey('ID3'), 'Save as PDF/local/',
          ];
          assertDropdownItems(dropdownItems);

          dropdown.dispatchEvent(new CustomEvent(
              'selected-option-change',
              {bubbles: true, composed: true, detail: 'seeMore'}));
          return waitBeforeNextRender(destinationSettings);
        })
        .then(() => {
          assertTrue(destinationSettings.$.destinationDialog.get().isOpen());
        });
  });

  /**
   * @param expectedDestinationIds An array of the expected
   *     recent destination ids.
   */
  function assertRecentDestinations(expectedDestinationIds: string[]) {
    const recentDestinations =
        destinationSettings.getSettingValue('recentDestinations');
    assertEquals(expectedDestinationIds.length, recentDestinations.length);
    expectedDestinationIds.forEach((expectedId, index) => {
      assertEquals(expectedId, recentDestinations[index].id);
    });
  }

  function selectDestination(destination: Destination) {
    const storeDestination =
        destinationSettings.getDestinationStoreForTest().destinations().find(
            d => d.key === destination.key);
    assert(storeDestination);
    destinationSettings.getDestinationStoreForTest().selectDestination(
        storeDestination);
    flush();
  }

  /**
   * Tests that the destination being set correctly updates the recent
   * destinations array.
   */
  test(
      'UpdateRecentDestinations', function() {
        // Recent destinations start out empty.
        assertRecentDestinations([]);
        assertEquals(0, nativeLayer.getCallCount('getPrinterCapabilities'));

        initialize();

        return nativeLayer.whenCalled('getPrinterCapabilities')
            .then(() => {
              assertRecentDestinations(['Save as PDF']);
              assertEquals(
                  1, nativeLayer.getCallCount('getPrinterCapabilities'));

              // Add printers to store.
              nativeLayer.resetResolver('getPrinterCapabilities');
              destinationSettings.getDestinationStoreForTest()
                  .startLoadAllDestinations();
              return nativeLayer.whenCalled('getPrinters');
            })
            .then(() => {
              // Simulate setting a destination from the dialog.
              selectDestination(destinations[0]!);
              return nativeLayer.whenCalled('getPrinterCapabilities');
            })
            .then(() => {
              assertRecentDestinations(['ID1', 'Save as PDF']);
              assertEquals(
                  1, nativeLayer.getCallCount('getPrinterCapabilities'));

              // Reselect a recent destination. Still 2 destinations, but in a
              // different order.
              nativeLayer.resetResolver('getPrinterCapabilities');
              destinationSettings.$.destinationSelect.dispatchEvent(
                  new CustomEvent('selected-option-change', {
                    bubbles: true,
                    composed: true,
                    detail: 'Save as PDF/local/',
                  }));
              flush();
              assertRecentDestinations(['Save as PDF', 'ID1']);
              // No additional capabilities call, since the destination was
              // previously selected.
              assertEquals(
                  0, nativeLayer.getCallCount('getPrinterCapabilities'));

              // Select a third destination.
              selectDestination(destinations[1]!);
              return nativeLayer.whenCalled('getPrinterCapabilities');
            })
            .then(() => {
              assertRecentDestinations(['ID2', 'Save as PDF', 'ID1']);
              assertEquals(
                  1, nativeLayer.getCallCount('getPrinterCapabilities'));
              nativeLayer.resetResolver('getPrinterCapabilities');
              // Fill recent destinations up to the cap, then add a couple
              // more destinations. Make sure the length of the list does not
              // exceed NUM_PERSISTED_DESTINATIONS.
              const whenCapabilitiesDone =
                  nativeLayer.waitForMultipleCapabilities(
                      NUM_PERSISTED_DESTINATIONS);
              for (const destination of extraDestinations) {
                selectDestination(destination);
              }
              return whenCapabilitiesDone;
            })
            .then(() => {
              assertRecentDestinations(
                  extraDestinations.map(dest => dest.id).reverse());
              assertEquals(
                  NUM_PERSISTED_DESTINATIONS,
                  nativeLayer.getCallCount('getPrinterCapabilities'));
            });
      });

  // Tests that disabling the Save as PDF destination hides the corresponding
  // dropdown item.
  test(
      'DisabledSaveAsPdf', function() {
        // Initialize destination settings with the PDF printer disabled.
        pdfPrinterDisabled = true;
        initialize();

        return nativeLayer.whenCalled('getPrinterCapabilities')
            .then(() => {
              return waitBeforeNextRender(destinationSettings);
            })
            .then(() => {
              // Because the 'Save as PDF' fallback is unavailable, the first
              // destination is selected.
              const expectedDestination = makeLocalDestinationKey('ID1');
              assertDropdownItems([expectedDestination]);
            });
      });

  // Tests that disabling the 'Save as PDF' destination and exposing no
  // printers to the native layer results in a 'No destinations' option in the
  // dropdown.
  test('NoDestinations', function() {
    nativeLayer.setLocalDestinations([]);

    // Initialize destination settings with the PDF printer disabled.
    pdfPrinterDisabled = true;
    initialize();

    // 'getPrinters' will be called because there are no printers known to
    // the destination store and the 'Save as PDF' fallback is
    // unavailable.
    return Promise
        .all([
          nativeLayer.whenCalled('getPrinters'),
          // TODO (rbpotter): remove this wait once user manager is fully
          // removed.
          waitBeforeNextRender(destinationSettings),
        ])
        .then(() => assertDropdownItems(['noDestinations']));
  });
});
