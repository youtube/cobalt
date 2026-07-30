// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/composebox/composebox.js';

import type {ComposeboxElement} from 'chrome://resources/cr_components/composebox/composebox.js';
import {PageCallbackRouter, PageHandlerRemote} from 'chrome://resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxInputElement} from 'chrome://resources/cr_components/composebox/composebox_input.js';
import {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
import type {ContextualEntrypointAndMenuElement} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import {WindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageRemote as SearchboxPageRemote} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {getTrustedHtml} from 'chrome://webui-test/trusted_html.js';

import {installMock} from './composebox_test_utils.js';

suite('ComposeboxTest', () => {
  let composebox: ComposeboxElement;
  let handler: PageHandlerRemote&TestMock<PageHandlerRemote>;
  let searchboxCallbackRouterRemote: SearchboxPageRemote;
  let searchboxHandler: SearchboxPageHandlerRemote&TestMock<SearchboxPageHandlerRemote>;

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
    searchboxHandler.setResultFor(
        'getPageClassification',
        Promise.resolve({metricSource: 'CO_BROWSING_COMPOSEBOX'}));

    searchboxCallbackRouterRemote =
        ComposeboxProxyImpl.getInstance()
            .searchboxCallbackRouter.$.bindNewPipeAndPassRemote();

    composebox = document.createElement('cr-composebox');
    document.body.appendChild(composebox);
    await composebox.updateComplete;
  });

  teardown(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('suggestion activity link triggers navigation', async () => {
    composebox.suggestionActivityEnabled = true;

    // Mock results to show the link.
    const matches = [
      createSearchMatchForTesting({
        isNoncannedAimSuggestion: true,
      }),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await composebox.updateComplete;

    const suggestionActivity = composebox.shadowRoot.querySelector('#suggestionActivity');
    assertTrue(!!suggestionActivity);
    const localizedLink = suggestionActivity.querySelector('localized-link');
    assertTrue(!!localizedLink);

    const testUrl = 'about:blank?activity';
    // Simulate the event fired by localized-link.
    const anchor = document.createElement('a');
    anchor.href = testUrl;

    let preventDefaultCalled = false;
    const linkClickedEvent = new CustomEvent('link-clicked', {
      detail: {
        event: {
          preventDefault: () => {
            preventDefaultCalled = true;
          },
          currentTarget: anchor,
        },
      },
    });

    localizedLink.dispatchEvent(linkClickedEvent);

    const url = await handler.whenCalled('navigateUrl');
    assertEquals(testUrl, url);
    assertTrue(preventDefaultCalled);
  });

  test('suggestion activity link hidden when suggestions are non canned', async () => {
    composebox.suggestionActivityEnabled = true;

    const matches = [
      createSearchMatchForTesting({
        isNoncannedAimSuggestion: false,
      }),
    ];
    searchboxCallbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: matches,
        }));
    await searchboxCallbackRouterRemote.$.flushForTesting();
    await microtasksFinished();
    await composebox.updateComplete;

    const suggestionActivity = composebox.shadowRoot.querySelector('#suggestionActivity');
    assertFalse(!!suggestionActivity);
  });

  test(
      'smartComposeEnabled forwards from <cr-composebox> to <cr-composebox-input>',
      async () => {
        loadTimeData.overrideValues({composeboxSmartComposeEnabled: true});

        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        const fresh = document.createElement('cr-composebox');
        document.body.appendChild(fresh);
        await fresh.updateComplete;

        const input = fresh.shadowRoot.querySelector<ComposeboxInputElement>(
            'cr-composebox-input');
        assertTrue(!!input);
        await input.updateComplete;

        assertTrue(input.hasAttribute('smart-compose-enabled'));
      });

  test(
      'voice permission changed updates search-animated-glow' +
          'class and hides audio-wave',
      async () => {
        // Mock WindowProxy to enable voice search.
        const windowProxy = installMock(WindowProxy);
        windowProxy.setResultFor('hasWebkitSpeechRecognition', true);

        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: false,
        });

        // Recreate composebox so updated loadTimeData and WindowProxy mock take
        // effect.
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        composebox = document.createElement('cr-composebox');
        composebox.showVoiceSearch = true;
        document.body.appendChild(composebox);
        await composebox.updateComplete;

        const glow =
            composebox.shadowRoot.querySelector('search-animated-glow');
        assertTrue(!!glow);

        // Inject style to disable transitions for instant opacity evaluation.
        const style = document.createElement('style');
        style.textContent =
            '* { transition: none !important; animation: none !important; }';
        glow.shadowRoot.appendChild(style);

        // Make sure it is listening so the audio element becomes visible
        // (opacity 1).
        composebox.isListening = true;
        await composebox.updateComplete;
        await glow.updateComplete;

        assertTrue(glow.isListening, 'glow.isListening should be true');
        assertTrue(
            glow.hasAttribute('is-listening'),
            'glow should have is-listening attribute');

        const audioWave = glow.shadowRoot.querySelector('audio-wave');
        assertTrue(!!audioWave);
        assertEquals('1', window.getComputedStyle(audioWave).opacity);

        // Simulate voice permission prompt opening.
        composebox.onVoicePermissionChanged(
            new CustomEvent('voice-permission-changed', {
              detail: {
                isOpened: true,
                height: 100,
                width: 200,
              },
            }));
        await composebox.updateComplete;

        // Verify the class was added and opacity turned to 0.
        assertTrue(
            glow.classList.contains('embedded-permission-prompt-showing'));
        assertEquals('0', window.getComputedStyle(audioWave).opacity);
      });

  test(
      'voice permission changed updates search-animated-glow class' +
          'and hides recording-wave',
      async () => {
        // Mock WindowProxy to enable voice search.
        const windowProxy = installMock(WindowProxy);
        windowProxy.setResultFor('hasWebkitSpeechRecognition', true);

        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: true,
        });

        // Recreate composebox so updated loadTimeData and WindowProxy mock take
        // effect.
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        composebox = document.createElement('cr-composebox');
        composebox.showVoiceSearch = true;
        document.body.appendChild(composebox);
        await composebox.updateComplete;

        const glow =
            composebox.shadowRoot.querySelector('search-animated-glow');
        assertTrue(!!glow);

        // Inject style to disable transitions for instant opacity evaluation.
        const style = document.createElement('style');
        style.textContent =
            '* { transition: none !important; animation: none !important; }';
        glow.shadowRoot.appendChild(style);

        // Make sure it is listening so the audio element becomes visible
        // (opacity 1)
        composebox.isListening = true;
        await composebox.updateComplete;
        await glow.updateComplete;

        assertTrue(glow.isListening, 'glow.isListening should be true');
        assertTrue(
            glow.hasAttribute('is-listening'),
            'glow should have is-listening attribute');

        const recordingWave = glow.shadowRoot.querySelector('recording-wave');
        assertTrue(!!recordingWave);
        assertEquals('1', window.getComputedStyle(recordingWave).opacity);

        // Simulate voice permission prompt opening.
        composebox.onVoicePermissionChanged(
            new CustomEvent('voice-permission-changed', {
              detail: {
                isOpened: true,
                height: 100,
                width: 200,
              },
            }));
        await composebox.updateComplete;

        // Verify the class was added and opacity turned to 0.
        assertTrue(
            glow.classList.contains('embedded-permission-prompt-showing'));
        assertEquals('0', window.getComputedStyle(recordingWave).opacity);
      });

  test(
      'voice permission changed updates cr-composebox-voice-search class' +
          'and hides bottomActions',
      async () => {
        // Mock WindowProxy to enable voice search.
        const windowProxy = installMock(WindowProxy);
        windowProxy.setResultFor('hasWebkitSpeechRecognition', true);

        loadTimeData.overrideValues({
          voiceSearchCoherenceComposeboxesEnabled: true,
        });

        // Recreate composebox so updated loadTimeData and WindowProxy mock take
        // effect.
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        composebox = document.createElement('cr-composebox');
        composebox.showVoiceSearch = true;
        document.body.appendChild(composebox);
        await composebox.updateComplete;

        const voiceSearch =
            composebox.shadowRoot.querySelector('cr-composebox-voice-search');
        assertTrue(!!voiceSearch);

        // Inject style to disable transitions for instant opacity evaluation.
        const style = document.createElement('style');
        style.textContent =
            '* { transition: none !important; animation: none !important; }';
        voiceSearch.shadowRoot.appendChild(style);

        const bottomActions =
            voiceSearch.shadowRoot.querySelector('#bottomActions');
        assertTrue(!!bottomActions);

        // Make sure it is initially visible (opacity 1).
        assertEquals('1', window.getComputedStyle(bottomActions).opacity);

        // Simulate voice permission prompt opening.
        composebox.onVoicePermissionChanged(
            new CustomEvent('voice-permission-changed', {
              detail: {
                isOpened: true,
                height: 100,
                width: 200,
              },
            }));
        await composebox.updateComplete;
        await voiceSearch.updateComplete;

        // Verify the class was added and opacity turned to 0.
        assertTrue(
            voiceSearch.classList.contains(
                'embedded-permission-prompt-showing'));
        assertEquals('0', window.getComputedStyle(bottomActions).opacity);
      });

  test('connectedCallback calls getSmartTabSharingActive when' +
        ' smartTabSharingVisible pre-set to true', async () => {
    handler.setResultMapperFor(
        'getSmartTabSharingActive',
        () => Promise.resolve({active: true}));

    const newComposebox = document.createElement('cr-composebox');
    newComposebox.smartTabSharingVisible = true;
    document.body.appendChild(newComposebox);
    await handler.whenCalled('getSmartTabSharingActive');
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('getSmartTabSharingActive'));
    assertTrue(newComposebox.smartTabSharingActive);
  });

  test('connectedCallback does NOT call getSmartTabSharingActive when' +
        ' smartTabSharingVisible is false', () => {
    const newComposebox = document.createElement('cr-composebox');
    newComposebox.smartTabSharingVisible = false;
    document.body.appendChild(newComposebox);

    assertEquals(0, handler.getCallCount('getSmartTabSharingActive'));
    assertFalse(newComposebox.smartTabSharingActive);
  });

  test('host template .prop binding triggers getSmartTabSharingActive' +
        ' at child mount', async () => {
    handler.setResultMapperFor(
        'getSmartTabSharingActive',
        () => Promise.resolve({active: true}));

    document.body.innerHTML = getTrustedHtml(`
      <cr-composebox smart-tab-sharing-visible></cr-composebox>
    `);

    const newComposebox =
        document.body.querySelector<ComposeboxElement>('cr-composebox');
    assertTrue(!!newComposebox);

    await handler.whenCalled('getSmartTabSharingActive');
    await microtasksFinished();

    assertEquals(1, handler.getCallCount('getSmartTabSharingActive'));
    assertTrue(newComposebox.smartTabSharingActive);
  });

});

