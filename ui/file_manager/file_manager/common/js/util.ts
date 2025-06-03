// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file should contain utility functions used only by the
 * files app. Other shared utility functions can be found in base/*_util.js,
 * which allows finer-grained control over introducing dependencies.
 */

import {promisify} from './api.js';
import {isDriveFsBulkPinningEnabled} from './flags.js';

export function iconSetToCSSBackgroundImageValue(
    iconSet: chrome.fileManagerPrivate.IconSet): string {
  let lowDpiPart = null;
  let highDpiPart = null;
  if (iconSet.icon16x16Url) {
    lowDpiPart = 'url(' + iconSet.icon16x16Url + ') 1x';
  }
  if (iconSet.icon32x32Url) {
    highDpiPart = 'url(' + iconSet.icon32x32Url + ') 2x';
  }

  if (lowDpiPart && highDpiPart) {
    return '-webkit-image-set(' + lowDpiPart + ', ' + highDpiPart + ')';
  } else if (lowDpiPart) {
    return '-webkit-image-set(' + lowDpiPart + ')';
  } else if (highDpiPart) {
    return '-webkit-image-set(' + highDpiPart + ')';
  }

  return 'none';
}

/**
 * Mapping table for FileError.code style enum to DOMError.name string.
 */
export enum FileErrorToDomError {
  ABORT_ERR = 'AbortError',
  INVALID_MODIFICATION_ERR = 'InvalidModificationError',
  INVALID_STATE_ERR = 'InvalidStateError',
  NO_MODIFICATION_ALLOWED_ERR = 'NoModificationAllowedError',
  NOT_FOUND_ERR = 'NotFoundError',
  NOT_READABLE_ERR = 'NotReadable',
  PATH_EXISTS_ERR = 'PathExistsError',
  QUOTA_EXCEEDED_ERR = 'QuotaExceededError',
  TYPE_MISMATCH_ERR = 'TypeMismatchError',
  ENCODING_ERR = 'EncodingError',
}

/**
 * Extracts path from filesystem: URL.
 * @return The path if it can be parsed, null if it cannot.
 */
export function extractFilePath(url: string|null|undefined): string|null {
  const match =
      /^filesystem:[\w-]*:\/\/[\w-]*\/(external|persistent|temporary)(\/.*)$/
          .exec(url || '');
  const path = match && match[2];
  if (!path) {
    return null;
  }
  return decodeURIComponent(path);
}

/**
 * @return True if the Files app is running as an open files or a
 *     select folder dialog. False otherwise.
 */
export function runningInBrowser(): boolean {
  return !window.appID;
}

/**
 * The type of a file operation error.
 */
export enum FileOperationErrorType {
  UNEXPECTED_SOURCE_FILE = 0,
  TARGET_EXISTS = 1,
  FILESYSTEM_ERROR = 2,
}

/**
 * The last URL with visitURL().
 */
let lastVisitedURL: string;

/**
 * Visit the URL.
 *
 * If the browser is opening, the url is opened in a new tab, otherwise the url
 * is opened in a new window.
 */
export function visitURL(url: string): void {
  lastVisitedURL = url;
  // openURL opens URLs in the primary browser (ash vs lacros) as opposed to
  // window.open which always opens URLs in ash-chrome.
  chrome.fileManagerPrivate.openURL(url);
}

/**
 * Return the last URL visited with visitURL().
 */
export function getLastVisitedURL(): string {
  return lastVisitedURL;
}

/**
 * Returns whether the window is teleported or not.
 */
export function isTeleported(): Promise<boolean> {
  return new Promise(onFulfilled => {
    chrome.fileManagerPrivate.getProfiles(
        (_: chrome.fileManagerPrivate.ProfileInfo[], currentId: string,
         displayedId: string) => {
          onFulfilled(currentId !== displayedId);
        });
  });
}

/**
 * Runs chrome.test.sendMessage in test environment. Does nothing if running
 * in production environment.
 */
export function testSendMessage(message: string): void {
  const test = chrome.test || (window.top as ChromeWindow).chrome.test;
  if (test) {
    test.sendMessage(message);
  }
}

/**
 * Extracts the extension of the path.
 *
 * Examples:
 * splitExtension('abc.ext') -> ['abc', '.ext']
 * splitExtension('a/b/abc.ext') -> ['a/b/abc', '.ext']
 * splitExtension('a/b') -> ['a/b', '']
 * splitExtension('.cshrc') -> ['', '.cshrc']
 * splitExtension('a/b.backup/hoge') -> ['a/b.backup/hoge', '']
 */
