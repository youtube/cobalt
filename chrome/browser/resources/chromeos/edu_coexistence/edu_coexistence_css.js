// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/chromeos/cros_color_overrides.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const styleMod = document.createElement('dom-module');
styleMod.appendChild(html`{__html_template__}`.content);
styleMod.register('edu-coexistence-css');
