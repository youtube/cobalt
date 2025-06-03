// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface Attachment {
  name: string;
  size: number;
  readable: boolean;
}

export enum DisplayAnnotationsAction {
  DISPLAY_ANNOTATIONS = 'display-annotations',
  HIDE_ANNOTATIONS = 'hide-annotations',
}

export interface DocumentMetadata {
  author: string;
  canSerializeDocument: boolean;
  creationDate: string;
  creator: string;
  fileSize: string;
  keywords: string;
  linearized: boolean;
  modDate: string;
  pageSize: string;
  producer: string;
  subject: string;
  title: string;
  version: string;
}

/** Enumeration of page fitting types and bounding box fitting types. */
export enum FittingType {
  NONE = 'none',
  FIT_TO_PAGE = 'fit-to-page',
  FIT_TO_WIDTH = 'fit-to-width',
  FIT_TO_HEIGHT = 'fit-to-height',
  FIT_TO_BOUNDING_BOX = 'fit-to-bounding-box',
  FIT_TO_BOUNDING_BOX_WIDTH = 'fit-to-bounding-box-width',
  FIT_TO_BOUNDING_BOX_HEIGHT = 'fit-to-bounding-box-height',
}

export interface NamedDestinationMessageData {
  messageId: string;
  pageNumber: number;
  namedDestinationView?: string;
}

/**
 * Enumeration of save message request types. Must match `SaveRequestType` in
 * pdf/pdf_view_web_plugin.h.
 */
export enum SaveRequestType {
  ANNOTATION,
  ORIGINAL,
  EDITED,
}

export interface Point {
  x: number;
  y: number;
}

export interface Rect {
  x: number;
  y: number;
  width: number;
  height: number;
}

export type ExtendedKeyEvent = KeyboardEvent&{
  fromScriptingAPI?: boolean,
  fromPlugin?: boolean,
};

/**
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused. This enum is tied directly to a UMA
 * enum, PdfOcrUserSelection, defined in //tools/metrics/histograms/enums.xml
 * and should always reflect it (do not change one without changing the other).
 */
export enum PdfOcrUserSelection {
  DEPRECATED_TURN_ON_ONCE_FROM_CONTEXT_MENU = 0,
  TURN_ON_ALWAYS_FROM_CONTEXT_MENU = 1,
  TURN_OFF_FROM_CONTEXT_MENU = 2,
  TURN_ON_ALWAYS_FROM_MORE_ACTIONS = 3,
  TURN_OFF_FROM_MORE_ACTIONS = 4,
  TURN_ON_ALWAYS_FROM_SETTINGS = 5,
  TURN_OFF_FROM_SETTINGS = 6,
}
