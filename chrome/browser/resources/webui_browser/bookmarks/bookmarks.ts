// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './bookmark_tree_node.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BookmarksService} from '../bookmarks_api.mojom-webui.js';
import type {BookmarkNode} from '../bookmarks_api.mojom-webui.js';

import {getCss} from './bookmarks.css.js';
import {getHtml} from './bookmarks.html.js';

export class BookmarksElement extends CrLitElement {
  static get is() {
    return 'webui-browser-bookmarks';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      rootNode_: {type: Object},
    };
  }

  protected accessor rootNode_: BookmarkNode = {
    folder: {
      id: {value: ''},
      title: '',
      children: [],
    },
  };

  override connectedCallback() {
    super.connectedCallback();
    this.fetchBookmarks_();
    this.addEventListener('bookmarks-changed', () => this.fetchBookmarks_());
  }

  private fetchBookmarks_() {
    BookmarksService.getRemote().getBookmarks().then(
        root => this.rootNode_ = root);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-bookmarks': BookmarksElement;
  }
}

customElements.define(BookmarksElement.is, BookmarksElement);
