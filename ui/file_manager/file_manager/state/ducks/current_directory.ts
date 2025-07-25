// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFileTasks} from '../../common/js/api.js';
import {DialogType} from '../../common/js/dialog_type.js';
import {getNativeEntry} from '../../common/js/entry_utils.js';
import {annotateTasks, getDefaultTask, INSTALL_LINUX_PACKAGE_TASK_DESCRIPTOR} from '../../common/js/file_tasks.js';
import {descriptorEqual} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FakeEntry, FilesAppDirEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {CurrentDirectory, DirectoryContent, FileData, FileKey, FileTasks, PropStatus, Selection, State} from '../../externs/ts/state.js';
import {constants} from '../../foreground/js/constants.js';
import {PathComponent} from '../../foreground/js/path_component.js';
import type {ActionsProducerGen} from '../../lib/actions_producer.js';
import {Slice} from '../../lib/base_store.js';
import {keyedKeepFirst} from '../../lib/concurrency_models.js';
import {getStore} from '../store.js';

import {cacheEntries} from './all_entries.js';

/**
 * @fileoverview Current directory slice of the store.
 * @suppress {checkTypes}
 */

const slice = new Slice<State, State['currentDirectory']>('currentDirectory');
export {slice as currentDirectorySlice};

function getEmptySelection(keys: FileKey[] = []): Selection {
  return {
    keys,
    dirCount: 0,
    fileCount: 0,
    // hostedCount might be updated to undefined in the for loop below.
    hostedCount: 0,
    // offlineCachedCount might be updated to undefined in the for loop below.
    offlineCachedCount: 0,
    fileTasks: {
      tasks: [],
      defaultTask: undefined,
      policyDefaultHandlerStatus: undefined,
      status: PropStatus.STARTED,
    },
  };
}

/**
 * Returns true if any of the entries in `currentDirectory` are DLP disabled,
 * and false otherwise.
 */
export function hasDlpDisabledFiles(currentState: State): boolean {
  const content = currentState.currentDirectory?.content;
  if (!content) {
    return false;
  }
  for (const key of content!.keys) {
    const fileData = currentState.allEntries[key];
    if (!fileData) {
      console.warn(`Missing entry: ${key}`);
      continue;
    }
    if (fileData!.metadata.isRestrictedForDestination) {
      return true;
    }
  }
  return false;
}

/** Create action to change the Current Directory. */
export const changeDirectory = slice.addReducer('set', changeDirectoryReducer);

function changeDirectoryReducer(currentState: State, payload: {
  to?: DirectoryEntry|FilesAppDirEntry, toKey: FileKey,
  status?: PropStatus,
}): State {
  // Cache entries, so the reducers can use any entry from `allEntries`.
  if (payload.to) {
    cacheEntries(currentState, [payload.to]);
  }

  const {to, toKey} = payload;
  const key = toKey || to!.toURL();
  const status = payload.status || PropStatus.STARTED;

  const fileData = currentState.allEntries[key];

  let selection = currentState.currentDirectory?.selection;
  // Use an empty selection when a selection isn't defined or it's navigating to
  // a new directory.
  if (!selection || currentState.currentDirectory?.key !== key) {
    selection = {
      keys: [],
      dirCount: 0,
      fileCount: 0,
      hostedCount: undefined,
      offlineCachedCount: undefined,
      fileTasks: {
        tasks: [],
        policyDefaultHandlerStatus: undefined,
        defaultTask: undefined,
        status: PropStatus.SUCCESS,
      },
    };
  }

  let content = currentState.currentDirectory?.content;
  let hasDlpDisabledFiles =
      currentState.currentDirectory?.hasDlpDisabledFiles || false;
  // Use empty content when it isn't defined or it's navigating to a new
  // directory. The content will be updated again after a successful scan.
  if (!content || currentState.currentDirectory?.key !== key) {
    content = {
      keys: [],
    };
    hasDlpDisabledFiles = false;
  }

  let currentDirectory: CurrentDirectory = {
    key,
    status,
    pathComponents: [],
    content: content,
    rootType: undefined,
    selection,
    hasDlpDisabledFiles: hasDlpDisabledFiles,
  };

  // The new directory might not be in the allEntries yet, this might happen
  // when starting to change the directory for a entry that isn't cached.
  // At the end of the change directory, DirectoryContents will send an Action
  // with the Entry to be cached.
  if (fileData) {
    const {volumeManager} = window.fileManager;
    if (!volumeManager) {
      console.debug(`VolumeManager not available yet.`);
      currentDirectory = currentState.currentDirectory || currentDirectory;
    } else {
      const components = PathComponent.computeComponentsFromEntry(
          fileData.entry!, volumeManager);
      currentDirectory.pathComponents = components.map(c => {
        return {
          name: c.name,
          label: c.name,
          key: c.url_,
        };
      });

      const locationInfo = volumeManager.getLocationInfo(fileData.entry!);
      currentDirectory.rootType = locationInfo?.rootType;
    }
  }

  return {
    ...currentState,
    currentDirectory,
  };
}

