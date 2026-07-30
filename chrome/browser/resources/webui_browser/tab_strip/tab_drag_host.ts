// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TabDragServiceRemote} from '../tab_drag_api.mojom-webui.js';

import type {TabStripItem} from './items.js';
import type {TabElement} from './tab.js';

export interface TabDragHost {
  shadowRoot: ShadowRoot|null;
  itemsForDrag: TabStripItem[];
  tabDragService: TabDragServiceRemote;
  getBoundingClientRect(): DOMRect;
  activateTabForDrag(id: string): void;
  getTabElementForDrag(id: string): TabElement|null;
  setItemsForDrag(items: TabStripItem[]): void;
  setDragInProgressForDrag(value: boolean): void;
  setTabStripNoDrag(noDrag: boolean): void;
  requestUpdate(): void;
  fire(type: string, detail?: unknown): void;
  commitDrag(tabId: string, finalIndex: number): void;
  getDragContainerBounds(): DOMRect;
}
