// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface HasMojoConnection {
  onConnectionError: {addListener: (l: Function) => number};
  $: {close(): void};
}
export interface HasPostMessagePipe {
  addCloseHandler(f: Function): void;
  close(): void;
}

// Automatically closes all pipes when one of them is closed.
export function linkPipeClosure(
    ...entries: Array<HasPostMessagePipe|HasMojoConnection>) {
  let activeEntries: Array<HasPostMessagePipe|HasMojoConnection>|null = entries;
  const destroy = () => {
    if (!activeEntries) {
      return;
    }
    for (const entry of activeEntries) {
      if ((entry as Partial<HasMojoConnection>).$) {
        (entry as HasMojoConnection).$.close();
      } else {
        (entry as HasPostMessagePipe).close();
      }
    }
    activeEntries = null;
  };

  for (const entry of entries) {
    if ((entry as Partial<HasMojoConnection>).$) {
      (entry as HasMojoConnection).onConnectionError.addListener(() => {
        destroy();
      });
    } else {
      (entry as HasPostMessagePipe).addCloseHandler(() => {
        destroy();
      });
    }
  }
}