export function splitExtension(path: string): [string, string] {
  let dotPosition = path.lastIndexOf('.');
  if (dotPosition <= path.lastIndexOf('/')) {
    dotPosition = -1;
  }

  const filename = dotPosition != -1 ? path.substr(0, dotPosition) : path;
  const extension = dotPosition != -1 ? path.substr(dotPosition) : '';
  return [filename, extension];
}

/**
 * Checks if an API call returned an error, and if yes then prints it.
 */
export function checkAPIError(): void {
  if (chrome.runtime.lastError) {
    console.warn(chrome.runtime.lastError.message);
  }
}

/**
 * Makes a promise which will be fulfilled |ms| milliseconds later.
 */
export function delay(ms: number): Promise<void> {
  return new Promise(resolve => {
    setTimeout(resolve, ms);
  });
}

/**
 * Makes a promise which will be rejected if the given |promise| is not resolved
 * or rejected for |ms| milliseconds.
 */
export function timeoutPromise<T>(
    promise: Promise<T>, ms: number, message?: string): Promise<T> {
  return Promise.race([
    promise,
    delay(ms).then(() => {
      throw new Error(message || 'Operation timed out.');
    }),
  ]);
}

/**
 * Executes a functions only when the context is not the incognito one in a
 * regular session. Returns a promise that when fulfilled informs us whether or
 * not the callback was invoked.
 */
export async function doIfPrimaryContext(callback: VoidCallback):
    Promise<boolean> {
  const guestMode = await isInGuestMode();
  if (guestMode) {
    callback();
    return true;
  }
  return false;
}

/**
 * Returns the Files app modal dialog used to embed any files app dialog
 * that derives from cr.ui.dialogs.
 */
export function getFilesAppModalDialogInstance(): HTMLDialogElement {
  let dialogElement =
      document.querySelector<HTMLDialogElement>('#files-app-modal-dialog');

  if (!dialogElement) {  // Lazily create the files app dialog instance.
    dialogElement = document.createElement('dialog');
    dialogElement.id = 'files-app-modal-dialog';
    document.body.appendChild(dialogElement);
  }

  return dialogElement;
}

export function descriptorEqual(
    left: chrome.fileManagerPrivate.FileTaskDescriptor,
    right: chrome.fileManagerPrivate.FileTaskDescriptor): boolean {
  return left.appId === right.appId && left.taskType === right.taskType &&
      left.actionId === right.actionId;
}

/**
 * Create a taskID which is a string unique-ID for a task. This is temporary
 * and will be removed once we use task.descriptor everywhere instead.
 */
export function makeTaskID(
    {appId, taskType, actionId}: chrome.fileManagerPrivate.FileTaskDescriptor):
    string {
  return `${appId}|${taskType}|${actionId}`;
}

/**
 * Returns a new promise which, when fulfilled carries a boolean indicating
 * whether the app is in the guest mode. Typical use:
 *
 * isInGuestMode().then(
 *     (guest) => { if (guest) { ... in guest mode } }
 */
export async function isInGuestMode(): Promise<boolean> {
  const profiles: chrome.fileManagerPrivate.ProfileInfo[] =
      await promisify(chrome.fileManagerPrivate.getProfiles);
  return profiles.length > 0 && profiles[0]?.profileId === '$guest';
}

/**
 * A kind of error that represents user electing to cancel an operation. We use
 * this specialization to differentiate between system errors and errors
 * generated through legitimate user actions.
 */
export class UserCanceledError extends Error {}

/**
 * Returns whether the given value is null or undefined.
 */
export const isNullOrUndefined = <T>(value: T): boolean =>
    value === null || value === undefined;

/**
 * Bulk pinning should only show visible UI elements when in progress or
 * continuing to sync.
 */
export function canBulkPinningCloudPanelShow(
    stage: chrome.fileManagerPrivate.BulkPinStage|undefined,
    pref: boolean|undefined): boolean {
  if (!isDriveFsBulkPinningEnabled()) {
    return false;
  }

  const BulkPinStage = chrome.fileManagerPrivate.BulkPinStage;
  // If the stage is in progress and the bulk pinning preference is enabled,
  // then the cloud panel should not be visible.
  if (pref &&
      (stage === BulkPinStage.GETTING_FREE_SPACE ||
       stage === BulkPinStage.LISTING_FILES ||
       stage === BulkPinStage.SYNCING)) {
    return true;
  }

  // For the PAUSED... states the preference should still be enabled, however,
  // for the latter the preference will have been disabled.
  if ((stage === BulkPinStage.PAUSED_OFFLINE && pref) ||
      (stage === BulkPinStage.PAUSED_BATTERY_SAVER && pref) ||
      stage === BulkPinStage.NOT_ENOUGH_SPACE) {
    return true;
  }

  return false;
}
