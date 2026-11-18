// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ItemData, SplitViewData, TabData, TabGroupData} from './tab_data.js';
import {TabSearchApiProxyImpl} from './tab_search_api_proxy.js';

export interface OptionKeyObject {
  name: string;
  getter: (data: TabData|TabGroupData|SplitViewData) => string | undefined;
  weight: number;
}

export interface SearchOptions {
  includeScore?: boolean;
  includeMatches?: boolean;
  ignoreLocation?: boolean;
  threshold?: number;
  distance?: number;
  keys: OptionKeyObject[];
}

/**
 * @return A new array of entries satisfying the input. If no search input is
 *     present, returns a shallow copy of the records.
 */
export async function search<T extends ItemData>(
    input: string, records: T[], options: SearchOptions): Promise<T[]> {
  if (input.length === 0) {
    return [...records];
  }
  const searchStartTime = Date.now();
  const result = await exactSearch(input, records, options);
  chrome.metricsPrivate.recordTime(
      'Tabs.TabSearch.WebUI.SearchAlgorithmDuration',
      Math.round(Date.now() - searchStartTime));
  return result;
}

function cloneTabDataObj<T extends ItemData>(tabData: T): T {
  const clone = Object.assign({}, tabData);
  clone.highlightRanges = {};
  Object.setPrototypeOf(clone, Object.getPrototypeOf(tabData));

  return clone;
}

/**
 * The exact match algorithm returns records ranked according to priorities
 * and scores. The search is case-insensitive and diacritic-insensitive (accents
 * are folded), offloading the character matching to the browser process via
 * Mojo.
 *
 * Records are ordered by priority (higher priority comes first) and sorted by
 * score within the same priority. See `scoringFunction` for how to calculate
 * score and `prioritizeMatchResult` for how to calculate priority.
 */
async function exactSearch<T extends ItemData>(
    searchText: string, records: T[], options: SearchOptions): Promise<T[]> {
  if (searchText.length === 0) {
    return records;
  }

  // Default distance to calculate score for search fields based on match
  // position.
  const defaultDistance = 200;
  const distance = options.distance || defaultDistance;

  // Controls how heavily weighted the search field weights are relative to each
  // other in the scoring function.
  const searchFieldWeights = (options.keys).reduce((acc, {name, weight}) => {
    acc[name] = weight;
    return acc;
  }, {} as {[key: string]: number});

  // Batch all strings to search in.
  const targets: string[] = [];
  for (const record of records) {
    for (const searchField of options.keys) {
      const fieldText = searchField.getter(record as TabData | TabGroupData);
      if (fieldText) {
        targets.push(fieldText);
      }
    }
  }

  // Query the browser process to find match ranges. The C++ backend handles
  // case-insensitivity, diacritic-insensitivity, and quotation folding
  // natively.
  const {ranges} =
      await TabSearchApiProxyImpl.getInstance().getRangesIgnoringCaseAndAccents(
          searchText, targets);

  // Perform an exact match search with range discovery.
  const exactMatches = [];
  let targetIdx = 0;
  for (const tabDataRecord of records) {
    let matchFound = false;
    const matchedRecord = cloneTabDataObj(tabDataRecord);
    // Searches for fields or nested fields in the record.
    for (const searchField of options.keys) {
      const fieldText =
          searchField.getter(tabDataRecord as TabData | TabGroupData);
      if (fieldText) {
        const matchRanges = ranges[targetIdx++];
        if (matchRanges && matchRanges.length !== 0) {
          // Convert Mojo TokenRange DTOs (Data Transfer Objects) to plain JS
          // Range domain objects. This decouples the UI layers from
          // Mojo-specific serialization internals and ensures type safety.
          matchedRecord.highlightRanges[searchField.name] =
              matchRanges.map(r => ({start: r.start, length: r.length}));
          matchFound = true;
        }
      }
    }

    if (matchFound) {
      exactMatches.push({
        tab: matchedRecord,
        score: scoringFunction(matchedRecord, distance, searchFieldWeights),
      });
    }
  }

  // Sort by score.
  exactMatches.sort((a, b) => (b.score - a.score));

  // Reorder match result by priorities.
  return prioritizeMatchResult(
      options.keys, exactMatches.map(item => item.tab));
}

/**
 * A scoring function based on match indices of specified search fields.
 * Matches near the beginning of the string will have a higher score than
 * matches near the end of the string. Multiple matches will have a higher score
 * than single matches.
 */
function scoringFunction(
    tabData: ItemData, distance: number,
    searchFieldWeights: {[key: string]: number}) {
  let score = 0;
  // For every match, map the match index in [0, distance] to a scalar value in
  // [1, 0].
  for (const key in searchFieldWeights) {
    if (tabData.highlightRanges[key]) {
      for (const {start} of tabData.highlightRanges[key]) {
        score += Math.max((distance - start) / distance, 0) *
            searchFieldWeights[key]!;
      }
    }
  }

  return score;
}

/**
 * Reorder match result based on priorities (highest to lowest priority):
 * 1. All items with a search key matching the searchText at the beginning of
 *    the string.
 * 2. All items with a search key matching the searchText at the beginning of a
 *    word in the string.
 * 3. All remaining items with a search key matching the searchText elsewhere in
 *    the string.
 */
function prioritizeMatchResult<T extends ItemData>(
    searchFields: OptionKeyObject[], result: T[]): T[] {
  const itemsMatchingStringStart = [];
  const itemsMatchingWordStart = [];
  const others = [];

  for (const tab of result) {
    let matchesStringStart = false;
    let matchesWordStart = false;

    for (const searchField of searchFields) {
      const matchRanges = tab.highlightRanges[searchField.name];
      if (!matchRanges || matchRanges.length === 0) {
        continue;
      }

      const fieldText = searchField.getter(tab as TabData | TabGroupData);
      if (!fieldText) {
        continue;
      }

      for (const matchRange of matchRanges) {
        if (matchRange.start === 0) {
          matchesStringStart = true;
          break;
        }
        if (matchRange.start > 0) {
          const prevChar = fieldText.charAt(matchRange.start - 1);
          // Non-word character check (rough approximation of \b)
          if (/\W/.test(prevChar)) {
            matchesWordStart = true;
          }
        }
      }
      // We can only early exit if we found a String Start match,
      // because a Word Start match in this field could still be overridden
      // by a String Start match in a subsequent field.
      if (matchesStringStart) {
        break;
      }
    }

    if (matchesStringStart) {
      itemsMatchingStringStart.push(tab);
    } else if (matchesWordStart) {
      itemsMatchingWordStart.push(tab);
    } else {
      others.push(tab);
    }
  }
  return itemsMatchingStringStart.concat(itemsMatchingWordStart, others);
}
