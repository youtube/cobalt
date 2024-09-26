// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_tab_box/cr_tab_box.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './app.html.js';
import {KeyValue, Log, LogData, MetricsInternalsBrowserProxy, MetricsInternalsBrowserProxyImpl} from './browser_proxy.js';
import {getEventsPeekString, logEventToString, sizeToString, timestampToString, umaLogTypeToString} from './log_utils.js';

/**
 * An empty log. It is appended to a logs table when there are no logs (for
 * purely aesthetic reasons).
 */
const EMPTY_LOG: Log = {
  type: 'N/A',
  hash: 'N/A',
  timestamp: '',
  size: -1,
  events: [],
};

export class MetricsInternalsAppElement extends CustomElement {
  static get is(): string {
    return 'metrics-internals-app';
  }

  static override get template() {
    return getTemplate();
  }

  /**
   * Resolves once the component has finished loading.
   */
  initPromise: Promise<void>;

  private browserProxy_: MetricsInternalsBrowserProxy =
      MetricsInternalsBrowserProxyImpl.getInstance();

  /**
   * Previous summary tables data. Used to prevent re-renderings of the tables
   * when the data has not changed.
   */
  private previousVariationsSummaryData_: string = '';
  private previousUmaSummaryData_: string = '';

  constructor() {
    super();
    this.initPromise = this.init_();
  }

  /**
   * Returns UMA logs data (with their proto) as a JSON string. Used when
   * exporting UMA logs data. Returns a promise.
   */
  getUmaLogsExportContent(): Promise<string> {
    return this.browserProxy_.getUmaLogData(/*includeLogProtoData*/ true);
  }

  private async init_(): Promise<void> {
    // Fetch variations summary data and set up a recurring timer.
    await this.updateVariationsSummary_();
    setInterval(() => this.updateVariationsSummary_(), 3000);

    // Fetch UMA summary data and set up a recurring timer.
    await this.updateUmaSummary_();
    setInterval(() => this.updateUmaSummary_(), 3000);

    // Set up the UMA table caption.
    const umaTableCaption = this.$('#uma-table-caption') as HTMLElement;
    const isUsingMetricsServiceObserver =
        await this.browserProxy_.isUsingMetricsServiceObserver();
    umaTableCaption.textContent = isUsingMetricsServiceObserver ?
        'List of all UMA logs closed since browser startup.' :
        'List of UMA logs closed since opening this page. Starting the browser \
        with the --export-uma-logs-to-file command line flag will instead show \
        all logs closed since browser startup.';

    // Set up a listener for UMA logs. Also update UMA log data immediately in
    // case there are logs that we already have data on.
    addWebUiListener(
        'uma-log-created-or-event', () => this.updateUmaLogsData_());
    await this.updateUmaLogsData_();

    // Set up the UMA "Export logs" button.
    const exportUmaLogsButton = this.$('#export-uma-logs') as HTMLElement;
    exportUmaLogsButton.addEventListener('click', () => this.exportUmaLogs_());
  }

  /**
   * Callback function to expand/collapse an element on click.
   * @param e The click event.
   */
  private toggleEventsExpand_(e: MouseEvent): void {
    let umaLogEventsDiv = e.target as HTMLElement;

    // It is possible we have clicked a descendant. Keep checking the parent
    // until we are the the root div of the events.
    while (!umaLogEventsDiv.classList.contains('uma-log-events')) {
      umaLogEventsDiv = umaLogEventsDiv.parentElement as HTMLElement;
    }
    umaLogEventsDiv.classList.toggle('uma-log-events-expanded');
  }

  /**
   * Fills the passed table element with the given summary.
   */
  private updateSummaryTable_(tableBody: HTMLElement, summary: KeyValue[]):
      void {
    // Clear the table first.
    tableBody.replaceChildren();

    const template = this.$('#summary-row-template') as HTMLTemplateElement;
    for (const info of summary) {
      const row = template.content.cloneNode(true) as HTMLElement;
      const [key, value] = row.querySelectorAll('td');

      assert(key);
      key.textContent = info.key;

      assert(value);
      value.textContent = info.value;

      tableBody.appendChild(row);
    }
  }

