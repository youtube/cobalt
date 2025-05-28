// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {removeQueryAndReferenceFromURL, sendWebKitMessage, trim} from '//ios/web/public/js_messaging/resources/utils.js'

gCrWeb.utils_tests = {
  removeQueryAndReferenceFromURL,
  sendWebKitMessage,
  trim,
};
