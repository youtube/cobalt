// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class MetadataRequest {
  /**
   * @param names Property name list to be requested.
   */
  constructor(public entry: Entry, public names: string[]) {}
}
