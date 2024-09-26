// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://print-management/print_management.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {IronIconElement} from '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {setMetadataProviderForTesting} from 'chrome://print-management/mojo_interface_provider.js';
import {PrintJobEntryElement} from 'chrome://print-management/print_job_entry.js';
import {PrintManagementElement} from 'chrome://print-management/print_management.js';
import {ActivePrintJobInfo, ActivePrintJobState, CompletedPrintJobInfo, PrinterErrorCode, PrintingMetadataProviderInterface, PrintJobCompletionStatus, PrintJobInfo, PrintJobsObserverRemote} from 'chrome://print-management/printing_manager.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

export function initPrintJobEntryElement(): PrintJobEntryElement {
  const element = document.createElement('print-job-entry');
  document.body.appendChild(element);
  flush();
  return element;
}

/**
 * Tear down an element. Remove from dom and call |flushTasks| to finish any
 * async cleanup in polymer and execute pending promises.
 */
export async function teardownElement(element: HTMLElement|null) {
  if (!element) {
    return;
  }
  element.remove();
  await flushTasks();
}

// Returns the match for |selector| in |element|'s shadow DOM.
function querySelector<E extends Element>(
    element: Element, selector: string): E|null {
  if (!element) {
    return null;
  }
  if (!element.shadowRoot) {
    return null;
  }

  return element.shadowRoot.querySelector<E>(selector);
}

// Converts a JS string to mojo_base::mojom::String16 object.
function strToMojoString16(str: string): {data: number[]} {
  const arr = [];
  for (let i = 0; i < str.length; i++) {
    arr[i] = str.charCodeAt(i);
  }
  return {data: arr};
}

/**
 * Converts a JS time (milliseconds since UNIX epoch) to mojom::time
 * (microseconds since WINDOWS epoch).
 */
function convertToMojoTime(jsDate: Date): number {
  const windowsEpoch = new Date(Date.UTC(1601, 0, 1, 0, 0, 0));
  const jsEpoch = new Date(Date.UTC(1970, 0, 1, 0, 0, 0));
  return ((jsEpoch.getTime() - windowsEpoch.getTime()) * 1000) +
      (jsDate.getTime() * 1000);
}

// Converts utf16 to a readable string.
function decodeString16(arr: {data: number[]}): string {
  return arr.data.map(ch => String.fromCodePoint(ch)).join('');
}

function createJobEntry(
    id: string, title: string, date: number, printerErrorCode: number,
    completedInfo?: CompletedPrintJobInfo,
    activeInfo?: ActivePrintJobInfo): PrintJobInfo {
  // Assert that only one of either |completedInfo| or |activeInfo| is non-null.
  assertTrue(completedInfo ? !activeInfo : !!activeInfo);

  const jobEntry: PrintJobInfo = {
    'id': id,
    'title': strToMojoString16(title),
    'creationTime': {internalValue: BigInt(date)},
    'printerId': 'printerId',
    'printerName': strToMojoString16('printerName'),
    'printerUri': {url: '192.168.1.1'},
    'numberOfPages': 4,
    'printerErrorCode': printerErrorCode,
    'completedInfo': undefined,
    'activePrintJobInfo': undefined,
  };

  if (completedInfo) {
    jobEntry.completedInfo = completedInfo;
  } else {
    jobEntry.activePrintJobInfo = activeInfo;
  }
  return jobEntry;
}

function createCompletedPrintJobInfo(completionStatus: number):
    CompletedPrintJobInfo {
  const completedInfo = {'completionStatus': completionStatus};
  return completedInfo;
}

function createOngoingPrintJobInfo(
    printedPages: number,
    activeState: ActivePrintJobState): ActivePrintJobInfo {
  const activeInfo = {
    'printedPages': printedPages,
    'activeState': activeState,
  };
  return activeInfo;
}

function verifyPrintJobs(
    expected: PrintJobInfo[], actual: PrintJobEntryElement[]) {
  assertEquals(expected.length, actual.length);
  for (let i = 0; i < expected.length; i++) {
    const actualJobInfo = actual[i]!.jobEntry;
    const expectedJob = expected[i]!;
    assertEquals(expectedJob.id, actualJobInfo.id);
    assertEquals(
        decodeString16(expectedJob.title), decodeString16(actualJobInfo.title));
    assertEquals(
        Number(expectedJob.creationTime.internalValue),
        Number(actualJobInfo.creationTime.internalValue));
    assertEquals(expectedJob.printerId, actualJobInfo.printerId);
    assertEquals(
        decodeString16(expectedJob.printerName),
        decodeString16(actualJobInfo.printerName));
    assertEquals(expectedJob.printerErrorCode, actualJobInfo.printerErrorCode);

    if (actualJobInfo.completedInfo) {
      assertEquals(
          expectedJob.completedInfo?.completionStatus,
          actualJobInfo.completedInfo.completionStatus);
    } else {
      assertEquals(
          expectedJob.activePrintJobInfo?.printedPages,
          actualJobInfo.activePrintJobInfo?.printedPages);
      assertEquals(
          expectedJob.activePrintJobInfo?.activeState,
          actualJobInfo.activePrintJobInfo?.activeState);
    }
  }
}

function getHistoryPrintJobEntries(page: HTMLElement): PrintJobEntryElement[] {
  const entryList = querySelector(page, '#entryList')!;
  return Array.from(
      entryList.querySelectorAll('print-job-entry:not([hidden])'));
}

function getOngoingPrintJobEntries(page: HTMLElement): PrintJobEntryElement[] {
  assertTrue(!!querySelector<HTMLElement>(page, '#ongoingEmptyState')?.hidden);
  const entryList = querySelector(page, '#ongoingList')!;
  return Array.from(
      entryList.querySelectorAll('print-job-entry:not([hidden])'));
}

