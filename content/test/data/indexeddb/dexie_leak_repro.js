// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep references to prevent GC (mimicking Dexie's connection leak)
let leakedConnections = [];

function openConnection(name) {
  return new Promise((resolve, reject) => {
    let req = indexedDB.open(name, 1);
    req.onsuccess = (e) => {
      let db = e.target.result;
      leakedConnections.push(db); // Simulate leaking a reference.
      resolve("success");
    };
    req.onerror = (e) => {
      reject(e.target.error.name);
    };
    req.onblocked = (e) => {
      // If blocked, we might resolve or reject, but for repro we expect
      // success.
      console.warn("Open blocked");
    };
  });
}

async function runSequentialLeak(count) {
  try {
    for (let i = 0; i < count; i++) {
      await openConnection('repro_db_seq');
    }
    return "done";
  } catch (e) {
    return "error: " + e;
  }
}

async function runConcurrentLeak(count) {
  try {
    let promises = [];
    for (let i = 0; i < count; i++) {
      promises.push(openConnection('repro_db_con'));
    }
    await Promise.all(promises);
    return "done";
  } catch (e) {
    return "error: " + e;
  }
}
