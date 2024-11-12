// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Enumeration of valid drop locations relative to an element. These are
 * bit masks to allow combining multiple locations in a single value.
 */
export enum DropPosition {
  NONE = 0,
  ABOVE = 1,
  ON = 2,
  BELOW = 4,
}

/**
 * Commands which can be handled by the CommandManager. This enum is also used
 * for metrics and should be kept in sync with BookmarkManagerCommand in
 * enums.xml. Values must never be renumbered or reused.
 */
export enum Command {
  EDIT = 0,
  // REMOVED_COPY_URL = 1  // Removed as being redundant with COPY
  SHOW_IN_FOLDER = 2,
  DELETE = 3,
  OPEN_NEW_TAB = 4,
  OPEN_NEW_WINDOW = 5,
  OPEN_INCOGNITO = 6,
  UNDO = 7,
  REDO = 8,
  // OPEN triggers when you double-click an item. NOT USED FOR METRICS.
  OPEN = 9,
  SELECT_ALL = 10,
  DESELECT_ALL = 11,
  COPY = 12,
  CUT = 13,
  PASTE = 14,
  SORT = 15,
  ADD_BOOKMARK = 16,
  ADD_FOLDER = 17,
  IMPORT = 18,
  EXPORT = 19,
  HELP_CENTER = 20,

  // Added for more precise metrics purposes. OPEN is re-mapped to one of these.
  OPEN_BOOKMARK = 21,
  OPEN_FOLDER = 22,

  // Append new values to the end of the enum.
  MAX_VALUE = 23,
}

/**
 * Where the menu was opened from. Values must never be renumbered or reused.
 */
export enum MenuSource {
  NONE = 0,
  ITEM = 1,
  TREE = 2,
  TOOLBAR = 3,
  LIST = 4,

  // Append new values to the end of the enum.
  NUM_VALUES = 5,
}

/**
 * Mirrors the C++ enum from IncognitoModePrefs.
 */
export enum IncognitoAvailability {
  ENABLED = 0,
  DISABLED = 1,
  FORCED = 2,
}

export const LOCAL_STORAGE_FOLDER_STATE_KEY: string = 'folderOpenState';

export const LOCAL_STORAGE_TREE_WIDTH_KEY: string = 'treeWidth';

export const ROOT_NODE_ID: string = '0';

export const BOOKMARKS_BAR_ID: string = '1';

export const OPEN_CONFIRMATION_LIMIT: number = 15;

/**
 * Folders that are beneath this depth will be closed by default in the folder
 * tree (where the Bookmarks Bar folder is at depth 0).
 */
export const FOLDER_OPEN_BY_DEFAULT_DEPTH: number = 1;
