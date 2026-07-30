// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const dbName = 'corrupt-index';

function test() {
  let created = false;
  const request = indexedDB.open(dbName);
  request.onerror = unexpectedErrorCallback;
  request.onupgradeneeded = (event) => {
    created = true;
    const db = event.target.result;
    const indexedStore = db.createObjectStore('indexed_store', {keyPath: 'id'});
    indexedStore.createIndex('index', 'indexedValue');
    indexedStore.put({id: 1, indexedValue: 'lookup'});
    db.createObjectStore('plain_store', {keyPath: 'id'});
  };
  request.onsuccess = (event) => {
    const db = event.target.result;
    if (created) {
      db.close();
      done('database created');
      return;
    }
    triggerCorruptionDetection(db);
  };
}

function triggerCorruptionDetection(db) {
  let stopKeepingAlive = () => {};
  if (location.search.includes('concurrent')) {
    // Hold a rollback-able transaction open so it is still active when the
    // corrupt read forces the database closed.
    const writeTransaction = db.transaction('plain_store', 'readwrite');
    writeTransaction.objectStore('plain_store').put({id: 1});
    stopKeepingAlive = keepAlive(writeTransaction, 'plain_store');
    writeTransaction.oncomplete = () =>
        fail('write transaction committed; corruption was not detected');
  }

  const request = db.transaction('indexed_store', 'readonly')
                      .objectStore('indexed_store')
                      .index('index')
                      .get('lookup');
  request.onsuccess = () => {
    fail('index read succeeded; corruption was not detected');
  };
  request.onerror = () => {
    stopKeepingAlive();
    db.close();
    setTimeout(verifyRecovered, 0);
  };
}

function verifyRecovered() {
  const request = indexedDB.open(dbName);
  request.onerror = unexpectedErrorCallback;
  request.onupgradeneeded = () =>
      fail('database was razed; expected recovery to preserve it');
  request.onsuccess = (event) => {
    const db = event.target.result;
    const request = db.transaction('indexed_store', 'readonly')
                        .objectStore('indexed_store')
                        .index('index')
                        .get('lookup');
    request.onsuccess = () => {
      db.close();
      const record = request.result;
      if (record && record.id === 1 && record.indexedValue === 'lookup') {
        done('database recovered; index read returned the expected record');
      } else {
        fail('index read returned an unexpected result: ' +
             JSON.stringify(record));
      }
    };
    request.onerror = () =>
        fail('index read failed after reopen; database was not recovered');
  };
}
