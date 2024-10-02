// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const styleElement = document.createElement('dom-module');
styleElement.setAttribute('assetpath', 'chrome://resources/');
styleElement.innerHTML = `{__html_template__}`;
styleElement.register('device-emulator-shared-styles');
