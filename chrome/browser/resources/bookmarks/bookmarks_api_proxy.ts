// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ChromeEvent} from '/tools/typescript/definitions/chrome_event.js';

import type {BookmarkNode, NodeMap} from './types.js';
import {normalizeNode, normalizeNodes} from './util.js';

export type Query = string|{
  query?: string,
  url?: string,
  title?: string,
};

class BookmarkEventForwarder<T extends Function> implements ChromeEvent<T> {
  private listeners_: T[] = [];

  addListener(listener: T) {
    this.listeners_.push(listener);
  }

  removeListener(listener: T) {
    this.listeners_ = this.listeners_.filter(l => l !== listener);
  }

  forward(...args: any[]) {
    this.listeners_.forEach(l => l(...args));
  }
}

export interface BookmarksApiProxy {
  getTree(): Promise<NodeMap>;
  search(query: Query): Promise<BookmarkNode[]>;
  update(id: string, changes: {title?: string, url?: string}):
      Promise<BookmarkNode>;
  create(parentId: string, index: number|null, title: string, url?: string):
      Promise<BookmarkNode>;

  onCreated: ChromeEvent<
      (parentId: string, index: number, node: BookmarkNode) => void>;
  onRemoved: ChromeEvent<(id: string, parentId: string, index: number) => void>;
  onChanged: ChromeEvent<(id: string, node: BookmarkNode) => void>;
  onMoved: ChromeEvent<
      (id: string, oldParentId: string, oldIndex: number, newParentId: string,
       newIndex: number) => void>;
  onChildrenReordered:
      ChromeEvent<(id: string, reorderInfo: {childIds: string[]}) => void>;
  onImportBegan: ChromeEvent<() => void>;
  onImportEnded: ChromeEvent<() => void>;
}

export class BookmarksApiProxyImpl implements BookmarksApiProxy {
  onCreated = new BookmarkEventForwarder<
      (parentId: string, index: number, node: BookmarkNode) => void>();
  onRemoved = new BookmarkEventForwarder<
      (id: string, parentId: string, index: number) => void>();
  onChanged =
      new BookmarkEventForwarder<(id: string, node: BookmarkNode) => void>();
  onMoved = new BookmarkEventForwarder<
      (id: string, oldParentId: string, oldIndex: number, newParentId: string,
       newIndex: number) => void>();
  onChildrenReordered = new BookmarkEventForwarder<
      (id: string, reorderInfo: {childIds: string[]}) => void>();
  onImportBegan = new BookmarkEventForwarder<() => void>();
  onImportEnded = new BookmarkEventForwarder<() => void>();

  constructor() {
    chrome.bookmarks.onCreated.addListener((_id, node) => {
      this.onCreated.forward(node.parentId!, node.index!, normalizeNode(node));
    });
    chrome.bookmarks.onRemoved.addListener((id, removeInfo) => {
      this.onRemoved.forward(id, removeInfo.parentId, removeInfo.index);
    });
    chrome.bookmarks.onChanged.addListener((id, changeInfo) => {
      this.onChanged.forward(id, {
        id: id,
        title: changeInfo.title,
        url: changeInfo.url,
      });
    });
    chrome.bookmarks.onMoved.addListener((id, moveInfo) => {
      this.onMoved.forward(
          id, moveInfo.oldParentId, moveInfo.oldIndex, moveInfo.parentId,
          moveInfo.index);
    });
    chrome.bookmarks.onChildrenReordered.addListener((id, reorderInfo) => {
      this.onChildrenReordered.forward(id, {childIds: reorderInfo.childIds});
    });
    chrome.bookmarks.onImportBegan.addListener(() => {
      this.onImportBegan.forward();
    });
    chrome.bookmarks.onImportEnded.addListener(() => {
      this.onImportEnded.forward();
    });
  }

  getTree() {
    return chrome.bookmarks.getTree().then(
        results => normalizeNodes(results[0]!));
  }

  search(query: Query) {
    return chrome.bookmarks.search(query).then(
        results => results.map(normalizeNode));
  }

  update(id: string, changes: {title?: string, url?: string}) {
    return chrome.bookmarks.update(id, changes).then(normalizeNode);
  }

  create(parentId: string, index: number|null, title: string, url?: string) {
    const createDetails: chrome.bookmarks.CreateDetails = {
      parentId: parentId,
      title: title,
    };
    if (index !== null) {
      createDetails.index = index;
    }
    if (url !== undefined) {
      createDetails.url = url;
    }
    return chrome.bookmarks.create(createDetails).then(normalizeNode);
  }

  static getInstance(): BookmarksApiProxy {
    return instance || (instance = new BookmarksApiProxyImpl());
  }

  static setInstance(obj: BookmarksApiProxy) {
    instance = obj;
  }
}

let instance: BookmarksApiProxy|null = null;