class FakePrintingMetadataProvider implements
    PrintingMetadataProviderInterface {
  private resolverMap_ = new Map();

  private printJobs_: PrintJobInfo[] = [];

  private shouldAttemptCancel_ = true;
  private isAllowedByPolicy_ = true;

  private printJobsObserverRemote_: PrintJobsObserverRemote|null = null;

  private expirationPeriod_ = 90;

  constructor() {
    this.resetForTest();
  }

  resetForTest() {
    this.printJobs_ = [];
    this.shouldAttemptCancel_ = true;
    this.isAllowedByPolicy_ = true;
    if (this.printJobsObserverRemote_) {
      this.printJobsObserverRemote_ = null;
    }

    this.resolverMap_.set('getPrintJobs', new PromiseResolver());
    this.resolverMap_.set('deleteAllPrintJobs', new PromiseResolver());
    this.resolverMap_.set('observePrintJobs', new PromiseResolver());
    this.resolverMap_.set('cancelPrintJob', new PromiseResolver());
    this.resolverMap_.set(
        'getDeletePrintJobHistoryAllowedByPolicy', new PromiseResolver());
    this.resolverMap_.set(
        'getPrintJobHistoryExpirationPeriod', new PromiseResolver());
  }

  private getResolver_(methodName: string): PromiseResolver<void> {
    const method = this.resolverMap_.get(methodName);
    assertTrue(!!method, `Method '${methodName}' not found.`);
    return method;
  }

  protected methodCalled(methodName: string) {
    this.getResolver_(methodName).resolve();
  }

  whenCalled(methodName: string): Promise<any> {
    return this.getResolver_(methodName).promise.then(() => {
      // Support sequential calls to whenCalled by replacing the promise.
      this.resolverMap_.set(methodName, new PromiseResolver());
    });
  }

  getObserverRemote(): PrintJobsObserverRemote {
    return this.printJobsObserverRemote_!;
  }

  setPrintJobs(printJobs: PrintJobInfo[]) {
    this.printJobs_ = printJobs;
  }

  setExpirationPeriod(expirationPeriod: number) {
    this.expirationPeriod_ = expirationPeriod;
  }

  setShouldAttemptCancel(shouldAttemptCancel: boolean) {
    this.shouldAttemptCancel_ = shouldAttemptCancel;
  }

  setDeletePrintJobPolicy(isAllowedByPolicy: boolean) {
    this.isAllowedByPolicy_ = isAllowedByPolicy;
  }

  addPrintJob(job: PrintJobInfo) {
    this.printJobs_ = this.printJobs_.concat(job);
  }

  simulatePrintJobsDeletedfromDatabase() {
    this.printJobs_ = [];
    this.printJobsObserverRemote_!.onAllPrintJobsDeleted();
  }

  simulateUpdatePrintJob(job: PrintJobInfo) {
    if (job.activePrintJobInfo?.activeState ===
        ActivePrintJobState.kDocumentDone) {
      // Create copy of |job| to modify.
      const updatedJob = Object.assign({}, job);
      updatedJob.activePrintJobInfo = undefined;
      updatedJob.completedInfo =
          createCompletedPrintJobInfo(PrintJobCompletionStatus.kPrinted);
      // Replace with updated print job.
      const idx =
          this.printJobs_.findIndex(arrJob => arrJob.id === updatedJob.id);
      if (idx !== -1) {
        this.printJobs_.splice(idx, 1, updatedJob);
      }
    }
    this.printJobsObserverRemote_!.onPrintJobUpdate(job);
  }

  // printingMetadataProvider methods

  getPrintJobs(): Promise<{printJobs: PrintJobInfo[]}> {
    return new Promise(resolve => {
      this.methodCalled('getPrintJobs');
      resolve({printJobs: this.printJobs_ || []});
    });
  }

  deleteAllPrintJobs(): Promise<{success: boolean}> {
    return new Promise(resolve => {
      this.printJobs_ = [];
      this.methodCalled('deleteAllPrintJobs');
      resolve({success: true});
    });
  }

  getDeletePrintJobHistoryAllowedByPolicy():
      Promise<{isAllowedByPolicy: boolean}> {
    return new Promise(resolve => {
      this.methodCalled('getDeletePrintJobHistoryAllowedByPolicy');
      resolve({isAllowedByPolicy: this.isAllowedByPolicy_});
    });
  }

  getPrintJobHistoryExpirationPeriod():
      Promise<{expirationPeriodInDays: number, isFromPolicy: boolean}> {
    return new Promise(resolve => {
      this.methodCalled('getPrintJobHistoryExpirationPeriod');
      resolve({
        expirationPeriodInDays: this.expirationPeriod_,
        isFromPolicy: true,
      });
    });
  }

  cancelPrintJob(): Promise<{attemptedCancel: boolean}> {
    return new Promise(resolve => {
      this.methodCalled('cancelPrintJob');
      resolve({attemptedCancel: this.shouldAttemptCancel_});
    });
  }

  observePrintJobs(remote: PrintJobsObserverRemote): Promise<void> {
    return new Promise(resolve => {
      this.printJobsObserverRemote_ = remote;
      this.methodCalled('observePrintJobs');
      resolve();
    });
  }
}

