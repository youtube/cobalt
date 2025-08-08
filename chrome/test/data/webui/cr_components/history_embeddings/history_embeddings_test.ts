// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/strings.m.js';
import 'chrome://resources/cr_components/history_embeddings/history_embeddings.js';

import {CrFeedbackOption} from '//resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import {getFaviconForPageURL} from '//resources/js/icon.js';
import {HistoryEmbeddingsBrowserProxyImpl} from 'chrome://resources/cr_components/history_embeddings/browser_proxy.js';
import type {HistoryEmbeddingsElement} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.js';
import {AnswerStatus, PageHandlerRemote, UserFeedback} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.mojom-webui.js';
import type {SearchQuery, SearchResult, SearchResultItem} from 'chrome://resources/cr_components/history_embeddings/history_embeddings.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

[true, false].forEach((enableAnswers) => {
  const suitSuffix = enableAnswers ? 'enabled' : 'disabled';
  suite(`cr-history-embeddings-answers-${suitSuffix}`, () => {
    let element: HistoryEmbeddingsElement;
    let handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

    const mockResults: SearchResultItem[] = [
      {
        title: 'Google',
        url: {url: 'http://google.com'},
        urlForDisplay: 'google.com',
        relativeTime: '2 hours ago',
        shortDateTime: 'Sept 2, 2022',
        sourcePassage: 'Google description',
        lastUrlVisitTimestamp: 1000,
        answerData: null,
      },
      {
        title: 'Youtube',
        url: {url: 'http://youtube.com'},
        urlForDisplay: 'youtube.com',
        relativeTime: '4 hours ago',
        shortDateTime: 'Sept 2, 2022',
        sourcePassage: 'Youtube description',
        lastUrlVisitTimestamp: 2000,
        answerData: null,
      },
    ];

    setup(async () => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      loadTimeData.overrideValues({
        enableHistoryEmbeddingsAnswers: enableAnswers,
        enableHistoryEmbeddingsImages: true,
      });

      handler = TestMock.fromClass(PageHandlerRemote);

      const mockSearch = handler.search;
      handler.search = (query: SearchQuery) => {
        mockSearch(query);
        // Simulate a response from browser. If some other results are needed,
        // consider calling this directly from tests instead.
        element.searchResultChangedForTesting({
          query: query.query,
          answerStatus: AnswerStatus.kUnspecified,
          answer: '',
          items: [...mockResults],
        });
      };

      HistoryEmbeddingsBrowserProxyImpl.setInstance(
          new HistoryEmbeddingsBrowserProxyImpl(handler));

      element = document.createElement('cr-history-embeddings');
      document.body.appendChild(element);
      element.overrideLoadingStateMinimumMsForTesting(0);
      element.showMoreFromSiteMenuOption = true;

      element.numCharsForQuery = 21;
      element.searchQuery = 'some query';
      await handler.whenCalled('search');
      element.overrideQueryResultMinAgeForTesting(0);
      return flushTasks();
    });

    function getResultElements(): HTMLElement[] {
      if (enableAnswers) {
        return Array.from(element.shadowRoot!.querySelectorAll('.result-item'));
      }
      return Array.from(
          element.shadowRoot!.querySelectorAll('cr-url-list-item'));
    }

    test('Searches', async () => {
      assertEquals('some query', handler.getArgs('search')[0].query);
    });

    test('DisplaysHeading', async () => {
      loadTimeData.overrideValues({
        historyEmbeddingsHeading: 'searched for "$1"',
        historyEmbeddingsWithAnswersResultsHeading:
            'searched with answers enabled',
        historyEmbeddingsHeadingLoading: 'loading results for "$1"',
      });
      element.searchQuery = 'my query';
      const headingEl = element.shadowRoot!.querySelector('.results-heading');
      assertTrue(!!headingEl);
      assertEquals(
          'loading results for "my query"', headingEl.textContent!.trim());
      await handler.whenCalled('search');
      await flushTasks();
      if (enableAnswers) {
        assertEquals(
            'searched with answers enabled', headingEl.textContent!.trim());
      } else {
        assertEquals('searched for "my query"', headingEl.textContent!.trim());
      }
    });

    test('DisplaysLoading', async () => {
      element.overrideLoadingStateMinimumMsForTesting(100);
      element.searchQuery = 'my new query';
      await handler.whenCalled('search');
      const loadingResultsEl =
          element.shadowRoot!.querySelector('.loading-results');
      assertTrue(!!loadingResultsEl);
      assertTrue(
          isVisible(loadingResultsEl),
          'Loading state should be visible even if search immediately resolved');

      await new Promise(resolve => setTimeout(resolve, 100));
      assertFalse(
          isVisible(loadingResultsEl),
          'Loading state should disappear once the minimum of 100ms is over');

      if (enableAnswers) {
        element.searchResultChangedForTesting({
          query: 'my new query',
          answerStatus: AnswerStatus.kLoading,
          answer: '',
          items: [...mockResults],
        });
        await flushTasks();
        const loadingAnswersEl =
            element.shadowRoot!.querySelector('.loading-answer');
        assertTrue(!!loadingAnswersEl);
        assertTrue(
            isVisible(loadingAnswersEl), 'Answers should still be loading');

        // Answer status changes to success, which should hide loading state.
        element.searchResultChangedForTesting({
          query: 'my new query',
          answerStatus: AnswerStatus.kSuccess,
          answer: 'some answer',
          items: [...mockResults],
        });
        await flushTasks();
        assertFalse(
            isVisible(loadingAnswersEl), 'Answers should no longer be loading');
      }
    });

    test('DisplaysResults', async () => {
      const resultsElements = getResultElements();
      assertEquals(2, resultsElements.length);
      if (enableAnswers) {
        assertEquals(
            'Google',
            resultsElements[0]!.querySelector<HTMLElement>(
                                   '.result-title')!.innerText);
        assertEquals(
            'Youtube',
            resultsElements[1]!.querySelector<HTMLElement>(
                                   '.result-title')!.innerText);
      } else {
        assertEquals('Google', resultsElements[0]!.title);
        assertEquals('Youtube', resultsElements[1]!.title);
      }
    });

    // If this test is flaky, the order of search results being processed by
    // the component is not correct. See crbug.com/371049023.
    test('MultipleResultsAtTheSameTime', async () => {
      element.overrideLoadingStateMinimumMsForTesting(100);

      // Perform a new search.
      element.searchQuery = 'my new query';
      await flushTasks();

      // Two search results immediately after each other will sometimes have
      // the same value for `performance.now()`. In these cases, the results
      // should still be processed in the same order and not result in the
      // loading status being the last processed result.
      element.searchResultChangedForTesting({
        query: 'my new query',
        answerStatus: AnswerStatus.kLoading,
        answer: '',
        items: [...mockResults],
      });
      element.searchResultChangedForTesting({
        query: 'my new query',
        answerStatus: AnswerStatus.kUnanswerable,
        answer: '',
        items: [...mockResults],
      });

      // Wait for all timeouts to flush.
      await new Promise(resolve => setTimeout(resolve, 100));
      await flushTasks();

      const loadingAnswersEl =
          element.shadowRoot!.querySelector('.loading-answer');
      assertFalse(isVisible(loadingAnswersEl));
    });

    test('SwitchesDateFormat', async () => {
      element.showRelativeTimes = false;
      await flushTasks();
      const times = getResultElements().map(
          result => result.querySelector<HTMLElement>('.time'));
      assertEquals(mockResults[0]!.shortDateTime, times[0]!.innerText);
      assertEquals(mockResults[1]!.shortDateTime, times[1]!.innerText);

      element.showRelativeTimes = true;
      await flushTasks();
      assertEquals(mockResults[0]!.relativeTime, times[0]!.innerText);
      assertEquals(mockResults[1]!.relativeTime, times[1]!.innerText);
    });

    test('FiresClick', async () => {
      const resultsElements = getResultElements();
      const resultClickEventPromise = eventToPromise('result-click', element);
      // Prevent clicking from actually open in new tabs for native anchor tags.
      resultsElements[0]!.addEventListener('click', (e) => e.preventDefault());
      resultsElements[0]!.dispatchEvent(new MouseEvent('click', {
        button: 1,
        altKey: true,
        metaKey: false,
        shiftKey: true,
        ctrlKey: false,
        cancelable: true,
      }));
      const resultClickEvent = await resultClickEventPromise;
      assertEquals(mockResults[0], resultClickEvent.detail.item);
      assertEquals(true, resultClickEvent.detail.middleButton);
      assertEquals(true, resultClickEvent.detail.altKey);
      assertEquals(false, resultClickEvent.detail.ctrlKey);
      assertEquals(false, resultClickEvent.detail.metaKey);
      assertEquals(true, resultClickEvent.detail.shiftKey);
    });

    test('FiresContextMenu', async () => {
      const resultsElements = getResultElements();
      const resultContextMenuEventPromise =
          eventToPromise('result-context-menu', element);
      resultsElements[0]!.dispatchEvent(
          new MouseEvent('contextmenu', {clientX: 50, clientY: 100}));
      const resultContextMenuEvent = await resultContextMenuEventPromise;
      assertEquals(mockResults[0], resultContextMenuEvent.detail.item);
      assertEquals(50, resultContextMenuEvent.detail.x);
      assertEquals(100, resultContextMenuEvent.detail.y);
    });

    test('FiresMouseEventsForAnswerLink', async () => {
      if (!enableAnswers) {
        return;
      }

      const resultWithAnswer = {
        title: 'Website with answer',
        url: {url: 'http://answer.com'},
        urlForDisplay: 'Answer.com',
        relativeTime: '2 months ago',
        shortDateTime: 'Sept 2, 2022',
        sourcePassage: 'Answer description',
        lastUrlVisitTimestamp: 2000,
        answerData: {answerTextDirectives: []},
      };
      element.searchResultChangedForTesting({
        query: 'some query',
        answerStatus: AnswerStatus.kSuccess,
        answer: 'some answer',
        items: [...mockResults, resultWithAnswer],
      });
      await flushTasks();

      const answerLink = element.shadowRoot!.querySelector('.answer-link');
      assertTrue(!!answerLink);
      answerLink.addEventListener('click', (e) => e.preventDefault());
      const answerClickEventPromise = eventToPromise('answer-click', element);
      answerLink.dispatchEvent(new MouseEvent('click', {
        button: 1,
        altKey: true,
        metaKey: false,
        shiftKey: true,
        ctrlKey: false,
        cancelable: true,
      }));
      const answerClickEvent = await answerClickEventPromise;
      assertEquals(resultWithAnswer, answerClickEvent.detail.item);
      assertEquals(true, answerClickEvent.detail.middleButton);
      assertEquals(true, answerClickEvent.detail.altKey);
      assertEquals(false, answerClickEvent.detail.ctrlKey);
      assertEquals(false, answerClickEvent.detail.metaKey);
      assertEquals(true, answerClickEvent.detail.shiftKey);

      const answerLinkContextMenuEventPromise =
          eventToPromise('answer-context-menu', element);
      answerLink.dispatchEvent(
          new MouseEvent('contextmenu', {clientX: 50, clientY: 100}));
      const answerLinkContextMenuEvent =
          await answerLinkContextMenuEventPromise;
      assertEquals(resultWithAnswer, answerLinkContextMenuEvent.detail.item);
      assertEquals(50, answerLinkContextMenuEvent.detail.x);
      assertEquals(100, answerLinkContextMenuEvent.detail.y);
    });

    test('FiresClickOnMoreActions', async () => {
      const moreActionsIconButtons = getResultElements().map(
          result => result.querySelector('cr-icon-button'));
      moreActionsIconButtons[0]!.dispatchEvent(new Event('click'));
      await flushTasks();

      // Clicking on the more actions button for the first item should load
      // the cr-action-menu and open it.
      const moreActionsMenu =
          element.shadowRoot!.querySelector('cr-action-menu');
      assertTrue(!!moreActionsMenu);
      assertTrue(moreActionsMenu.open);

      const actionMenuItems = moreActionsMenu.querySelectorAll('button');
      assertEquals(2, actionMenuItems.length);

      // Clicking on the first button should fire the 'more-from-site-click'
      // event with the first item's model, and then close the menu.
      const moreFromSiteEventPromise =
          eventToPromise('more-from-site-click', element);
      const moreFromSiteItem =
          moreActionsMenu.querySelector<HTMLElement>('#moreFromSiteOption')!;
      moreFromSiteItem.dispatchEvent(new Event('click'));
      const moreFromSiteEvent = await moreFromSiteEventPromise;
      assertEquals(mockResults[0], moreFromSiteEvent.detail);
      assertFalse(moreActionsMenu.open);

      // Clicking on the second button should fire the 'remove-item-click' event
      // with the second item's model, and then close the menu.
      moreActionsIconButtons[1]!.dispatchEvent(new Event('click'));
      assertTrue(moreActionsMenu.open);
      const removeItemEventPromise =
          eventToPromise('remove-item-click', element);
      const removeItemItem = moreActionsMenu.querySelector<HTMLElement>(
          '#removeFromHistoryOption')!;
      removeItemItem.dispatchEvent(new Event('click'));
      await flushTasks();
      const removeItemEvent = await removeItemEventPromise;
      assertEquals(mockResults[1], removeItemEvent.detail);
      assertFalse(moreActionsMenu.open);
    });

    test('RemovesItemsFromFrontend', async () => {
      const moreActionsIconButtons = getResultElements().map(
          result => result.querySelector('cr-icon-button'));

      // Open the 'more actions' menu for the first result and remove it.
      moreActionsIconButtons[0]!.dispatchEvent(new Event('click'));
      element.shadowRoot!
          .querySelector<HTMLElement>(
              '#removeFromHistoryOption')!.dispatchEvent(new Event('click'));
      await flushTasks();

      // There is still 1 result left so it should still be visible.
      assertFalse(element.isEmpty);
      assertTrue(isVisible(element));
      assertEquals(1, getResultElements().length);

      // Open the 'more actions' menu for the last result and remove it.
      moreActionsIconButtons[0]!.dispatchEvent(new Event('click'));
      element.shadowRoot!
          .querySelector<HTMLElement>(
              '#removeFromHistoryOption')!.dispatchEvent(new Event('click'));
      await flushTasks();

      // No results left.
      assertTrue(element.isEmpty);
      assertFalse(isVisible(element));
    });

    test('SetsUserFeedback', async () => {
      const feedbackButtonsEl =
          element.shadowRoot!.querySelector('cr-feedback-buttons');
      assertTrue(!!feedbackButtonsEl);
      assertEquals(
          CrFeedbackOption.UNSPECIFIED, feedbackButtonsEl.selectedOption,
          'defaults to unspecified');

      function dispatchFeedbackOptionChange(option: CrFeedbackOption) {
        feedbackButtonsEl!.dispatchEvent(
            new CustomEvent('selected-option-changed', {
              bubbles: true,
              composed: true,
              detail: {value: option},
            }));
      }

      dispatchFeedbackOptionChange(CrFeedbackOption.THUMBS_DOWN);
      assertEquals(
          UserFeedback.kUserFeedbackNegative,
          await handler.whenCalled('setUserFeedback'));
      assertEquals(
          CrFeedbackOption.THUMBS_DOWN, feedbackButtonsEl.selectedOption);
      handler.reset();

      dispatchFeedbackOptionChange(CrFeedbackOption.THUMBS_UP);
      assertEquals(
          UserFeedback.kUserFeedbackPositive,
          await handler.whenCalled('setUserFeedback'));
      assertEquals(
          CrFeedbackOption.THUMBS_UP, feedbackButtonsEl.selectedOption);
      handler.reset();

      dispatchFeedbackOptionChange(CrFeedbackOption.UNSPECIFIED);
      assertEquals(
          UserFeedback.kUserFeedbackUnspecified,
          await handler.whenCalled('setUserFeedback'));
      assertEquals(
          CrFeedbackOption.UNSPECIFIED, feedbackButtonsEl.selectedOption);
      handler.reset();

      // Search again with new query.
      element.searchQuery = 'new query';

      await handler.whenCalled('search');
      await flushTasks();
      assertEquals(
          CrFeedbackOption.UNSPECIFIED, feedbackButtonsEl.selectedOption,
          'defaults back to unspecified when there is a new set of results');
    });

    test('SendsQualityLog', async () => {
      // Click on the second result.
      const resultsElements = getResultElements();
      resultsElements[1]!.dispatchEvent(new Event('click'));

      // Perform a new search, which should log the previous result.
      element.searchQuery = 'some new query';
      await handler.whenCalled('search');
      let [clickedIndices, numChars] =
          await handler.whenCalled('sendQualityLog');
      assertDeepEquals([1], clickedIndices);
      assertEquals(21, numChars);
      handler.resetResolver('sendQualityLog');

      // Override the minimum result age and ensure transient results are not
      // logged. Only after the 100ms passes and another search is performed
      // should the quality log be sent.
      element.overrideLoadingStateMinimumMsForTesting(50);
      element.overrideQueryResultMinAgeForTesting(100);
      element.numCharsForQuery = 25;
      element.searchQuery = 'some newer que';
      await handler.whenCalled('search');
      // Perform another query immediately and then wait 100ms. This will ensure
      // that this is the result set that the quality log is sent for.
      element.numCharsForQuery = 30;
      element.searchQuery = 'some newer query';
      await handler.whenCalled('search');
      await new Promise(resolve => setTimeout(resolve, 50));   // Loading state.
      await new Promise(resolve => setTimeout(resolve, 100));  // Result age.

      // A new query should now send quality log for the last result.
      element.numCharsForQuery = 50;
      element.searchQuery = 'some even newer query';
      await handler.whenCalled('search');
      assertEquals(1, handler.getCallCount('sendQualityLog'));

      // Updating the numCharsForQuery property should have no impact.
      element.numCharsForQuery = 90;

      [clickedIndices, numChars] = await handler.whenCalled('sendQualityLog');
      assertDeepEquals([], clickedIndices);
      assertEquals(30, numChars);
    });

    test('SendsQualityLogOnDisconnect', async () => {
      if (!enableAnswers) {
        return;
      }
      element.remove();
      const [clickedIndices, numChars] =
          await handler.whenCalled('sendQualityLog');
      assertDeepEquals([], clickedIndices);
      assertEquals(21, numChars);
    });

    test('SendsQualityLogOnBeforeUnload', async () => {
      if (!enableAnswers) {
        return;
      }
      window.dispatchEvent(new Event('beforeunload'));
      const [clickedIndices, numChars] =
          await handler.whenCalled('sendQualityLog');
      assertDeepEquals([], clickedIndices);
      assertEquals(21, numChars);
    });

    test('SendsQualityLogOnVisibilityChange', async () => {
      if (!enableAnswers) {
        return;
      }
      // Remove and re-add element to call connectedCallback again.
      element.remove();
      element.inSidePanel = true;
      document.body.appendChild(element);
      await flushTasks();

      Object.defineProperty(
          document, 'visibilityState', {value: 'hidden', writable: true});
      document.dispatchEvent(new CustomEvent('visibilitychange'));
      const [clickedIndices, numChars] =
          await handler.whenCalled('sendQualityLog');
      assertDeepEquals([], clickedIndices);
      assertEquals(21, numChars);
    });

    test('ForceFlushesQualityLogOnBeforeUnload', async () => {
      if (!enableAnswers) {
        return;
      }
      handler.resetResolver('sendQualityLog');
      // Make the min age really long so we can test a beforeunload happening
      // before results are considered 'stable'.
      element.overrideQueryResultMinAgeForTesting(100000);

      window.dispatchEvent(new Event('beforeunload'));

      // Log should immediately be sent without having to wait the 100000ms.
      assertEquals(1, handler.getCallCount('sendQualityLog'));
    });

    test('SendsQualityLogOnlyOnce', async () => {
      // Click on a couple of the results.
      const resultsElements = getResultElements();
      resultsElements[0]!.dispatchEvent(new Event('click'));
      resultsElements[1]!.dispatchEvent(new Event('click'));

      // Multiple events that can cause logs.
      element.searchQuery = 'some newer query';
      await handler.whenCalled('search');
      window.dispatchEvent(new Event('beforeunload'));
      element.remove();

      const [clickedIndices, numChars] =
          await handler.whenCalled('sendQualityLog');
      assertDeepEquals([0, 1], clickedIndices);
      assertEquals(21, numChars);
      assertEquals(1, handler.getCallCount('sendQualityLog'));
    });

    test('ForceSupressesLogging', async () => {
      element.forceSuppressLogging = true;
      const resultsElements = getResultElements();
      resultsElements[0]!.dispatchEvent(new Event('click'));
      window.dispatchEvent(new Event('beforeunload'));
      assertEquals(0, handler.getCallCount('sendQualityLog'));
    });

    test('RecordsMetrics', async () => {
      // Clicking on a result sends metrics for the click.
      const resultsElements = getResultElements();
      resultsElements[0]!.dispatchEvent(new Event('click'));
      assertDeepEquals(
          [
            /* nonEmptyResults= */ true,
            /* userClickedResult= */ true,
            /* answerShown= */ false,
            /* answerCitationClicked= */ false,
            /* otherHistoryResultClicked= */ false,
            /* queryWordCount= */ 2,
          ],
          await handler.whenCalled('recordSearchResultsMetrics'));

      async function resetAndUpdateResults(
          resultOverrides: Partial<SearchResult>) {
        handler.resetResolver('recordSearchResultsMetrics');
        const newQuery = 'query ' + Math.random();
        element.searchQuery = newQuery;
        element.searchResultChangedForTesting(Object.assign(
            {
              query: newQuery,
              answerStatus: AnswerStatus.kUnspecified,
              answer: '',
              items: [mockResults],
            },
            resultOverrides));
        return flushTasks();
      }

      // Empty results sends metrics.
      await resetAndUpdateResults({items: []});
      window.dispatchEvent(new Event('beforeunload'));  // Flush metrics.
      assertDeepEquals(
          [
            /* nonEmptyResults= */ false,
            /* userClickedResult= */ false,
            /* answerShown= */ false,
            /* answerCitationClicked= */ false,
            /* otherHistoryResultClicked= */ false,
            /* queryWordCount= */ 2,
          ],
          await handler.whenCalled('recordSearchResultsMetrics'),
          'Empty result set metrics are flushed.');

      if (!enableAnswers) {
        return;
      }

      // Result set with answer sends metrics.
      const resultWithAnswer = Object.assign({}, mockResults[0], {
        answerData: {answerTextDirectives: []},
      });
      await resetAndUpdateResults({
        answerStatus: AnswerStatus.kSuccess,
        answer: 'some answer',
        items: [resultWithAnswer, ...mockResults],
      });
      window.dispatchEvent(new Event('beforeunload'));  // Flush metrics.
      assertDeepEquals(
          [
            /* nonEmptyResults= */ true,
            /* userClickedResult= */ false,
            /* answerShown= */ true,
            /* answerCitationClicked= */ false,
            /* otherHistoryResultClicked= */ false,
            /* queryWordCount= */ 2,
          ],
          await handler.whenCalled('recordSearchResultsMetrics'),
          'Shown answers are recorded.');

      // Non-embedding result clicks are recorded.
      await resetAndUpdateResults({
        answerStatus: AnswerStatus.kSuccess,
        answer: 'some answer',
        items: [resultWithAnswer, ...mockResults],
      });
      element.otherHistoryResultClicked = true;
      window.dispatchEvent(new Event('beforeunload'));  // Flush metrics.
      assertDeepEquals(
          [
            /* nonEmptyResults= */ true,
            /* userClickedResult= */ false,
            /* answerShown= */ true,
            /* answerCitationClicked= */ false,
            /* otherHistoryResultClicked= */ true,
            /* queryWordCount= */ 2,
          ],
          await handler.whenCalled('recordSearchResultsMetrics'),
          'Non-embedding result clicked is recorded.');

      // Clicking answers is recorded.
      await resetAndUpdateResults({
        answerStatus: AnswerStatus.kSuccess,
        answer: 'some answer',
        items: [resultWithAnswer, ...mockResults],
      });
      element.otherHistoryResultClicked = false;
      const answerLink =
          element.shadowRoot!.querySelector<HTMLAnchorElement>('.answer-link');
      assertTrue(!!answerLink);
      answerLink.addEventListener('click', (e) => e.preventDefault());
      answerLink.click();
      window.dispatchEvent(new Event('beforeunload'));  // Flush metrics.
      assertDeepEquals(
          [
            /* nonEmptyResults= */ true,
            /* userClickedResult= */ false,
            /* answerShown= */ true,
            /* answerCitationClicked= */ true,
            /* otherHistoryResultClicked= */ false,
            /* queryWordCount= */ 2,
          ],
          await handler.whenCalled('recordSearchResultsMetrics'),
          'Clicking answers is recorded.');
    });

    test('ShowsAnswerSection', async () => {
      if (!enableAnswers) {
        return;
      }
      loadTimeData.overrideValues({
        historyEmbeddingsAnswerHeading: 'Answer section',
        historyEmbeddingsAnswerLoadingHeading: 'Loading answer...',
      });
      const answerSectionElement =
          element.shadowRoot!.querySelector<HTMLElement>('.answer-section');
      assertTrue(!!answerSectionElement);
      assertFalse(
          isVisible(answerSectionElement),
          'Answer section should be hidden because the state is unspecified.');

      element.searchResultChangedForTesting({
        query: 'some query',
        answerStatus: AnswerStatus.kLoading,
        answer: '',
        items: [...mockResults],
      });
      await flushTasks();
      assertTrue(
          isVisible(answerSectionElement),
          'Answer should be visible to show loading state.');
      const headingEl =
          answerSectionElement.querySelector<HTMLElement>('.heading');
      assertTrue(!!headingEl);
      assertEquals('Loading answer...', headingEl.innerText.trim());

      element.searchResultChangedForTesting({
        query: 'some query',
        answerStatus: AnswerStatus.kSuccess,
        answer: 'some answer',
        items: [...mockResults],
      });
      await flushTasks();
      assertTrue(
          isVisible(answerSectionElement),
          'Answer should be visible to show answer.');
      assertEquals('Answer section', headingEl.innerText.trim());

      element.searchQuery = 'new query';
      await flushTasks();
      assertFalse(
          isVisible(answerSectionElement),
          'A new query should hide the previous answer.');

      element.searchResultChangedForTesting({
        query: 'new query',
        answerStatus: AnswerStatus.kUnanswerable,
        answer: '',
        items: [...mockResults],
      });
      await flushTasks();
      assertFalse(isVisible(answerSectionElement));

      element.searchResultChangedForTesting({
        query: 'new query',
        answerStatus: AnswerStatus.kFiltered,
        answer: '',
        items: [...mockResults],
      });
      await flushTasks();
      assertFalse(isVisible(answerSectionElement));
    });

    test('ShowsAnswer', async () => {
      if (!enableAnswers) {
        return;
      }

      const answerElement =
          element.shadowRoot!.querySelector<HTMLElement>('.answer');
      assertTrue(!!answerElement);
      assertFalse(
          isVisible(answerElement),
          'Answer should not be visible since there is no answer yet.');

      element.searchResultChangedForTesting({
        query: 'some query',
        answerStatus: AnswerStatus.kSuccess,
        answer: 'some answer',
        items: [...mockResults],
      });
      await flushTasks();
      assertTrue(isVisible(answerElement));
      assertFalse(answerElement.hasAttribute('is-error'));
      assertEquals('some answer', answerElement!.innerText);
    });

    test('DisplaysAnswerSource', async () => {
      if (!enableAnswers) {
        return;
      }

      // Make the result at index 1 the result corresponding to the answer.
      const resultWithAnswer = {
        title: 'Website with answer',
        url: {url: 'http://answer.com'},
        urlForDisplay: 'Answer.com',
        relativeTime: '2 months ago',
        shortDateTime: 'Sept 2, 2022',
        sourcePassage: 'Answer description',
        lastUrlVisitTimestamp: 2000,
        answerData: {answerTextDirectives: []},
      };

      element.searchResultChangedForTesting({
        query: 'some query',
        answerStatus: AnswerStatus.kSuccess,
        answer: 'some answer',
        items: [...mockResults, resultWithAnswer],
      });
      await flushTasks();

      const answerSource = element.shadowRoot!.querySelector<HTMLAnchorElement>(
          '.answer-source');
      assertTrue(!!answerSource);
      assertFalse(answerSource.hidden);
      assertEquals(
          'http://answer.com/',
          answerSource.querySelector<HTMLAnchorElement>('.answer-link')!.href);

      assertTrue(
          answerSource.querySelector<HTMLElement>(
                          '.result-url')!.innerText.includes('Answer.com'));
      assertTrue(answerSource.querySelector<HTMLElement>(
                                 '.time')!.innerText.includes('Sept 2, 2022'));
      assertEquals(
          getFaviconForPageURL('http://answer.com', true),
          answerSource.querySelector<HTMLElement>(
                          '.favicon')!.style.backgroundImage);
    });

    test('GetsAnswerSourceUrl', async () => {
      if (!enableAnswers) {
        return;
      }

      function sendAnswerWithTextDirectives(directives: string[] = []) {
        const resultWithAnswer = {
          title: 'Website with answer',
          url: {url: 'http://answer.com'},
          urlForDisplay: 'Answer.com',
          relativeTime: '2 months ago',
          shortDateTime: 'Jan 2, 2022',
          sourcePassage: 'Answer description',
          lastUrlVisitTimestamp: 2000,
          answerData: {answerTextDirectives: directives},
        };

        element.searchResultChangedForTesting({
          query: 'some query',
          answerStatus: AnswerStatus.kSuccess,
          answer: 'some answer',
          items: [...mockResults, resultWithAnswer],
        });

        return flushTasks();
      }

      const answerLink =
          element.shadowRoot!.querySelector<HTMLAnchorElement>('.answer-link');
      assertTrue(!!answerLink);
      await sendAnswerWithTextDirectives([]);
      assertEquals('http://answer.com/', answerLink.href);

      await sendAnswerWithTextDirectives(['start text']);
      assertEquals(
          'http://answer.com/#:~:text=start%20text', answerLink.href,
          'should generate a link using the first directive');

      await sendAnswerWithTextDirectives(['another start text', 'end text']);
      assertEquals(
          'http://answer.com/#:~:text=another%20start%20text', answerLink.href,
          'should ignore other directives');
    });

    test('DisplaysFavicons', async () => {
      if (!enableAnswers) {
        // Favicons for without answers is embedded in a separate component.
        return;
      }

      const favicons = element.shadowRoot!.querySelectorAll<HTMLElement>(
          '.result-item .favicon');
      assertEquals(2, favicons.length);
      assertEquals(
          getFaviconForPageURL(mockResults[0]!.url.url, true),
          favicons[0]!.style.backgroundImage);
      assertEquals(
          getFaviconForPageURL(mockResults[1]!.url.url, true),
          favicons[1]!.style.backgroundImage);
    });

    test('DisplaysAnswererErrors', async () => {
      if (!enableAnswers) {
        return;
      }

      loadTimeData.overrideValues({
        historyEmbeddingsAnswererErrorModelUnavailable: 'model not available',
        historyEmbeddingsAnswererErrorTryAgain: 'try again',
      });

      async function updateAnswerStatus(status: AnswerStatus) {
        element.searchResultChangedForTesting({
          query: 'some query',
          answer: '',
          answerStatus: status,
          items: [...mockResults],
        });
        return flushTasks();
      }

      await updateAnswerStatus(AnswerStatus.kExecutionFailure);
      const errorEl = element.shadowRoot!.querySelector<HTMLElement>('.answer');
      assertTrue(!!errorEl);
      assertTrue(isVisible(errorEl));
      assertTrue(errorEl.hasAttribute('is-error'));
      assertEquals('try again', errorEl.innerText);
    });
  });
});
