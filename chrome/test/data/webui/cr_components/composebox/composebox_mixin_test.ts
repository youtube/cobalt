// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import {ComposeboxEmbedderMixin} from 'chrome://resources/cr_components/composebox/composebox_mixin.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {DriveDisclaimerStatus, DriveUploadError, PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote, SuggestInventory} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {InputType} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';

import {installMock} from './composebox_test_utils.js';

const TestElementBase = ComposeboxEmbedderMixin(I18nMixinLit(CrLitElement));

class TestComposeboxMixinElement extends TestElementBase {
  static get is() {
    return 'test-composebox-mixin';
  }

  // Implementation of ComposeboxEmbedderMixinInterface
  private mockInput_ = {
    inputElement: document.createElement('input'),
    input: '',
  };

  override getInputElement(): any {
    return this.mockInput_;
  }

  private mockDropdown_ = {
    unselect: () => {},
  };

  override getDropdownElement(): any {
    return this.mockDropdown_;
  }

  private activeElement_: Element|null = null;
  setActiveElement(elem: Element|null) {
    this.activeElement_ = elem;
  }

  override getActiveElement(): Element|null {
    return this.activeElement_;
  }

  override getPageHandler() {
    return ComposeboxProxyImpl.getInstance().handler;
  }

  override getSearchboxCallbackRouter() {
    return ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
  }

  override getSearchboxHandler() {
    return ComposeboxProxyImpl.getInstance().searchboxHandler;
  }

  override getContextEntrypointElement() {
    return null;
  }
}

customElements.define(
    TestComposeboxMixinElement.is, TestComposeboxMixinElement);

