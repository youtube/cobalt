// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ScrollToParams} from '../../glic_api/glic_api.js';
import {defInterface, defMessage} from '../transport/messaging.js';

// Shared between host and client.

export const AnnotationHostDef = defInterface({
  name: 'AnnotationHost',
  methods: [
    {
      name: 'scrollTo',
      request: defMessage<{
        params: ScrollToParams,
      }>(),
      histogram: {id: 45},
    },
    {
      name: 'dropScrollToHighlight',
      request: defMessage<void>(),
      histogram: {id: 57},
    },
  ],
});
export type AnnotationHost = typeof AnnotationHostDef;

export const AnnotationClientDef = defInterface({
  name: 'AnnotationClient',
  methods: [],
});
export type AnnotationClient = typeof AnnotationClientDef;