suite('composeboxSharedMountAutoRepositionDefault', () => {
  let composebox: ComposeboxElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    loadTimeData.resetForTesting({
      // Reuse the ComposeboxTest suite's key set, but sets
      // `composeboxShowContextMenu` to true so
      // composebox_context_menu.html.ts's
      // shared `<cr-composebox-contextual-entrypoint-and-menu>` mount renders.
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
      composeboxShowContextMenu: true,
      // Keys accessed by ContextualActionMenuElement class-field
      // initialization once the shared
      // `<cr-composebox-contextual-entrypoint-and-menu>` mount renders.
      // loadTimeData.getBoolean() asserts on absent keys, so these are
      // required.
      // Not optional with defaults - when `composeboxShowContextMenu` is true.
      composeboxContextMenuEnableMultiTabSelection: false,
      composeboxShowContextMenuTabPreviews: false,
      ShowContextMenuHeaders: false,
      menu: 'menu',
      addContextTile: 'Add context',
      addContext: 'Add context',
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
      dismissButton: 'Dismiss',
      composeboxDragAndDropHint: 'Hint',
      removeSuggestion: 'Remove',
      contextManagementInComposeboxEnabled: false,
      tabFaviconChipsToCoinsEnabled: false,
    });

    const handler = installMock(
        PageHandlerRemote,
        mock => ComposeboxProxyImpl.setInstance(new ComposeboxProxyImpl(
            mock, new PageCallbackRouter(), new SearchboxPageHandlerRemote(),
            new SearchboxPageCallbackRouter())));
    handler.setResultMapperFor(
        'getSmartTabSharingActive', () => Promise.resolve({active: false}));

    installMock(
        SearchboxPageHandlerRemote,
        mock => ComposeboxProxyImpl.getInstance().searchboxHandler = mock);

    composebox = document.createElement('cr-composebox');
    composebox.showMenuOnClick = true;
    composebox.usePecApi = true;
    document.body.appendChild(composebox);
    await composebox.updateComplete;
  });

  teardown(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('ShareComposeboxMountPreservesAutoReposition', async () => {
    const entrypointAndMenu = composebox.shadowRoot.querySelector<
        ContextualEntrypointAndMenuElement>(
      'cr-composebox-contextual-entrypoint-and-menu');
    assertTrue(!!entrypointAndMenu);
    await entrypointAndMenu.updateComplete;
    assertFalse(entrypointAndMenu.disableAutoReposition);

    const contextualActionMenu = entrypointAndMenu.$.menu;
    await contextualActionMenu.updateComplete;
    const crActionMenu = contextualActionMenu.$.menu;
    assertTrue(crActionMenu.autoReposition);
    assertTrue(crActionMenu.hasAttribute('auto-reposition'));
  });
});