suite('ComposeboxMixinTest', () => {
  let element: TestComposeboxMixinElement;
  let handler: PageHandlerRemote&TestMock<PageHandlerRemote>;
  let searchboxHandler: SearchboxPageHandlerRemote&
      TestMock<SearchboxPageHandlerRemote>;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.resetForTesting({
      composeboxShowImageSuggest: false,
      composeboxSmartComposeEnabled: false,
      composeboxShowContextMenuDescription: false,
      composeboxShowZps: true,
      composeboxContextDragAndDropEnabled: false,
      composeboxSource: 'NTP',
      composeboxFileMaxCount: 1,
      composeboxFileMaxSize: 1024,
      composeboxAttachmentFileTypes: '.pdf',
      composeboxImageFileTypes: 'image/png',
      lensSendRawFileMediaTypesEnabled: false,
      voiceSearchCoherenceAnySearchboxExperimentEnabled: false,
      voiceSearchCoherenceSearchboxEnabled: false,
      voiceSearchCoherenceComposeboxesEnabled: false,
      composeDeepSearchPlaceholder: 'Deep Search',
      composeCreateImagePlaceholder: 'Create Image',
      searchboxComposePlaceholder: 'Compose',
      composeboxShowContextMenu: false,
      composeboxShowTypedSuggest: false,
      composeboxCancelButtonTitleInput: 'Cancel input',
      composeboxCancelButtonTitle: 'Cancel',
      voiceSearchButtonLabel: 'Voice search',
      lensSearchButtonLabel: 'Lens search',
      lensSearchHint: 'Lens search',
      suggestionActivityLink: '<a>Activity</a>',
      composeboxSubmitButtonTitle: 'Submit',
      composeboxSmartComposeTabTitle: 'Tab',
      composeboxSmartComposeTitle: 'Smart Compose',
      voiceListening: 'Listening',
      voiceDetails: 'Details',
      voiceClose: 'Close',
      voiceStop: 'Stop',
      dismissButton: 'Dismiss',
      composeboxDragAndDropHint: 'Hint',
      removeSuggestion: 'Remove',
      composeboxDeleteFileTitle: 'Delete',
      contextManagementInComposeboxEnabled: false,
      tabFaviconChipsToCoinsEnabled: false,
      maxFilesReachedError: 'Max files reached',
      composeboxFileUploadInvalidTooLarge: 'File too large',
      composeboxFileUploadStartedText: 'Upload started',
    });

    handler = installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));
    handler.setResultMapperFor(
        'getSmartTabSharingActive', () => Promise.resolve({active: false}));

    searchboxHandler = installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);

    element = document.createElement('test-composebox-mixin') as
        TestComposeboxMixinElement;
    document.body.appendChild(element);
    await element.updateComplete;
  });

  teardown(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test(
      'refreshTabSuggestions() dedupes restored and current tabs', async () => {
        const tab1 = {
          tabId: 0,
          title: 'Tab 1',
          url: 'about:blank?1',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        const tab2Restored = {
          tabId: 0,
          title: 'Tab 2',
          url: 'about:blank?2',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        const tab2Recent = {
          tabId: 2,
          title: 'Tab 2',
          url: 'about:blank?2',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };
        const tab3 = {
          tabId: 3,
          title: 'Tab 3',
          url: 'about:blank?3',
          showInCurrentTabChip: false,
          showInPreviousTabChip: false,
          lastActive: {internalValue: 0n},
        };

        // Mock searchboxHandler.getRecentTabs to return tab2Recent and tab3.
        searchboxHandler.setResultFor(
            'getRecentTabs', Promise.resolve({tabs: [tab2Recent, tab3]}));

        // Set aimThreadRestoredTabs to contain tab1 and tab2Restored.
        element.aimThreadRestoredTabs = [tab1, tab2Restored];

        await element.refreshTabSuggestions();

        // Expected tabSuggestions: [tab1, tab2Restored, tab3]
        // (tab2Recent from recent tabs should be filtered out because its URL
        // matches tab2Restored)
        assertEquals(3, element.tabSuggestions.length);
        assertEquals(0, element.tabSuggestions[0]!.tabId);
        assertEquals('about:blank?1', element.tabSuggestions[0]!.url);
        assertEquals(0, element.tabSuggestions[1]!.tabId);
        assertEquals('about:blank?2', element.tabSuggestions[1]!.url);
        assertEquals(3, element.tabSuggestions[2]!.tabId);
        assertEquals('about:blank?3', element.tabSuggestions[2]!.url);
      });

  test('clears selected tabs on submit', async () => {
    // Selected Tab (ID: 100) checked by the user.
    const tokenTab = 'test-token-tab' as unknown as UnguessableToken;
    const selectedTabId = 100;
    const mockTabFile = new ComposeboxFile(
        tokenTab, 'Selected Tab', 'tab', InputType.kBrowserTab, {
          isDeletable: true,
          tabId: selectedTabId,
          url: 'about:blank',
        });

    // Add the selected tab to the active files and added tabs maps.
    element.files = new Map([[tokenTab, mockTabFile]]);
    element.addedTabsIds = new Map([[selectedTabId, tokenTab]]);

    await element.updateComplete;

    element.submitCleanup();

    // Verify: The selected Tab 100 must be completely removed from the
    // current active selection.
    assertFalse(element.addedTabsIds.has(selectedTabId));
    assertFalse(element.files.has(tokenTab));
  });

  test('queryAutocomplete passes cursor position', async () => {
    element.input = 'hello';
    await element.updateComplete;

    const inputElement = element.getInputElement();
    inputElement.inputElement.value = 'hello';
    inputElement.inputElement.focus();
    inputElement.inputElement.selectionStart = 3;
    inputElement.inputElement.selectionEnd = 3;

    searchboxHandler.resetResolver('queryAutocompleteWithSuggestInventory');
    element.queryAutocomplete(/*clearMatches=*/ false);

    const args = await searchboxHandler.whenCalled(
        'queryAutocompleteWithSuggestInventory');
    assertDeepEquals(args, ['hello', false, 3, SuggestInventory.kDefault]);
  });

  test(
      'queryAutocomplete passes cursor position when input is out of sync',
      async () => {
        element.input = 'hello';
        await element.updateComplete;

        const inputElement = element.getInputElement();
        inputElement.inputElement.value = 'hello';
        inputElement.inputElement.focus();
        inputElement.inputElement.selectionStart = 3;
        inputElement.inputElement.selectionEnd = 3;

        // Simulate a programming update of the input as happens when, e.g., the
        // user closes the composebox. This update won't be immediately
        // reflected in the DOM.
        element.input = 'hello world';

        // Clear the `queryAutocompleteWithSuggestInventory` called for ZPS.
        searchboxHandler.resetResolver('queryAutocompleteWithSuggestInventory');
        element.queryAutocomplete(/*clearMatches=*/ false);

        const args = await searchboxHandler.whenCalled(
            'queryAutocompleteWithSuggestInventory');
        assertDeepEquals(
            args, ['hello world', false, 11, SuggestInventory.kDefault]);
      });

  test(
      'Shift+Enter allows inserting a newline when input is focused and not empty',
      async () => {
        element.input = 'Some text';
        await element.updateComplete;

        const inputElement = element.getInputElement();
        inputElement.inputElement.focus();

        const event = new KeyboardEvent('keydown', {
          key: 'Enter',
          shiftKey: true,
          bubbles: true,
          cancelable: true,
        });

        element.setActiveElement(inputElement.inputElement);

        element.onKeydown(event);

        assertFalse(event.defaultPrevented);
      });

  test(
      'Enter prevents inserting a newline and attempts to submit query when focus is not in dropdown',
      () => {
        const event = new KeyboardEvent('keydown', {
          key: 'Enter',
          shiftKey: false,
          bubbles: true,
          cancelable: true,
        });

        element.setActiveElement(element.getInputElement().inputElement);

        element.onKeydown(event);

        assertTrue(event.defaultPrevented);
      });

  test(
      'Shift+Enter submits dropdown selection when focus is in dropdown',
      () => {
        const event = new KeyboardEvent('keydown', {
          key: 'Enter',
          shiftKey: true,
          bubbles: true,
          cancelable: true,
        });

        element.setActiveElement(element.getDropdownElement());

        element.onKeydown(event);

        assertTrue(event.defaultPrevented);
      });

  test('autocomplete matches are cleared on submit', async () => {
    element.input = 'Some text';
    await element.updateComplete;

    const event = new KeyboardEvent('keydown', {
      key: 'Enter',
      shiftKey: false,
      bubbles: true,
      cancelable: true,
    });
    element.setActiveElement(element.getInputElement().inputElement);
    element.onKeydown(event);

    const clearResult = await searchboxHandler.whenCalled('stopAutocomplete');
    assertTrue(clearResult);
    assertFalse(element.showDropdown);
    assertEquals(null, element.result);
    assertEquals('', element.lastQueriedInput);
  });

  test('smartComposeInlineHint is sliced on sequential typing', async () => {
    element.smartComposeEnabled = true;
    element.input = 'hello';
    element.smartComposeInlineHint = ' world';
    await element.updateComplete;

    const inputElem = element.getInputElement();
    inputElem.input = 'hello ';
    element.onInputInput(new CustomEvent('input') as any);
    await element.updateComplete;

    assertEquals('world', element.smartComposeInlineHint);
    assertEquals('hello ', element.input);

    inputElem.input = 'hello w';
    element.onInputInput(new CustomEvent('input') as any);
    await element.updateComplete;

    assertEquals('orld', element.smartComposeInlineHint);
  });

  test('smartComposeInlineHint is cleared on non-matching typing', async () => {
    element.smartComposeEnabled = true;
    element.input = 'hello';
    element.smartComposeInlineHint = ' world';
    await element.updateComplete;

    const inputElem = element.getInputElement();
    inputElem.input = 'hello!';
    element.onInputInput(new CustomEvent('input') as any);
    await element.updateComplete;

    assertEquals('', element.smartComposeInlineHint);
  });

  test(
      'filters tabs from carousel when tab chips to coins flag is enabled',
      async () => {
        loadTimeData.overrideValues({
          tabFaviconChipsToCoinsEnabled: true,
        });

        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        const freshComposebox =
            document.createElement('test-composebox-mixin') as
            TestComposeboxMixinElement;
        document.body.appendChild(freshComposebox);

        const regularFile = {
          name: 'image.png',
          type: 'image/png',
        } as unknown as ComposeboxFile;
        const tabFile = {
          name: 'Google',
          url: {url: 'about:blank'},
        } as unknown as ComposeboxFile;
        freshComposebox.files = new Map([
          ['uuid-1' as unknown as UnguessableToken, regularFile],
          ['uuid-2' as unknown as UnguessableToken, tabFile],
        ]);

        freshComposebox.requestUpdate();
        await freshComposebox.updateComplete;

        const filteredFiles = freshComposebox.getFilteredCarouselFiles();
        assertEquals(1, filteredFiles.length);
        assertEquals('image.png', filteredFiles[0]!.name);
      });

  test('does not filter tabs from carousel when flag is disabled', async () => {
    loadTimeData.overrideValues({
      tabFaviconChipsToCoinsEnabled: false,
    });

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const freshComposebox = document.createElement('test-composebox-mixin') as
        TestComposeboxMixinElement;
    document.body.appendChild(freshComposebox);

    const regularFile = {
      name: 'image.png',
      type: 'image/png',
    } as unknown as ComposeboxFile;
    const tabFile = {
      name: 'Google',
      url: {url: 'about:blank'},
    } as unknown as ComposeboxFile;
    freshComposebox.files = new Map([
      ['uuid-1' as unknown as UnguessableToken, regularFile],
      ['uuid-2' as unknown as UnguessableToken, tabFile],
    ]);

    freshComposebox.requestUpdate();
    await freshComposebox.updateComplete;

    const filteredFiles = freshComposebox.getFilteredCarouselFiles();
    assertEquals(2, filteredFiles.length);
  });

  test(
      'onOpenDriveUpload suppresses upload if disclaimer not accepted',
      async () => {
        searchboxHandler.setResultFor(
            'getDriveDisclaimerStatus',
            Promise.resolve({status: DriveDisclaimerStatus.kNotAccepted}));

        await element.onOpenDriveUpload();

        assertEquals(
            1, searchboxHandler.getCallCount('getDriveDisclaimerStatus'));
        assertEquals(0, searchboxHandler.getCallCount('onDriveUploadClicked'));
      });

  test('onOpenDriveUpload triggers upload if disclaimer accepted', async () => {
    searchboxHandler.setResultFor(
        'getDriveDisclaimerStatus',
        Promise.resolve({status: DriveDisclaimerStatus.kAccepted}));
    searchboxHandler.setResultFor(
        'onDriveUploadClicked',
        Promise.resolve({response: {files: [], error: null}}));

    await element.onOpenDriveUpload();

    assertEquals(1, searchboxHandler.getCallCount('getDriveDisclaimerStatus'));
    assertEquals(1, searchboxHandler.getCallCount('onDriveUploadClicked'));
  });

  test('addDriveUploads adds files to composebox', async () => {
    const token = {high: 1n, low: 1n} as unknown as UnguessableToken;
    element.addDriveUploads([{
      token,
      mimeType: 'image/png',
      fileName: 'file.png',
      thumbnailUrl: 'thumb',
      iconUrl: 'icon',
    }]);

    await element.updateComplete;
    assertTrue(element.files.has(token));
    const file = element.files.get(token)!;
    assertEquals('file.png', file.name);
    assertEquals('image/png', file.type);
    assertFalse(element.showDropdown);
  });

  test('addDriveUploads handles max files exceeded error', async () => {
    element.addDriveUploads([], DriveUploadError.kMaxFilesExceeded);
    await element.updateComplete;
    assertEquals(element.i18n('maxFilesReachedError'), element.errorMessage);
  });

  test('addDriveUploads handles size limit exceeded error', async () => {
    element.addDriveUploads([], DriveUploadError.kSizeLimitExceeded);
    await element.updateComplete;
    assertEquals(
        element.i18n('composeboxFileUploadInvalidTooLarge', 0),
        element.errorMessage);
  });

  test('updateState calls addDriveUploads for drive uploads', async () => {
    const token = {high: 2n, low: 2n} as unknown as UnguessableToken;
    element.state = {
      text: 'hello',
      files: [{
        token,
        mimeType: 'image/png',
        fileName: 'file.png',
        thumbnailUrl: 'thumb',
        iconUrl: 'icon',
      }],
      mode: 0,
      model: 0,
    };
    await element.updateComplete;
    assertTrue(element.files.has(token));
    assertEquals('hello', element.input);
  });
});
