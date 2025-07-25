// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Testing implementation of host.js.
 *
 */

goog.provide('cvox.TestHost');

goog.require('cvox.AbstractHost');
goog.require('cvox.HostFactory');

/**
 * @constructor
 * @extends {cvox.AbstractHost}
 */
cvox.TestHost = function() {
  cvox.AbstractHost.call(this);
};
goog.inherits(cvox.TestHost, cvox.AbstractHost);

/** @override */
cvox.HostFactory.hostConstructor = cvox.TestHost;
