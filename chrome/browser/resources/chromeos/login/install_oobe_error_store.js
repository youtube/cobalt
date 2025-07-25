// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class OobeErrorStore {
  static getInstance() {
    return OobeErrorStore.instance_ || (OobeErrorStore.instance_ = new OobeErrorStore());
  }

  constructor() {
    this.store_ = [];
    window.addEventListener('error', (e) => {
      // Add to the error store. This is used by tests that ensure no errors
      // are present by checking the length of this array.
      this.store_.push(e);

      // Additionally, print out the error with its stack information on the
      // console so that it appears in log files.
      if (e.error && e.error.stack) {
        console.error(e.error.stack);
      }
    });
  }

  get length() {
    return this.store_.length;
  }
}

window.OobeErrorStore = OobeErrorStore.getInstance();