// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {catchAndReportErrors} from '//ios/web/public/js_messaging/resources/error_reporting.js';
import {gCrWebLegacy} from '//ios/web/public/js_messaging/resources/gcrweb.js';

for (const namespace in gCrWebLegacy) {
  const namespaceObject = gCrWebLegacy[namespace];
  for (const itemName in namespaceObject) {
    const exposedItem = namespaceObject[itemName];
    if (typeof exposedItem === 'function') {
      const funcName = '__gCrWeb.' + namespace + '.' + itemName;
      gCrWebLegacy[namespace][itemName] = function(...args: unknown[]) {
        return catchAndReportErrors.apply(null, [funcName, exposedItem, args]);
      };
    }
  }
}
