// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './info_view_table_row.js';

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {getTemplate} from './info_view_table.html.js';

export class InfoViewTableElement extends CustomElement {
  static get template() {
    return getTemplate();
  }

  setData(dataArray) {
    dataArray.forEach(data => {
      const row = document.createElement('info-view-table-row');
      row.setData(data);
      this.shadowRoot.querySelector('#info-view-table').appendChild(row);
    });
  }
}

customElements.define('info-view-table', InfoViewTableElement);
