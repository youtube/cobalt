// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BookmarksService} from '../bookmarks_api.mojom-webui.js';
import type {BookmarkNode} from '../bookmarks_api.mojom-webui.js';

import {getCss} from './bookmark_tree_node.css.js';
import {getHtml} from './bookmark_tree_node.html.js';

// TODO(crbug.com/501483829): split this up into two different types.
export class BookmarkTreeNodeElement extends CrLitElement {
  static get is() {
    return 'webui-browser-bookmark-tree-node';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      node: {type: Object},
    };
  }

  accessor node: BookmarkNode = {
    folder: {
      id: {value: ''},
      title: '',
      children: [],
    },
  };

  private bookmarksService_ = BookmarksService.getRemote();

  protected onAddUrlClick(e: Event) {
    e.stopPropagation();
    e.preventDefault();
    if (!this.node.folder) {
      return;
    }

    const parentId = this.node.folder.id!;
    const newUrlNode = {
      url: {
        id: null,
        title: 'new bookmark',
        url: 'chrome://new-tab-page',
      },
    };

    this.bookmarksService_.createBookmarkNode(parentId, null, newUrlNode);
  }

  protected onAddFolderClick(e: Event) {
    e.stopPropagation();
    e.preventDefault();
    if (!this.node.folder) {
      return;
    }

    const parentId = this.node.folder.id!;
    const newFolderNode = {
      folder: {
        id: null,
        title: 'new folder',
        children: [],
      },
    };

    this.bookmarksService_.createBookmarkNode(parentId, null, newFolderNode);
  }

  protected onEditClick(e: Event) {
    e.stopPropagation();
    e.preventDefault();

    if (this.node.url) {
      const updatedNode = {
        url: {
          id: this.node.url.id!,
          title: 'has been updated',
          url: 'http://updated.somewhere',
        },
      };

      this.bookmarksService_.updateBookmarkNode(updatedNode);
    } else if (this.node.folder) {
      const updatedNode = {
        folder: {
          id: this.node.folder.id!,
          title: 'updated folder',
          children: [],
        },
      };

      this.bookmarksService_.updateBookmarkNode(updatedNode);
    }
  }

  protected onDeleteClick(e: Event) {
    e.stopPropagation();
    e.preventDefault();

    const id = this.node.url ? this.node.url.id! : this.node.folder!.id!;

    this.bookmarksService_.deleteBookmarkNode(id);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-bookmark-tree-node': BookmarkTreeNodeElement;
  }
}

customElements.define(BookmarkTreeNodeElement.is, BookmarkTreeNodeElement);
