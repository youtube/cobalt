// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var CANNOT_OPEN_DB = -1;
var SETUP_FAILED = -2;
var TEST_FAILED = -3;

function setup() {
  if ('webkitIndexedDB' in window) {
    indexedDB = webkitIndexedDB;
    IDBCursor = webkitIDBCursor;
    IDBKeyRange = webkitIDBKeyRange;
    IDBTransaction = webkitIDBTransaction;
    return true;
  }

  if ('indexedDB' in window)
    return true;

  return false;
}

function log(msg) {
  document.write(msg + '<br>');
}
