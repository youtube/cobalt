// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Metrics calls to start measurement of script loading.  Include
 * this as the first script in main.html (i.e. after the common scripts that
 * define the metrics namespace).
 */

import {metrics} from '../../common/js/metrics.js';

metrics.startInterval('Load.Total');
metrics.startInterval('Load.Script');
