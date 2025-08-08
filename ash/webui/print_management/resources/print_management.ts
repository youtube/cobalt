// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import './print_job_clear_history_dialog.js';
import './print_job_entry.js';
import './print_management_fonts.css.js';
import './print_management_shared.css.js';
import './printer_setup_info.js';
import './strings.m.js';

import {IronIconElement} from '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getMetadataProvider, getPrintManagementHandler} from './mojo_interface_provider.js';
import {getTemplate} from './print_management.html.js';
import {ActivePrintJobState, LaunchSource, PrintingMetadataProviderInterface, PrintJobInfo, PrintJobsObserverInterface, PrintJobsObserverReceiver, PrintManagementHandlerInterface} from './printing_manager.mojom-webui.js';

const METADATA_STORED_INDEFINITELY = -1;
const METADATA_STORED_FOR_ONE_DAY = 1;
const METADATA_NOT_STORED = 0;

type RemovePrintJobEvent = CustomEvent<string>;

declare global {
  interface HTMLElementEventMap {
    'remove-print-job': RemovePrintJobEvent;
    'all-history-cleared': CustomEvent<void>;
  }
}

function comparePrintJobsReverseChronologically(
    first: PrintJobInfo, second: PrintJobInfo): number {
  return -comparePrintJobsChronologically(first, second);
}

function comparePrintJobsChronologically(
    first: PrintJobInfo, second: PrintJobInfo): number {
  return Number(first.creationTime.internalValue) -
      Number(second.creationTime.internalValue);
}

/**
 * @fileoverview
 * 'print-management' is used as the main app to display print jobs.
 */

const PrintManagementElementBase = I18nMixin(PolymerElement);

export interface PrintManagementElement {
  $: {deleteIcon: IronIconElement};
}