  /**
   * Fetches variations summary data and updates the view.
   */
  private async updateVariationsSummary_(): Promise<void> {
    const summary: KeyValue[] =
        await this.browserProxy_.fetchVariationsSummary();
    const variationsSummaryTableBody =
        this.$('#variations-summary-body') as HTMLElement;

    // Don't re-render the table if the data has not changed.
    const newDataString = summary.toString();
    if (newDataString === this.previousVariationsSummaryData_) {
      return;
    }

    this.previousVariationsSummaryData_ = newDataString;
    this.updateSummaryTable_(variationsSummaryTableBody, summary);
  }

  /**
   * Fetches UMA summary data and updates the view.
   */
  private async updateUmaSummary_(): Promise<void> {
    const summary: KeyValue[] = await this.browserProxy_.fetchUmaSummary();
    const umaSummaryTableBody = this.$('#uma-summary-body') as HTMLElement;

    // Don't re-render the table if the data has not changed.
    const newDataString = summary.toString();
    if (newDataString === this.previousUmaSummaryData_) {
      return;
    }

    this.previousUmaSummaryData_ = newDataString;
    this.updateSummaryTable_(umaSummaryTableBody, summary);
  }

  /**
   * Fills the passed table element with the given logs.
   */
  private updateLogsTable_(tableBody: HTMLElement, logs: Log[]): void {
    // Clear the table first.
    tableBody.replaceChildren();

    const template = this.$('#uma-log-row-template') as HTMLTemplateElement;

    // Iterate through the logs in reverse order so that the most recent log
    // shows up first.
    for (const log of logs.slice(0).reverse()) {
      const row = template.content.cloneNode(true) as HTMLElement;
      const [type, hash, timestamp, size, events] = row.querySelectorAll('td');

      assert(type);
      type.textContent = umaLogTypeToString(log.type);

      assert(hash);
      hash.textContent = log.hash;

      assert(timestamp);
      timestamp.textContent = timestampToString(log.timestamp);

      assert(size);
      size.textContent = sizeToString(log.size);

      assert(events);
      const eventsPeekDiv =
          events.querySelector<HTMLElement>('.uma-log-events-peek');
      assert(eventsPeekDiv);
      eventsPeekDiv.addEventListener('click', this.toggleEventsExpand_, false);
      const eventsPeekText =
          events.querySelector<HTMLElement>('.uma-log-events-peek-text');
      assert(eventsPeekText);
      eventsPeekText.textContent = getEventsPeekString(log.events);
      const eventsText =
          events.querySelector<HTMLElement>('.uma-log-events-text');
      assert(eventsText);
      // Iterate through the events in reverse order so that the most recent
      // event shows up first.
      for (const event of log.events.slice(0).reverse()) {
        const div = document.createElement('div');
        div.textContent = logEventToString(event);
        eventsText.appendChild(div);
      }

      tableBody.appendChild(row);
    }
  }

  /**
   * Fetches the latest UMA logs and renders them. This is called when the page
   * is loaded and whenever there is a log that created or changed.
   */
  private async updateUmaLogsData_(): Promise<void> {
    const logsData: string =
        await this.browserProxy_.getUmaLogData(/*includeLogProtoData=*/ false);
    const logs: LogData = JSON.parse(logsData);
    // If there are no logs, append an empty log. This is purely for aesthetic
    // reasons. Otherwise, the table may look confusing.
    if (!logs.logs.length) {
      logs.logs = [EMPTY_LOG];
    }

    // We don't compare the new data with the old data to prevent re-renderings
    // because this should only be called when there is an actual change.

    const umaLogsTableBody = this.$('#uma-logs-body') as HTMLElement;
    this.updateLogsTable_(umaLogsTableBody, logs.logs);
  }

  /**
   * Exports the accumulated UMA logs, including their proto data, as a JSON
   * file. This will initiate a download.
   */
  private async exportUmaLogs_(): Promise<void> {
    const logsData: string = await this.getUmaLogsExportContent();
    const file = new Blob([logsData], {type: 'text/plain'});
    const a = document.createElement('a');
    a.href = URL.createObjectURL(file);
    a.download = `uma_logs_${new Date().getTime()}.json`;
    a.click();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'metrics-internals-app': MetricsInternalsAppElement;
  }
}

customElements.define(
    MetricsInternalsAppElement.is, MetricsInternalsAppElement);