suite('PrintManagementTest', () => {
  let page: PrintManagementElement|null = null;

  let mojoApi_: FakePrintingMetadataProvider;

  suiteSetup(() => {
    mojoApi_ = new FakePrintingMetadataProvider();
    setMetadataProviderForTesting(mojoApi_);
  });

  teardown(function() {
    mojoApi_.resetForTest();
    page?.remove();
    page = null;
  });

  function initializePrintManagementApp(printJobs: PrintJobInfo[]):
      Promise<any> {
    mojoApi_.setPrintJobs(printJobs);
    page = document.createElement('print-management') as PrintManagementElement;
    document.body.appendChild(page);
    assertTrue(!!page);
    flush();
    return mojoApi_.whenCalled('observePrintJobs');
  }

  function simulateCancelPrintJob(
      jobEntryElement: PrintJobEntryElement,
      mojoApi: FakePrintingMetadataProvider, shouldAttemptCancel: boolean,
      expectedHistoryList: PrintJobInfo[]): Promise<any> {
    mojoApi.setShouldAttemptCancel(shouldAttemptCancel);

    const cancelButton = querySelector<HTMLButtonElement>(
        jobEntryElement, '#cancelPrintJobButton')!;
    cancelButton.click();
    return mojoApi.whenCalled('cancelPrintJob').then(() => {
      // Create copy of |jobEntryElement.jobEntry| to modify.
      const updatedJob = Object.assign({}, jobEntryElement.jobEntry);
      updatedJob.activePrintJobInfo = createOngoingPrintJobInfo(
          /*printedPages=*/ 0, ActivePrintJobState.kDocumentDone);
      // Simulate print jobs cancelled notification update sent.
      mojoApi.getObserverRemote().onPrintJobUpdate(updatedJob);

      // Simulate print job database updated with the canceled print job.
      mojoApi.setPrintJobs(expectedHistoryList);
      return mojoApi.whenCalled('getPrintJobs');
    });
  }
  test('PrintJobHistoryExpirationPeriodOneDay', async () => {
    const completedInfo =
        createCompletedPrintJobInfo(PrintJobCompletionStatus.kPrinted);
    const expectedText = 'Print jobs older than 1 day will be removed';
    const expectedArr = [
      createJobEntry(
          'newest', 'titleA',
          convertToMojoTime(new Date(Date.UTC(2020, 3, 1, 1, 1, 1))),
          PrinterErrorCode.kNoError, completedInfo, /*activeInfo=*/ undefined),
    ];
    // Print job metadata will be stored for 1 day.
    mojoApi_.setExpirationPeriod(1);
    await initializePrintManagementApp(expectedArr.slice().reverse());
    await mojoApi_.whenCalled('getPrintJobs');
    await mojoApi_.whenCalled('getPrintJobHistoryExpirationPeriod');
    const historyInfoTooltip = querySelector(page!, 'paper-tooltip');
    assertEquals(expectedText, historyInfoTooltip?.textContent?.trim());
  });

  test('PrintJobHistoryExpirationPeriodDefault', async () => {
    const completedInfo =
        createCompletedPrintJobInfo(PrintJobCompletionStatus.kPrinted);
    const expectedText = 'Print jobs older than 90 days will be removed';
    const expectedArr = [
      createJobEntry(
          'newest', 'titleA',
          convertToMojoTime(new Date(Date.UTC(2020, 3, 1, 1, 1, 1))),
          PrinterErrorCode.kNoError, completedInfo, /*activeInfo=*/ undefined),
    ];

    // Print job metadata will be stored for 90 days which is the default
    // period when the policy is not controlled.
    mojoApi_.setExpirationPeriod(90);
    await initializePrintManagementApp(expectedArr.slice().reverse());
    await mojoApi_.whenCalled('getPrintJobs');
    flush();
    await mojoApi_.whenCalled('getPrintJobHistoryExpirationPeriod');
    const historyInfoTooltip = querySelector(page!, 'paper-tooltip');
    assertEquals(expectedText, historyInfoTooltip?.textContent?.trim());
  });

  test('PrintJobHistoryExpirationPeriodIndefinte', async () => {
    const completedInfo =
        createCompletedPrintJobInfo(PrintJobCompletionStatus.kPrinted);
    const expectedText = 'Print jobs will appear in history unless they are ' +
        'removed manually';
    const expectedArr = [
      createJobEntry(
          'newest', 'titleA',
          convertToMojoTime(new Date(Date.UTC(2020, 3, 1, 1, 1, 1))),
          PrinterErrorCode.kNoError, completedInfo, /*activeInfo=*/ undefined),
    ];

    // When this policy is set to a value of -1, the print jobs metadata is
    // stored indefinitely.
    mojoApi_.setExpirationPeriod(-1);
    await initializePrintManagementApp(expectedArr.slice().reverse());
    await mojoApi_.whenCalled('getPrintJobs');
    flush();
    await mojoApi_.whenCalled('getPrintJobHistoryExpirationPeriod');
    const historyInfoTooltip = querySelector(page!, 'paper-tooltip');
    assertEquals(expectedText, historyInfoTooltip?.textContent?.trim());
  });

  test('PrintJobHistoryExpirationPeriodNDays', async () => {
    const completedInfo =
        createCompletedPrintJobInfo(PrintJobCompletionStatus.kPrinted);
    const expectedText = 'Print jobs older than 4 days will be removed';
    const expectedArr = [
      createJobEntry(
          'newest', 'titleA',
          convertToMojoTime(new Date(Date.UTC(2020, 3, 1, 1, 1, 1))),
          PrinterErrorCode.kNoError, completedInfo, /*activeInfo=*/ undefined),
    ];

    // Print job metadata will be stored for 4 days.
    mojoApi_.setExpirationPeriod(4);
    await initializePrintManagementApp(expectedArr.slice().reverse());
    await mojoApi_.whenCalled('getPrintJobs');
    flush();
    await mojoApi_.whenCalled('getPrintJobHistoryExpirationPeriod');
    const historyInfoTooltip = querySelector(page!, 'paper-tooltip');
    assertEquals(expectedText, historyInfoTooltip?.textContent?.trim());
  });

  test('PrintHistoryListIsSortedReverseChronologically', async () => {
    const completedInfo =
        createCompletedPrintJobInfo(PrintJobCompletionStatus.kPrinted);
    const expectedArr = [
      createJobEntry(
          'newest', 'titleA',
          convertToMojoTime(new Date(Date.UTC(2020, 3, 1, 1, 1, 1))),
          PrinterErrorCode.kNoError, completedInfo, /*activeInfo=*/ undefined),
      createJobEntry(
          'middle', 'titleB',
          convertToMojoTime(new Date(Date.UTC(2020, 2, 1, 1, 1, 1))),
          PrinterErrorCode.kNoError, completedInfo, /*activeInfo=*/ undefined),
      createJobEntry(
          'oldest', 'titleC',
          convertToMojoTime(new Date(Date.UTC(2020, 1, 1, 1, 1, 1))),
          PrinterErrorCode.kNoError, completedInfo, /*activeInfo=*/ undefined),
    ];

    // Initialize with a reversed array of |expectedArr|, since we expect the
    // app to sort the list when it first loads. Since reverse() mutates the
    // original array, use a copy array to prevent mutating |expectedArr|.
    await initializePrintManagementApp(expectedArr.slice().reverse());
    await mojoApi_.whenCalled('getPrintJobs');
    flush();
    verifyPrintJobs(expectedArr, getHistoryPrintJobEntries(page!));
  });

  test('ClearAllButtonDisabledWhenNoPrintJobsSaved', async () => {
    // Initialize with no saved print jobs, expect the clear all button to be
    // disabled.
    await initializePrintManagementApp(/*printJobs=*/[]);
    await mojoApi_.whenCalled('getPrintJobs');
    flush();
    assertTrue(
        !!querySelector<HTMLButtonElement>(page!, '#clearAllButton')?.disabled);
    assertTrue(!querySelector(page!, '#policyIcon'));
  });

  test('ClearAllButtonDisabledByPolicy', async () => {
    const expectedArr = [createJobEntry(
        'newest', 'titleA',
        convertToMojoTime(new Date(Date.UTC(2020, 3, 1, 1, 1, 1))),
        PrinterErrorCode.kNoError,
        createCompletedPrintJobInfo(PrintJobCompletionStatus.kPrinted),
        /*activeInfo=*/ undefined)];
    // Set policy to prevent user from deleting history.
    mojoApi_.setDeletePrintJobPolicy(/*isAllowedByPolicy=*/ false);
    await initializePrintManagementApp(expectedArr);
    await mojoApi_.whenCalled('getPrintJobs');
    flush();
    assertTrue(
        !!querySelector<HTMLButtonElement>(page!, '#clearAllButton')?.disabled);
    assertTrue(!!querySelector(page!, '#policyIcon'));
  });

  test('ClearAllPrintHistory', async () => {
    const completedInfo =
        createCompletedPrintJobInfo(PrintJobCompletionStatus.kPrinted);
    const expectedArr = [
      createJobEntry(
          'fileA', 'titleA',
          convertToMojoTime(new Date(Date.parse('February 5, 2020 03:24:00'))),
          PrinterErrorCode.kNoError, completedInfo, /*activeInfo=*/ undefined),
      createJobEntry(
          'fileB', 'titleB',
          convertToMojoTime(new Date(Date.parse('February 5, 2020 03:24:00'))),
          PrinterErrorCode.kNoError, completedInfo, /*activeInfo=*/ undefined),
      createJobEntry(
          'fileC', 'titleC',
          convertToMojoTime(new Date(Date.parse('February 5, 2020 03:24:00'))),
          PrinterErrorCode.kNoError, completedInfo, /*activeInfo=*/ undefined),
    ];

    await initializePrintManagementApp(expectedArr);
    await mojoApi_.whenCalled('getPrintJobs');
    await mojoApi_.whenCalled('getDeletePrintJobHistoryAllowedByPolicy');
    flush();
    verifyPrintJobs(expectedArr, getHistoryPrintJobEntries(page!));

    // Click the clear all button.
    const button = querySelector<HTMLButtonElement>(page!, '#clearAllButton')!;
    button.click();
    flush();

    // Verify that the confirmation dialog shows up and click on the
    // confirmation button.
    const dialog = querySelector(page!, '#clearHistoryDialog');
    assertTrue(!!dialog);
    const dialogActionButton =
        querySelector<HTMLButtonElement>(dialog, '.action-button')!;
    assertTrue(!dialogActionButton.disabled);
    dialogActionButton.click();
    assertTrue(dialogActionButton.disabled);
    await mojoApi_.whenCalled('deleteAllPrintJobs');
    flush();

    // After clearing the history list, expect that the history list and
    // header are no longer
    assertTrue(!querySelector(page!, '#entryList'));
    assertTrue(!querySelector(page!, '#historyHeaderContainer'));
    assertTrue(
        !!querySelector<HTMLButtonElement>(page!, '#clearAllButton')?.disabled);
  });

  test('PrintJobDeletesFromObserver', async () => {
    const completedInfo =
        createCompletedPrintJobInfo(PrintJobCompletionStatus.kPrinted);
    const expectedArr = [
      createJobEntry(
          'fileC', 'titleC',
          convertToMojoTime(new Date(Date.parse('February 7, 2020 03:24:00'))),
          PrinterErrorCode.kNoError, completedInfo, /*activeInfo=*/ undefined),
      createJobEntry(
          'fileB', 'titleB',
          convertToMojoTime(new Date(Date.parse('February 6, 2020 03:24:00'))),
          PrinterErrorCode.kNoError, completedInfo, /*activeInfo=*/ undefined),
      createJobEntry(
          'fileA', 'titleA',
          convertToMojoTime(new Date(Date.parse('February 5, 2020 03:24:00'))),
          PrinterErrorCode.kNoError, completedInfo, /*activeInfo=*/ undefined),
    ];

    await initializePrintManagementApp(expectedArr);
    await mojoApi_.whenCalled('getPrintJobs');
    flush();
    verifyPrintJobs(expectedArr, getHistoryPrintJobEntries(page!));

    // Simulate observer call that signals all print jobs have been
    // deleted. Expect the UI to retrieve an empty list of print jobs.
    mojoApi_.simulatePrintJobsDeletedfromDatabase();
    await mojoApi_.whenCalled('getPrintJobs');
    flush();
    // After clearing the history list, expect that the history list and
    // header are no longer
    assertTrue(!querySelector(page!, '#entryList'));
    assertTrue(!querySelector(page!, '#historyHeaderContainer'));
    assertTrue(
        !!querySelector<HTMLButtonElement>(page!, '#clearAllButton')?.disabled);
  });

  test('HistoryHeaderIsHiddenWithEmptyPrintJobsInHistory', async () => {
    await initializePrintManagementApp(/*expectedArr=*/[]);
    await mojoApi_.whenCalled('getPrintJobs');
    flush();
    // Header should be not be rendered since no there are no completed
    // print jobs in the history.
    assertTrue(!querySelector(page!, '#historyHeaderContainer'));
  });

  test('LoadsOngoingPrintJob', async () => {
    const activeInfo1 = createOngoingPrintJobInfo(
        /*printedPages=*/ 0, ActivePrintJobState.kStarted);
    const activeInfo2 = createOngoingPrintJobInfo(
        /*printedPages=*/ 1, ActivePrintJobState.kStarted);
    const expectedArr = [
      createJobEntry(
          'fileA', 'titleA',
          convertToMojoTime(new Date(Date.parse('February 5, 2020 03:23:00'))),
          PrinterErrorCode.kNoError, /*completedInfo=*/ undefined, activeInfo1),
      createJobEntry(
          'fileB', 'titleB',
          convertToMojoTime(new Date(Date.parse('February 5, 2020 03:24:00'))),
          PrinterErrorCode.kNoError, /*completedInfo=*/ undefined, activeInfo2),
    ];

    await initializePrintManagementApp(expectedArr);
    await mojoApi_.whenCalled('getPrintJobs');
    flush();
    verifyPrintJobs(expectedArr, getOngoingPrintJobEntries(page!));
  });

  test('OngoingPrintJobUpdated', async () => {
    const expectedArr = [
      createJobEntry(
          'fileA', 'titleA',
          convertToMojoTime(new Date('February 5, 2020 03:24:00')),
          PrinterErrorCode.kNoError, /*completedInfo=*/ undefined,
          createOngoingPrintJobInfo(
              /*printedPages=*/ 0, ActivePrintJobState.kStarted)),
    ];

    const activeInfo2 = createOngoingPrintJobInfo(
        /*printedPages=*/ 1, ActivePrintJobState.kStarted);
    const expectedUpdatedArr = [
      createJobEntry(
          'fileA', 'titleA',
          convertToMojoTime(new Date(Date.parse('February 5, 2020 03:24:00'))),
          PrinterErrorCode.kNoError, /*completedInfo=*/ undefined, activeInfo2),
    ];

    await initializePrintManagementApp(expectedArr);
    await mojoApi_.whenCalled('getPrintJobs');
    flush();
    verifyPrintJobs(expectedArr, getOngoingPrintJobEntries(page!));
    mojoApi_.simulateUpdatePrintJob(expectedUpdatedArr[0]!);
    await flushTasks();
    flush();
    verifyPrintJobs(expectedUpdatedArr, getOngoingPrintJobEntries(page!));
  });

  test('OngoingPrintJobUpdatedToStopped', async () => {
    const expectedArr = [
      createJobEntry(
          'fileA', 'titleA',
          convertToMojoTime(new Date('February 5, 2020 03:24:00')),
          PrinterErrorCode.kNoError, /*completedInfo=*/ undefined,
          createOngoingPrintJobInfo(
              /*printedPages=*/ 0, ActivePrintJobState.kStarted)),
    ];

    const activeInfo2 = createOngoingPrintJobInfo(
        /*printedPages=*/ 0, ActivePrintJobState.kStarted);
    const expectedUpdatedArr = [
      createJobEntry(
          'fileA', 'titleA',
          convertToMojoTime(new Date('February 5, 2020 03:24:00')),
          PrinterErrorCode.kOutOfPaper, /*completedInfo=*/ undefined,
          activeInfo2),
    ];

    await initializePrintManagementApp(expectedArr);
    await mojoApi_.whenCalled('getPrintJobs');
    flush();
    verifyPrintJobs(expectedArr, getOngoingPrintJobEntries(page!));
    mojoApi_.simulateUpdatePrintJob(expectedUpdatedArr[0]!);
    await flushTasks();
    flush();
    verifyPrintJobs(expectedUpdatedArr, getOngoingPrintJobEntries(page!));
  });

  test('NewOngoingPrintJobsDetected', async () => {
    const initialJob = [createJobEntry(
        'fileA', 'titleA',
        convertToMojoTime(new Date('February 5, 2020 03:24:00')),
        PrinterErrorCode.kNoError, /*completedInfo=*/ undefined,
        createOngoingPrintJobInfo(
            /*printedPages=*/ 0, ActivePrintJobState.kStarted))];

    const newOngoingJob = createJobEntry(
        'fileB', 'titleB',
        convertToMojoTime(new Date(Date.parse('February 5, 2020 03:25:00'))),
        PrinterErrorCode.kNoError, /*completedInfo=*/ undefined,
        createOngoingPrintJobInfo(
            /*printedPages=*/ 1, ActivePrintJobState.kStarted));

    await initializePrintManagementApp(initialJob);
    await mojoApi_.whenCalled('getPrintJobs');
    flush();
    verifyPrintJobs(initialJob, getOngoingPrintJobEntries(page!));
    mojoApi_.simulateUpdatePrintJob(newOngoingJob);
    await flushTasks();
    flush();
    verifyPrintJobs(
        [initialJob[0]!, newOngoingJob], getOngoingPrintJobEntries(page!));
  });

  test('OngoingPrintJobCompletesAndUpdatesHistoryList', async () => {
    const id = 'fileA';
    const title = 'titleA';
    const date =
        convertToMojoTime(new Date(Date.parse('February 5, 2020 03:24:00')));

    const activeJob = createJobEntry(
        id, title, date, PrinterErrorCode.kNoError,
        /*completedInfo=*/ undefined,
        createOngoingPrintJobInfo(
            /*printedPages=*/ 0, ActivePrintJobState.kStarted));

    const expectedPrintJobArr = [createJobEntry(
        id, title, date, PrinterErrorCode.kNoError,
        createCompletedPrintJobInfo(PrintJobCompletionStatus.kPrinted),
        /*activeInfo=*/ undefined)];

    await initializePrintManagementApp([activeJob]);
    await mojoApi_.whenCalled('getPrintJobs');
    flush();
    verifyPrintJobs([activeJob], getOngoingPrintJobEntries(page!));

    // Simulate ongoing print job has completed.
    activeJob.activePrintJobInfo!.activeState =
        ActivePrintJobState.kDocumentDone;
    mojoApi_.simulateUpdatePrintJob(activeJob);

    // Simulate print job has been added to history.
    await mojoApi_.whenCalled('getPrintJobs');
    flush();
    verifyPrintJobs(expectedPrintJobArr, getHistoryPrintJobEntries(page!));
  });

  test('OngoingPrintJobEmptyState', async () => {
    await initializePrintManagementApp(/*expectedArr=*/[]);
    await mojoApi_.whenCalled('getPrintJobs');
    flush();
    // Assert that ongoing list is empty and the empty state message is
    // not hidden.
    assertTrue(!querySelector(page!, '#ongoingList'));
    assertTrue(
        !querySelector<HTMLElement>(page!, '#ongoingEmptyState')?.hidden);
  });

  test('CancelOngoingPrintJob', async () => {
    const kId = 'fileA';
    const kTitle = 'titleA';
    const kTime =
        convertToMojoTime(new Date(Date.parse('February 5, 2020 03:23:00')));
    const expectedArr = [
      createJobEntry(
          kId, kTitle, kTime, PrinterErrorCode.kNoError,
          /*completedInfo=*/ undefined,
          createOngoingPrintJobInfo(
              /*printedPages=*/ 0, ActivePrintJobState.kStarted)),
    ];

    const expectedHistoryList = [createJobEntry(
        kId, kTitle, kTime, PrinterErrorCode.kNoError,
        createCompletedPrintJobInfo(PrintJobCompletionStatus.kCanceled))];

    await initializePrintManagementApp(expectedArr);
    await mojoApi_.whenCalled('getPrintJobs');
    flush();
    const jobEntries = getOngoingPrintJobEntries(page!);
    verifyPrintJobs(expectedArr, jobEntries);

    await simulateCancelPrintJob(
        jobEntries[0]!, mojoApi_,
        /*shouldAttemptCancel*/ true, expectedHistoryList);
    flush();

    // Verify that there are no ongoing print jobs and history list is
    // populated.
    assertTrue(!querySelector(page!, '#ongoingList'));
    verifyPrintJobs(expectedHistoryList, getHistoryPrintJobEntries(page!));
  });

  test('CancelOngoingPrintJobNotAttempted', async () => {
    const kId = 'fileA';
    const kTitle = 'titleA';
    const kTime =
        convertToMojoTime(new Date(Date.parse('February 5, 2020 03:23:00')));

    const expectedArr = [
      createJobEntry(
          kId, kTitle, kTime, PrinterErrorCode.kNoError,
          /*completedInfo=*/ undefined,
          createOngoingPrintJobInfo(
              /*printedPages=*/ 0, ActivePrintJobState.kStarted)),
    ];

    const expectedHistoryList = [createJobEntry(
        kId, kTitle, kTime, PrinterErrorCode.kNoError,
        createCompletedPrintJobInfo(PrintJobCompletionStatus.kCanceled))];

    await initializePrintManagementApp(expectedArr);
    await mojoApi_.whenCalled('getPrintJobs');
    flush();
    const jobEntries = getOngoingPrintJobEntries(page!);
    verifyPrintJobs(expectedArr, jobEntries);

    await simulateCancelPrintJob(
        jobEntries[0]!, mojoApi_,
        /*shouldAttemptCancel=*/ false, expectedHistoryList);
    flush();

    // Verify that there are no ongoing print jobs and history list is
    // populated.
    // TODO(crbug/1093527): Show error message to user after UX guidance.
    assertTrue(!querySelector(page!, '#ongoingList'));
    verifyPrintJobs(expectedHistoryList, getHistoryPrintJobEntries(page!));
  });

  test('IsJellyEnabledForPrintManagementUpdatesCSS', async () => {
    const disabledUrl = 'chrome://resources/chromeos/colors/cros_styles.css';
    const linkEl = document.createElement('link');
    linkEl.href = disabledUrl;
    document.head.appendChild(linkEl);

    // Setup for disabled test.
    loadTimeData.overrideValues({
      isJellyEnabledForPrintManagement: false,
    });

    await initializePrintManagementApp([]);

    assertTrue(linkEl.href.includes(disabledUrl));

    // Clean up element.
    page?.remove();
    page = null;
    document.body.innerHTML = '';

    // Setup for enabled test.
    loadTimeData.overrideValues({
      isJellyEnabledForPrintManagement: true,
    });

    await initializePrintManagementApp([]);

    const enabledUrl = 'chrome://theme/colors.css';
    assertTrue(linkEl.href.includes(enabledUrl));

    // Clean up test element.
    document.head.removeChild(linkEl);
  });
});