/** Create action to update currently selected files/folders. */
export const updateSelection =
    slice.addReducer('set-selection', updateSelectionReducer);

function updateSelectionReducer(currentState: State, payload: {
  selectedKeys: FileKey[],
  entries: Array<Entry|FilesAppEntry>,
}): State {
  // Cache entries, so the reducers can use any entry from `allEntries`.
  cacheEntries(currentState, payload.entries);

  const updatingToEmpty =
      (payload.entries.length === 0 && payload.selectedKeys.length === 0);
  if (!currentState.currentDirectory) {
    if (!updatingToEmpty) {
      console.warn('Missing `currentDirectory`');
      console.debug('Dropping action:', payload);
    }
    return currentState;
  }

  if (!currentState.currentDirectory.content) {
    if (!updatingToEmpty) {
      console.warn('Missing `currentDirectory.content`');
      console.debug('Dropping action:', payload);
    }
    return currentState;
  }

  const selectedKeys = payload.selectedKeys;
  const contentKeys = new Set(currentState.currentDirectory!.content!.keys);
  const missingKeys = selectedKeys.filter(k => !contentKeys.has(k));

  if (missingKeys.length > 0) {
    console.warn(
        'Got selected keys that are not in current directory, ' +
        'continuing anyway');
    console.debug(`Missing keys: ${missingKeys.join('\n')} \nexisting keys:\n ${
        (currentState.currentDirectory?.content?.keys ?? []).join('\n')}`);
  }

  const selection = getEmptySelection(selectedKeys);

  for (const key of selectedKeys) {
    const fileData = currentState.allEntries[key];
    if (!fileData) {
      console.warn(`Missing entry: ${key}`);
      continue;
    }
    if (fileData.isDirectory) {
      selection.dirCount++;
    } else {
      selection.fileCount++;
    }

    // Update hostedCount to undefined if any entry doesn't have the metadata
    // yet.
    const isHosted = fileData.metadata?.hosted;
    if (isHosted === undefined) {
      selection.hostedCount = undefined;
    } else {
      if (selection.hostedCount !== undefined && isHosted) {
        selection.hostedCount++;
      }
    }

    // Update offlineCachedCount to undefined if any entry doesn't have the
    // metadata yet.
    const isOfflineCached = fileData.metadata?.offlineCached;
    if (isOfflineCached === undefined) {
      selection.offlineCachedCount = undefined;
    } else {
      if (selection.offlineCachedCount !== undefined && isOfflineCached) {
        selection.offlineCachedCount++;
      }
    }
  }

  const currentDirectory: CurrentDirectory = {
    ...currentState.currentDirectory,
    selection,
  } as CurrentDirectory;

  return {
    ...currentState,
    currentDirectory,
  };
}

/** Create action to update FileTasks for the current selection. */
export const updateFileTasks =
    slice.addReducer('set-file-tasks', updateFileTasksReducer);

function updateFileTasksReducer(
    currentState: State, payload: FileTasks): State {
  const initialSelection =
      currentState.currentDirectory?.selection ?? getEmptySelection();

  // Apply the changes over the current selection.
  const fileTasks: FileTasks = {
    ...initialSelection.fileTasks,
    ...payload,
  };

  // Update the selection and current directory objects.
  const selection: Selection = {
    ...initialSelection,
    fileTasks,
  };
  const currentDirectory: CurrentDirectory = {
    ...currentState.currentDirectory,
    selection,
  } as CurrentDirectory;

  return {
    ...currentState,
    currentDirectory,
  };
}

/** Create action to update the current directory's content. */
export const updateDirectoryContent =
    slice.addReducer('update-content', updateDirectoryContentReducer);

