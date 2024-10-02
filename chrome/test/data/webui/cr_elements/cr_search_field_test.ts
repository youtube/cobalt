// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';

import {CrSearchFieldElement} from 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertNotReached, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
// clang-format on

/** @fileoverview Suite of tests for cr-search-field. */
suite('cr-search-field', function() {
  let field: CrSearchFieldElement;
  let searches: string[]|null = null;

  function simulateSearch(term: string) {
    field.$.searchInput.value = term;
    field.onSearchTermInput();
    field.onSearchTermSearch();
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Ensure svg, which is referred to by a relative URL, is loaded from
    // chrome://resources and not chrome://test
    const base = document.createElement('base');
    base.href = 'chrome://resources/cr_elements/';
    document.head.appendChild(base);
    field = document.createElement('cr-search-field');
    searches = [];
    field.addEventListener('search-changed', function(event) {
      searches!.push((event as CustomEvent<string>).detail);
    });
    document.body.appendChild(field);
  });

  teardown(function() {
    searches = null;
  });

  // Test that no initial 'search-changed' event is fired during
  // construction and initialization of the cr-toolbar-search-field element.
  test('no initial search-changed event', function() {
    const onSearchChanged = () => {
      assertNotReached('Should not have fired search-changed event');
    };

    // Need to attach listener event before the element is created, to catch
    // the unnecessary initial event.
    document.body.addEventListener('search-changed', onSearchChanged);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(document.createElement('cr-search-field'));
    // Remove event listener on |body| so that other tests are not affected.
    document.body.removeEventListener('search-changed', onSearchChanged);
  });

  test('clear search button clears space and refocuses input', async () => {
    field.click();

    simulateSearch(' ');
    flush();
    assertTrue(field.hasSearchText);

    field.$.clearSearch.click();
    assertEquals('', field.getValue());
    await flushTasks();
    assertEquals(field.$.searchInput, field.shadowRoot!.activeElement);
    assertFalse(field.hasSearchText);
  });

  test('clear search button clears and refocuses input', async () => {
    field.click();

    simulateSearch('query1');
    flush();
    assertTrue(field.hasSearchText);

    field.$.clearSearch.click();
    assertEquals('', field.getValue());
    await flushTasks();
    assertEquals(field.$.searchInput, field.shadowRoot!.activeElement);
    assertFalse(field.hasSearchText);
  });

  test('notifies on new searches and setValue', function() {
    field.click();
    simulateSearch('query1');
    flush();
    assertEquals('query1', field.getValue());

    field.$.clearSearch.click();
    assertEquals('', field.getValue());

    simulateSearch('query2');
    // Ignore identical query.
    simulateSearch('query2');

    field.setValue('foo');
    // Ignore identical query.
    field.setValue('foo');

    field.setValue('');
    assertDeepEquals(['query1', '', 'query2', 'foo', ''], searches);
  });

  test('does not notify on setValue with noEvent=true', function() {
    field.click();
    field.setValue('foo', true);
    field.setValue('bar');
    field.setValue('baz', true);
    assertDeepEquals(['bar'], searches);
  });

  test('setValue will return early if the query has not changed', () => {
    // Need a space at the end, since the effective query will strip the spaces
    // at the beginning, but not at the end of the query.
    const value = 'test ';
    assertNotEquals(value, field.getValue());
    let calledSetValue = false;
    field.onSearchTermInput = () => {
      if (!calledSetValue) {
        calledSetValue = true;
        field.setValue(value);
      }
    };
    field.setValue(value, true);
    field.setValue(`  ${value}  `);
    assertTrue(calledSetValue);
    assertEquals(0, searches!.length);
  });
});