suite('PrintJobEntryTest', () => {
  let jobEntryTestElement: PrintJobEntryElement|null = null;

  let mojoApi_: PrintingMetadataProviderInterface;

  suiteSetup(() => {
    mojoApi_ = new FakePrintingMetadataProvider();
    setMetadataProviderForTesting(mojoApi_);
  });

  teardown(() => {
    teardownElement(jobEntryTestElement);
  });

  test('initializeJobEntry', () => {
    jobEntryTestElement = initPrintJobEntryElement();
    const expectedTitle = 'title.pdf';
    const expectedStatus = PrintJobCompletionStatus.kPrinted;
    const expectedPrinterError = PrinterErrorCode.kNoError;
    const expectedCreationTime = convertToMojoTime(new Date());

    const completedInfo = createCompletedPrintJobInfo(expectedStatus);
    jobEntryTestElement.jobEntry = createJobEntry(
        /*id=*/ '1', expectedTitle, expectedCreationTime, expectedPrinterError,
        completedInfo, /*activeInfo=*/ undefined);

    flush();

    // Assert the title, creation time, and status are displayed correctly.
    assertEquals(
        expectedTitle,
        querySelector(jobEntryTestElement, '#jobTitle')?.textContent?.trim());
    assertEquals(
        'Printed',
        querySelector(
            jobEntryTestElement, '#completionStatus')!.textContent?.trim());
    // Verify correct icon is shown.
    assertEquals(
        'print-management:file-pdf',
        querySelector<IronIconElement>(jobEntryTestElement, '#fileIcon')?.icon);

    // Change date and assert it shows the correct date (Feb 5, 2020);
    jobEntryTestElement.set('jobEntry.creationTime', {
      internalValue: convertToMojoTime(new Date('February 5, 2020 03:24:00')),
    });
    assertEquals(
        'Feb 5, 2020',
        querySelector(jobEntryTestElement, '#creationTime')
            ?.textContent?.trim());
  });

  test('initializeFailedJobEntry', () => {
    jobEntryTestElement = initPrintJobEntryElement();
    const expectedTitle = 'titleA.doc';
    // Create a print job with a failed status with error: out of paper.
    jobEntryTestElement.jobEntry = createJobEntry(
        /*id=*/ '1', expectedTitle,
        convertToMojoTime(new Date('February 5, 2020 03:24:00')),
        PrinterErrorCode.kOutOfPaper,
        createCompletedPrintJobInfo(PrintJobCompletionStatus.kFailed),
        /*activeInfo=*/ undefined);

    flush();

    // Assert the title, creation time, and status are displayed correctly.
    assertEquals(
        expectedTitle,
        querySelector(jobEntryTestElement, '#jobTitle')?.textContent?.trim());
    assertEquals(
        'Failed - Out of paper',
        querySelector(jobEntryTestElement, '#completionStatus')
            ?.textContent?.trim());
    // Verify correct icon is shown.
    assertEquals(
        'print-management:file-word',
        querySelector<IronIconElement>(jobEntryTestElement, '#fileIcon')?.icon);
    assertEquals(
        'Feb 5, 2020',
        querySelector(jobEntryTestElement, '#creationTime')
            ?.textContent?.trim());
  });

  test('initializeOngoingJobEntry', () => {
    jobEntryTestElement = initPrintJobEntryElement();
    const expectedTitle = 'title';
    const expectedCreationTime =
        convertToMojoTime(new Date('February 5, 2020 03:24:00'));
    const expectedPrinterError = ActivePrintJobState.kStarted;

    jobEntryTestElement.jobEntry = createJobEntry(
        /*id=*/ '1', expectedTitle, expectedCreationTime,
        PrinterErrorCode.kNoError, /*completedInfo=*/ undefined,
        createOngoingPrintJobInfo(/*printedPages=*/ 1, expectedPrinterError));

    flush();

    // Assert the title, creation time, and status are displayed correctly.
    assertEquals(
        expectedTitle,
        querySelector(jobEntryTestElement, '#jobTitle')?.textContent?.trim());
    assertEquals(
        'Feb 5, 2020',
        querySelector(jobEntryTestElement, '#creationTime')
            ?.textContent?.trim());
    assertEquals(
        '1/4',
        querySelector(jobEntryTestElement, '#numericalProgress')
            ?.textContent?.trim());
    assertEquals(
        'print-management:file-generic',
        querySelector<IronIconElement>(jobEntryTestElement, '#fileIcon')?.icon);
  });

  test('initializeStoppedOngoingJobEntry', () => {
    jobEntryTestElement = initPrintJobEntryElement();
    const expectedTitle = 'title';
    const expectedCreationTime =
        convertToMojoTime(new Date('February 5, 2020 03:24:00'));
    const expectedPrinterError = ActivePrintJobState.kStarted;
    const expectedOngoingError = PrinterErrorCode.kOutOfPaper;

    jobEntryTestElement.jobEntry = createJobEntry(
        /*id=*/ '1', expectedTitle, expectedCreationTime, expectedOngoingError,
        /*completedInfo=*/ undefined,
        createOngoingPrintJobInfo(/*printedPages=*/ 1, expectedPrinterError));

    flush();

    // Assert the title, creation time, and status are displayed correctly.
    assertEquals(
        expectedTitle,
        querySelector(jobEntryTestElement, '#jobTitle')?.textContent?.trim());
    assertEquals(
        'Feb 5, 2020',
        querySelector(jobEntryTestElement, '#creationTime')
            ?.textContent?.trim());
    assertEquals(
        'Stopped - Out of paper',
        querySelector(jobEntryTestElement, '#ongoingError')
            ?.textContent?.trim());
    assertEquals(
        'print-management:file-generic',
        querySelector<IronIconElement>(jobEntryTestElement, '#fileIcon')?.icon);
  });

  test('ensureGoogleFileIconIsShown', () => {
    jobEntryTestElement = initPrintJobEntryElement();
    jobEntryTestElement.jobEntry = createJobEntry(
        /*id=*/ '1', /*fileName=*/ '.test - Google Docs',
        /*date=*/ convertToMojoTime(new Date('February 5, 2020 03:24:00')),
        PrinterErrorCode.kNoError, /*completedInfo=*/ undefined,
        createOngoingPrintJobInfo(
            /*printedPages=*/ 1,
            /*printerError=*/ ActivePrintJobState.kStarted));

    flush();

    assertEquals(
        'print-management:file-gdoc',
        querySelector<IronIconElement>(jobEntryTestElement, '#fileIcon')?.icon);
  });

  test('ensureGenericFileIconIsShown', () => {
    jobEntryTestElement = initPrintJobEntryElement();
    jobEntryTestElement.jobEntry = createJobEntry(
        /*id=*/ '1', /*fileName=*/ '.test',
        /*date=*/ convertToMojoTime(new Date('February 5, 2020 03:24:00')),
        PrinterErrorCode.kNoError, /*completedInfo=*/ undefined,
        createOngoingPrintJobInfo(
            /*printedPages=*/ 1,
            /*printerError=*/ ActivePrintJobState.kStarted));

    flush();

    assertEquals(
        'print-management:file-generic',
        querySelector<IronIconElement>(jobEntryTestElement, '#fileIcon')?.icon);
  });

  test('ensureFileIconClassMatchesFileIcon', () => {
    jobEntryTestElement = initPrintJobEntryElement();
    jobEntryTestElement.jobEntry = createJobEntry(
        /*id=*/ '1', /*fileName=*/ '.test',
        /*date=*/ convertToMojoTime(new Date('February 5, 2020 03:24:00')),
        PrinterErrorCode.kNoError, /*completedInfo=*/ undefined,
        createOngoingPrintJobInfo(
            /*printedPages=*/ 1,
            /*printerError=*/ ActivePrintJobState.kStarted));
    flush();
    assertEquals(
        jobEntryTestElement.getFileIconClass(), 'flex-center file-icon-gray');

    jobEntryTestElement.jobEntry = createJobEntry(
        /*id=*/ '1', /*fileName=*/ '.doc',
        /*date=*/ convertToMojoTime(new Date('February 5, 2020 03:24:00')),
        PrinterErrorCode.kNoError, /*completedInfo=*/ undefined,
        createOngoingPrintJobInfo(
            /*printedPages=*/ 1,
            /*printerError=*/ ActivePrintJobState.kStarted));
    flush();
    assertEquals(
        jobEntryTestElement.getFileIconClass(), 'flex-center file-icon-blue');

    jobEntryTestElement.jobEntry = createJobEntry(
        /*id=*/ '1', /*fileName=*/ ' - Google Drawings',
        /*date=*/ convertToMojoTime(new Date('February 5, 2020 03:24:00')),
        PrinterErrorCode.kNoError, /*completedInfo=*/ undefined,
        createOngoingPrintJobInfo(
            /*printedPages=*/ 1,
            /*printerError=*/ ActivePrintJobState.kStarted));
    flush();
    assertEquals(
        jobEntryTestElement.getFileIconClass(), 'flex-center file-icon-red');

    jobEntryTestElement.jobEntry = createJobEntry(
        /*id=*/ '1', /*fileName=*/ '.xlsx',
        /*date=*/ convertToMojoTime(new Date('February 5, 2020 03:24:00')),
        PrinterErrorCode.kNoError, /*completedInfo=*/ undefined,
        createOngoingPrintJobInfo(
            /*printedPages=*/ 1,
            /*printerError=*/ ActivePrintJobState.kStarted));
    flush();
    assertEquals(
        jobEntryTestElement.getFileIconClass(), 'flex-center file-icon-green');

    jobEntryTestElement.jobEntry = createJobEntry(
        /*id=*/ '1', /*fileName=*/ ' - Google Slides',
        /*date=*/ convertToMojoTime(new Date('February 5, 2020 03:24:00')),
        PrinterErrorCode.kNoError, /*completedInfo=*/ undefined,
        createOngoingPrintJobInfo(
            /*printedPages=*/ 1,
            /*printerError=*/ ActivePrintJobState.kStarted));
    flush();
    assertEquals(
        jobEntryTestElement.getFileIconClass(), 'flex-center file-icon-yellow');
  });
});
