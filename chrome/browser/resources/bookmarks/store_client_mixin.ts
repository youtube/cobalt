// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {makeStoreClientMixin, StoreClientInterface} from 'chrome://resources/cr_elements/store_client/store_client.js';
import {Action} from 'chrome://resources/js/store_ts.js';

import {Store} from './store.js';
import {BookmarksPageState} from './types.js';

// A Bookmarks specific specialization of `StoreClientInterface`.
export interface StoreClientMixinInterface extends
    StoreClientInterface<BookmarksPageState, Action> {}

// `StoreClientMixin` binds Polymer elements to the Bookmarks store instance.
export const StoreClientMixin = makeStoreClientMixin(Store.getInstance);
