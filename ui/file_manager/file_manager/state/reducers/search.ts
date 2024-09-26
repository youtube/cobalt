// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SearchData, SearchOptions, State} from '../../externs/ts/state.js';
import {SearchAction} from '../actions/search.js';

/**
 * Helper function that does a deep comparison between two SearchOptions.
 */
function optionsChanged(
    stored: SearchOptions|undefined, fresh: SearchOptions|undefined): boolean {
  if (fresh === undefined) {
    // If fresh options are undefined, that means keep the stored options. No
    // matter what the stored options are, we are saying they have not changed.
    return false;
  }
  if (stored === undefined) {
    return true;
  }
  return fresh.location !== stored.location ||
      fresh.recency !== stored.recency ||
      fresh.fileCategory !== stored.fileCategory;
}

export function search(state: State, action: SearchAction): State {
  const payload = action.payload;
  const blankSearch = {
    query: undefined,
    status: undefined,
    options: undefined,
  };
  // Special case: if none of the fields are set, the action clears the search
  // state in the store.
  if (Object.values(payload).every(field => field === undefined)) {
    return {
      ...state,
      search: blankSearch,
    };
  }

  const currentSearch = state.search || blankSearch;
  // Create a clone of current search. We must not modify the original object,
  // as store customers are free to cache it and check for changes. If we modify
  // the original object the check for changes incorrectly return false.
  const search: SearchData = {...currentSearch};
  let changed = false;
  if (payload.query !== undefined && payload.query !== currentSearch.query) {
    search.query = payload.query;
    changed = true;
  }
  if (payload.status !== undefined && payload.status !== currentSearch.status) {
    search.status = payload.status;
    changed = true;
  }
  if (optionsChanged(currentSearch.options, payload.options)) {
    search.options = {...payload.options} as SearchOptions;
    changed = true;
  }
  return changed ? {...state, search} : state;
}
