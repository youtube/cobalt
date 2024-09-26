// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A base class for Tts living on Chrome platforms.
 *
 */

goog.provide('cvox.ChromeTtsBase');

goog.require('cvox.AbstractTts');


/**
 * @constructor
 * @extends {cvox.AbstractTts}
 */
cvox.ChromeTtsBase = function() {
  goog.base(this);
  this.propertyDefault['pitch'] = 1;
  this.propertyMin['pitch'] = 0.2;
  this.propertyMax['pitch'] = 2.0;

  this.propertyDefault['rate'] = 1;
  this.propertyMin['rate'] = 0.2;
  this.propertyMax['rate'] = 5.0;

  this.propertyDefault['volume'] = 1;
  this.propertyMin['volume'] = 0.2;
  this.propertyMax['volume'] = 1.0;
};
goog.inherits(cvox.ChromeTtsBase, cvox.AbstractTts);