export class PrintManagementElement extends PrintManagementElementBase
    implements PrintJobsObserverInterface {
  static get is(): string {
    return 'print-management';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      printJobs: {
        type: Array,
        value: () => [],
      },

      printJobHistoryExpirationPeriod: {
        type: String,
        value: '',
      },

      activeHistoryInfoIcon: {
        type: String,
        value: '',
      },

      isPolicyControlled: {
        type: Boolean,
        value: false,
      },

      ongoingPrintJobs: {
        type: Array,
        value: () => [],
      },

      // Used by FocusRowBehavior to track the last focused element on a row.
      lastFocused: Object,

      // Used by FocusRowBehavior to track if the list has been blurred.
      listBlurred: Boolean,

      showClearAllButton: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      showClearAllDialog: {
        type: Boolean,
        value: false,
      },

      showSetupAssistance: {
        type: Boolean,
        value: (): boolean => {
          return loadTimeData.getBoolean('isSetupAssistanceEnabled');
        },
      },

      deletePrintJobHistoryAllowedByPolicy: {
        type: Boolean,
        value: true,
      },

      shouldDisableClearAllButton: {
        type: Boolean,
        computed: 'computeShouldDisableClearAllButton(printJobs,' +
            'deletePrintJobHistoryAllowedByPolicy)',
      },

      /**
       * Receiver responsible for observing print job updates notification
       * events.
       */
      printJobsObserverReceiver: {type: Object},
    };
  }

  static get observers(): string[] {
    return ['onClearAllButtonUpdated(shouldDisableClearAllButton)'];
  }

  constructor() {
    super();

    this.mojoInterfaceProvider = getMetadataProvider();
    this.pageHandler = getPrintManagementHandler();

    window.CrPolicyStrings = {
      controlledSettingPolicy:
          loadTimeData.getString('clearAllPrintJobPolicyIndicatorToolTip'),
    };

    this.addEventListener('all-history-cleared', () => this.getPrintJobs());
    this.addEventListener('remove-print-job', (e) => this.removePrintJob(e));
  }

  private mojoInterfaceProvider: PrintingMetadataProviderInterface;
  private pageHandler: PrintManagementHandlerInterface;
  private isPolicyControlled: boolean;
  private printJobs: PrintJobInfo[];
  private printJobHistoryExpirationPeriod: string;
  private activeHistoryInfoIcon: string;
  private ongoingPrintJobs: PrintJobInfo[];
  private lastFocused: Element;
  private listBlurred: boolean;
  private showClearAllButton: boolean;
  private showClearAllDialog: boolean;
  private showSetupAssistance: boolean;
  private deletePrintJobHistoryAllowedByPolicy: boolean;
  private shouldDisableClearAllButton: boolean;
  private printJobsObserverReceiver: PrintJobsObserverReceiver;

  override connectedCallback(): void {
    super.connectedCallback();

    this.getPrintJobHistoryExpirationPeriod();
    this.startObservingPrintJobs();
    this.fetchDeletePrintJobHistoryPolicy();

    if (loadTimeData.getBoolean('isJellyEnabledForPrintManagement')) {
      // TODO(b/276493795): After the Jelly experiment is launched, replace
      // `cros_styles.css` with `theme/colors.css` directly in `index.html`.
      // Also add `theme/typography.css` to `index.html`.
      document.querySelector('link[href*=\'cros_styles.css\']')
          ?.setAttribute('href', 'chrome://theme/colors.css?sets=legacy,sys');
      const typographyLink = document.createElement('link');
      typographyLink.href = 'chrome://theme/typography.css';
      typographyLink.rel = 'stylesheet';
      document.head.appendChild(typographyLink);
      document.body.classList.add('jelly-enabled');
      ColorChangeUpdater.forDocument().start();
    }
  }

  override disconnectedCallback(): void {
    super.disconnectedCallback();

    this.printJobsObserverReceiver.$.close();
  }

  private startObservingPrintJobs(): void {
    this.printJobsObserverReceiver = new PrintJobsObserverReceiver((this));
    this.mojoInterfaceProvider
        .observePrintJobs(
            this.printJobsObserverReceiver.$.bindNewPipeAndPassRemote())
        .then(() => {
          this.getPrintJobs();
        });
  }

  private fetchDeletePrintJobHistoryPolicy(): void {
    this.mojoInterfaceProvider.getDeletePrintJobHistoryAllowedByPolicy().then(
        (param) => {
          this.onGetDeletePrintHistoryPolicy(param);
        });
  }

  private onGetDeletePrintHistoryPolicy(responseParam: {
    isAllowedByPolicy: boolean,
  }): void {
    this.showClearAllButton = true;
    this.deletePrintJobHistoryAllowedByPolicy = responseParam.isAllowedByPolicy;
  }

  onAllPrintJobsDeleted(): void {
    this.getPrintJobs();
  }

  onPrintJobUpdate(job: PrintJobInfo): void {
    // Only update ongoing print jobs.
    assert(job.activePrintJobInfo);

    // Check if |job| is an existing ongoing print job and requires an update
    // or if |job| is a new ongoing print job.
    const idx = this.getIndexOfOngoingPrintJob(job.id);
    if (idx !== -1) {
      // Replace the existing ongoing print job with its updated entry.
      this.splice('ongoingPrintJobs', idx, 1, job);
    } else {
      // New ongoing print jobs are appended to the ongoing print
      // jobs list.
      this.push('ongoingPrintJobs', job);
    }

    if (job.activePrintJobInfo?.activeState ===
        ActivePrintJobState.kDocumentDone) {
      // This print job is now completed, next step is to update the history
      // list with the recently stored print job.
      this.getPrintJobs();
    }
  }

  private onPrintJobsReceived(jobs: {printJobs: PrintJobInfo[]}): void {
    // TODO(crbug/1073690): Update this when BigInt is supported for
    // updateList().
    const ongoingList = [];
    const historyList = [];
    for (const job of jobs.printJobs) {
      // activePrintJobInfo is not null for ongoing print jobs.
      if (job.activePrintJobInfo) {
        ongoingList.push(job);
      } else {
        historyList.push(job);
      }
    }

    // Sort the print jobs in chronological order.
    this.ongoingPrintJobs = ongoingList.sort(comparePrintJobsChronologically);
    this.printJobs = historyList.sort(comparePrintJobsReverseChronologically);
  }

  private getPrintJobs(): void {
    this.mojoInterfaceProvider.getPrintJobs().then(
        this.onPrintJobsReceived.bind(this));
  }

  private onPrintJobHistoryExpirationPeriodReceived(printJobPolicyInfo: {
    expirationPeriodInDays: number,
    isFromPolicy: boolean,
  }): void {
    const expirationPeriod = printJobPolicyInfo.expirationPeriodInDays;
    // If print jobs are not persisted, we can return early since the tooltip
    // section won't be shown.
    if (expirationPeriod === METADATA_NOT_STORED) {
      return;
    }

    this.isPolicyControlled = printJobPolicyInfo.isFromPolicy;
    this.activeHistoryInfoIcon =
        this.isPolicyControlled ? 'enterpriseIcon' : 'infoIcon';

    switch (expirationPeriod) {
      case METADATA_STORED_INDEFINITELY:
        this.printJobHistoryExpirationPeriod =
            loadTimeData.getString('printJobHistoryIndefinitePeriod');
        break;
      case METADATA_STORED_FOR_ONE_DAY:
        this.printJobHistoryExpirationPeriod =
            loadTimeData.getString('printJobHistorySingleDay');
        break;
      default:
        this.printJobHistoryExpirationPeriod = loadTimeData.getStringF(
            'printJobHistoryExpirationPeriod',
            expirationPeriod,
        );
    }
  }

  private getPrintJobHistoryExpirationPeriod(): void {
    this.mojoInterfaceProvider.getPrintJobHistoryExpirationPeriod().then(
        this.onPrintJobHistoryExpirationPeriodReceived.bind(this));
  }

  private removePrintJob(e: RemovePrintJobEvent): void {
    const idx = this.getIndexOfOngoingPrintJob(e.detail);
    if (idx !== -1) {
      this.splice('ongoingPrintJobs', idx, 1);
    }
  }

  private onClearHistoryClicked(): void {
    this.showClearAllDialog = true;
  }

  private onClearHistoryDialogClosed(): void {
    this.showClearAllDialog = false;
  }

  private getIndexOfOngoingPrintJob(expectedId: string): number {
    return this.ongoingPrintJobs.findIndex(
        arrJob => arrJob.id === expectedId,
    );
  }

  private computeShouldDisableClearAllButton(): boolean {
    return !this.deletePrintJobHistoryAllowedByPolicy || !this.printJobs.length;
  }

  private onClearAllButtonUpdated(): void {
    this.$.deleteIcon.classList.toggle(
        'delete-enabled', !this.shouldDisableClearAllButton);
    this.$.deleteIcon.classList.toggle(
        'delete-disabled', this.shouldDisableClearAllButton);
  }

  /** Determine if printer setup UI should be shown. */
  private shouldShowSetupAssistance(): boolean {
    return this.showSetupAssistance && this.ongoingPrintJobs.length === 0 &&
        this.printJobs.length === 0;
  }

  /** Determine if ongoing jobs empty messaging should be shown. */
  private shouldShowOngoingEmptyState(): boolean {
    return !this.shouldShowSetupAssistance() &&
        this.ongoingPrintJobs.length === 0;
  }

  /** Determine if manage printer button in header should be shown. */
  private shouldShowManagePrinterButton(): boolean {
    return this.showSetupAssistance &&
        (this.ongoingPrintJobs.length > 0 || this.printJobs.length > 0);
  }

  private onManagePrintersClicked(): void {
    this.pageHandler.launchPrinterSettings(LaunchSource.kHeaderButton);
  }
}

customElements.define(PrintManagementElement.is, PrintManagementElement);
