// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BookmarkNode, BookmarksApiProxy, NodeMap, Query} from 'chrome://bookmarks/bookmarks.js';
import {normalizeNode, normalizeNodes} from 'chrome://bookmarks/bookmarks.js';
import {FakeChromeEvent} from 'chrome://webui-test/fake_chrome_event.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestBookmarksApiProxy extends TestBrowserProxy implements
    BookmarksApiProxy {
  onCreated = new FakeChromeEvent();
  onRemoved = new FakeChromeEvent();
  onChanged = new FakeChromeEvent();
  onMoved = new FakeChromeEvent();
  onChildrenReordered = new FakeChromeEvent();
  onImportBegan = new FakeChromeEvent();
  onImportEnded = new FakeChromeEvent();

  private searchResponse_: BookmarkNode[] = [];
  private getTreeResponse_: NodeMap = {};

  constructor() {
    super([
      'create',
      'getTree',
      'search',
      'update',
    ]);
  }

  getTree() {
    this.methodCalled('getTree');
    return Promise.resolve(this.getTreeResponse_);
  }

  setGetTree(nodes: chrome.bookmarks.BookmarkTreeNode[]) {
    this.getTreeResponse_ = normalizeNodes(nodes[0]!);
  }

  search(query: Query) {
    this.methodCalled('search', query);
    return Promise.resolve(this.searchResponse_);
  }

  setSearchResponse(response: chrome.bookmarks.BookmarkTreeNode[]) {
    this.searchResponse_ = response.map(normalizeNode);
  }

  update(id: string, changes: {title?: string, url?: string}) {
    this.methodCalled('update', [id, changes]);
    return Promise.resolve({
      id: id,
      title: changes.title || '',
      url: changes.url,
    });
  }

  create(parentId: string, index: number|null, title: string, url?: string) {
    this.methodCalled('create', [parentId, index, title, url]);
    return Promise.resolve({
      id: 'new_node',
      parentId: parentId,
      title: title,
      url: url,
    });
  }
}