function updateDirectoryContentReducer(currentState: State, payload: {
  entries: Array<Entry|FilesAppEntry>,
}): State {
  // Cache entries, so the reducers can use any entry from `allEntries`.
  cacheEntries(currentState, payload.entries);

  if (!currentState.currentDirectory) {
    console.warn('Missing `currentDirectory`');
    return currentState;
  }

  const initialContent: DirectoryContent =
      currentState.currentDirectory?.content ?? {keys: []};
  const keys = payload.entries.map(e => e.toURL());
  const content: DirectoryContent = {
    ...initialContent,
    keys,
  };

  let currentDirectory: CurrentDirectory = {
    ...currentState.currentDirectory,
    content,
  };
  const newState: State = {
    ...currentState,
    currentDirectory,
  };
  currentDirectory = {
    ...currentDirectory,
    hasDlpDisabledFiles: hasDlpDisabledFiles(newState),
  };

  return {
    ...newState,
    currentDirectory,
  };
}

/**
 * Linux package installation is currently only supported for a single file
 * which is inside the Linux container, or in a shareable volume.
 * TODO(timloh): Instead of filtering these out, we probably should show a
 * dialog with an error message, similar to when attempting to run Crostini
 * tasks with non-Crostini entries.
 */
function allowCrostiniTask(filesData: FileData[]) {
  if (filesData.length !== 1) {
    return false;
  }
  const fileData = filesData[0]!;
  const rootType = (fileData.entry as FakeEntry).rootType;
  if (rootType !== VolumeManagerCommon.RootType.CROSTINI) {
    return false;
  }
  const crostini = window.fileManager.crostini;
  return crostini.canSharePath(
      constants.DEFAULT_CROSTINI_VM, (fileData.entry as Entry),
      /*persiste=*/ false);
}

const emptyAction = (status: PropStatus) => updateFileTasks({
  tasks: [],
  policyDefaultHandlerStatus: undefined,
  defaultTask: undefined,
  status,
});

export async function*
    fetchFileTasksInternal(filesData: FileData[]):
        ActionsProducerGen {
  // Filters out the non-native entries.
  filesData = filesData.filter(getNativeEntry);
  const state = getStore().getState();
  const currentRootType = state.currentDirectory?.rootType;
  const dialogType = window.fileManager.dialogType;
  const shouldDisableTasks = (
      // File Picker/Save As doesn't show the "Open" button.
      dialogType !== DialogType.FULL_PAGE ||
      // The list of available tasks should not be available to trashed items.
      currentRootType === VolumeManagerCommon.RootType.TRASH ||
      filesData.length === 0);
  if (shouldDisableTasks) {
    yield emptyAction(PropStatus.SUCCESS);
    return;
  }
  const selectionHandler = window.fileManager.selectionHandler;
  const selection = selectionHandler.selection;
  await selection.computeAdditional(window.fileManager.metadataModel);
  yield;
  try {
    const resultingTasks = await getFileTasks(
        filesData.map(fd => fd.entry),
        filesData.map(fd => fd.metadata.sourceUrl || ''));
    if (!resultingTasks || !resultingTasks.tasks) {
      return;
    }
    yield;
    if (filesData.length === 0 || resultingTasks.tasks.length === 0) {
      yield emptyAction(PropStatus.SUCCESS);
      return;
    }
    if (!allowCrostiniTask(filesData)) {
      resultingTasks.tasks = resultingTasks.tasks.filter(
          (task: chrome.fileManagerPrivate.FileTask) => !descriptorEqual(
              task.descriptor, INSTALL_LINUX_PACKAGE_TASK_DESCRIPTOR));
    }
    const tasks = annotateTasks(resultingTasks.tasks, filesData);
    resultingTasks.tasks = tasks;
    // TODO: Migrate TaskHistory to the store.
    const taskHistory = window.fileManager.taskController.taskHistory;
    const defaultTask =
        getDefaultTask(
            tasks, resultingTasks.policyDefaultHandlerStatus, taskHistory) ??
        undefined;
    yield updateFileTasks({
      tasks,
      policyDefaultHandlerStatus: resultingTasks.policyDefaultHandlerStatus,
      defaultTask: defaultTask,
      status: PropStatus.SUCCESS,
    });
  } catch (error) {
    yield emptyAction(PropStatus.ERROR);
  }
}

/** Generates key based on each FileKey (entry.toURL()). */
function getSelectionKey(filesData: FileData[]): string {
  return filesData.map(f => f?.entry.toURL()).join('|');
}

export const fetchFileTasks =
    keyedKeepFirst(fetchFileTasksInternal, getSelectionKey);
