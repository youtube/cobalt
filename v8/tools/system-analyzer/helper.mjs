// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const KB = 1024;
const MB = KB * KB;
const GB = MB * KB;
const kMillis2Seconds = 1 / 1000;

function formatBytes(bytes) {
  const units = ['B', 'KiB', 'MiB', 'GiB'];
  const divisor = 1024;
  let index = 0;
  while (index < units.length && bytes >= divisor) {
    index++;
    bytes /= divisor;
  }
  return bytes.toFixed(2) + units[index];
}

function formatSeconds(millis) {
  return (millis * kMillis2Seconds).toFixed(2) + 's';
}

class CSSColor {
  static getColor(name) {
    const style = getComputedStyle(document.body);
    return style.getPropertyValue(`--${name}`);
  }
  static get backgroundColor() {
    return CSSColor.getColor('backgroud-color');
  }
  static get surfaceColor() {
    return CSSColor.getColor('surface-color');
  }
  static get primaryColor() {
    return CSSColor.getColor('primary-color');
  }
  static get secondaryColor() {
    return CSSColor.getColor('secondary-color');
  }
  static get onSurfaceColor() {
    return CSSColor.getColor('on-surface-color');
  }
  static get onBackgroundColor() {
    return CSSColor.getColor('on-background-color');
  }
  static get onPrimaryColor() {
    return CSSColor.getColor('on-primary-color');
  }
  static get onSecondaryColor() {
    return CSSColor.getColor('on-secondary-color');
  }
  static get defaultColor() {
    return CSSColor.getColor('default-color');
  }
  static get errorColor() {
    return CSSColor.getColor('error-color');
  }
  static get mapBackgroundColor() {
    return CSSColor.getColor('map-background-color');
  }
  static get timelineBackgroundColor() {
    return CSSColor.getColor('timeline-background-color');
  }
  static get red() {
    return CSSColor.getColor('red');
  }
  static get green() {
    return CSSColor.getColor('green');
  }
  static get yellow() {
    return CSSColor.getColor('yellow');
  }
  static get blue() {
    return CSSColor.getColor('blue');
  }
  static get orange() {
    return CSSColor.getColor('orange');
  }
  static get violet() {
    return CSSColor.getColor('violet');
  }
}

function typeToColor(type) {
  switch (type) {
    case 'new':
      return CSSColor.green;
    case 'Normalize':
      return CSSColor.violet;
    case 'SlowToFast':
      return CSSColor.orange;
    case 'InitialMap':
      return CSSColor.yellow;
    case 'Transition':
      return CSSColor.primaryColor;
    case 'ReplaceDescriptors':
      return CSSColor.red;
    case 'LoadGlobalIC':
      return CSSColor.green;
    case 'LoadIC':
      return CSSColor.primaryColor;
    case 'StoreInArrayLiteralIC':
      return CSSColor.violet;
    case 'StoreGlobalIC':
      return CSSColor.blue;
    case 'StoreIC':
      return CSSColor.orange;
    case 'KeyedLoadIC':
      return CSSColor.red;
    case 'KeyedStoreIC':
      return CSSColor.yellow;
  }
  return CSSColor.secondaryColor;
}

class DOM {
  static div(classes) {
    const node = document.createElement('div');
    if (classes !== void 0) {
      if (typeof classes === 'string') {
        node.classList.add(classes);
      } else {
        classes.forEach(cls => node.classList.add(cls));
      }
    }
    return node;
  }

  static table(className) {
    const node = document.createElement('table');
    if (className) node.classList.add(className);
    return node;
  }

  static td(textOrNode, className) {
    const node = document.createElement('td');
    if (typeof textOrNode === 'object') {
      node.appendChild(textOrNode);
    } else if (textOrNode) {
      node.innerText = textOrNode;
    }
    if (className) node.classList.add(className);
    return node;
  }

  static tr(className) {
    const node = document.createElement('tr');
    if (className) node.classList.add(className);
    return node;
  }

  static text(string) {
    return document.createTextNode(string);
  }

  static removeAllChildren(node) {
    let range = document.createRange();
    range.selectNodeContents(node);
    range.deleteContents();
  }

  static defineCustomElement(path, generator) {
    let name = path.substring(path.lastIndexOf('/') + 1, path.length);
    path = path + '-template.html';
    fetch(path)
        .then(stream => stream.text())
        .then(
            templateText =>
                customElements.define(name, generator(templateText)));
  }
}

function $(id) {
  return document.querySelector(id)
}

class V8CustomElement extends HTMLElement {
  _updateTimeoutId;
  _updateCallback = this._update.bind(this);

  constructor(templateText) {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});
    shadowRoot.innerHTML = templateText;
  }

  $(id) {
    return this.shadowRoot.querySelector(id);
  }

  querySelectorAll(query) {
    return this.shadowRoot.querySelectorAll(query);
  }

  update() {
    // Use timeout tasks to asynchronously update the UI without blocking.
    clearTimeout(this._updateTimeoutId);
    const kDelayMs = 5;
    this._updateTimeoutId = setTimeout(this._updateCallback, kDelayMs);
  }

  _update() {
    throw Error('Subclass responsibility');
  }
}

class LazyTable {
  constructor(table, rowData, rowElementCreator) {
    this._table = table;
    this._rowData = rowData;
    this._rowElementCreator = rowElementCreator;
    const tbody = table.querySelector('tbody');
    table.replaceChild(document.createElement('tbody'), tbody);
    table.querySelector('tfoot td').onclick = (e) => this._addMoreRows();
    this._addMoreRows();
  }

  _nextRowDataSlice() {
    return this._rowData.splice(0, 100);
  }

  _addMoreRows() {
    const fragment = new DocumentFragment();
    for (let row of this._nextRowDataSlice()) {
      const tr = this._rowElementCreator(row);
      fragment.appendChild(tr);
    }
    this._table.querySelector('tbody').appendChild(fragment);
  }
}

function delay(time) {
  return new Promise(resolver => setTimeout(resolver, time));
}

export {
  DOM,
  $,
  V8CustomElement,
  formatBytes,
  typeToColor,
  CSSColor,
  delay,
  LazyTable,
};
