// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture for ISearch.
 */
ChromeVoxISearchTest = class extends ChromeVoxE2ETest {
  /** @override */
  get runtimeDeps() {
    return ['ISearch', 'ISearchHandler'];
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    // Alphabetical based on file path.
    await importModule('ISearch', '/chromevox/background/panel/i_search.js');
    await importModule('Cursor', '/common/cursors/cursor.js');
  }

  get linksAndHeadingsDoc() {
    return `
      <p>start</p>
      <a href='#a'>Home</a>
      <a href='#b'>About US</a>
      <p>
        <h1>Latest Breaking News</h1>
        <a href='foo'>See more...</a>
      </p>
      <a href='#bar'>Questions?</a>
      <h2>Privacy Policy</h2>
      <p>end<span>of test</span></p>
    `;
  }
};


/**
 * @implements {ISearchHandler}
 */
class FakeISearchHandler {
  constructor(testObj) {
    this.test = testObj;
    this.expect_ = [];
  }

  /** @override */
  onSearchReachedBoundary(boundaryNode) {
    this.expect_.shift()({node: boundaryNode, isBoundary: true});
  }

  /** @override */
  onSearchResultChanged(node, start, end) {
    this.expect_.shift()({node, start, end});
  }

  expect(str, opt_callback) {
    this.expect_.push(this.test.newCallback(function(args) {
      const node = args.node;
      let actual = node.name || node.role;
      if (args.start && args.end) {
        actual = 'start=' + args.start + ' end=' + args.end + ' text=' + actual;
      }
      if (args.isBoundary) {
        actual = 'boundary=' + actual;
      }
      assertEquals(str, actual);
      opt_callback && opt_callback();
    }));
  }
}


AX_TEST_F('ChromeVoxISearchTest', 'Simple', async function() {
  const rootNode = await this.runWithLoadedTree(this.linksAndHeadingsDoc);
  const handler = new FakeISearchHandler(this);
  const search = new ISearch(new Cursor(rootNode, 0));
  search.handler = handler;

  // Simple forward search.
  search.search('US', 'forward');
  handler.expect(
      'start=6 end=8 text=About US',
      search.search.bind(search, 'start', 'backward'));

  handler.expect(
      'start',
      // Boundary (beginning).
      search.search.bind(search, 'foo', 'backward'));

  handler.expect(
      'boundary=start',
      // Boundary (end).
      search.search.bind(search, 'foo', 'forward'));

  // Search "focus" doesn't move.
  handler.expect(
      'boundary=start',
      // Mixed case substring.
      search.search.bind(search, 'bReak', 'forward'));

  handler.expect(
      'start=7 end=12 text=Latest Breaking News',
      search.search.bind(search, 'bReaki', 'forward'));

  // Incremental search stays on the current node.
  handler.expect(
      'start=7 end=13 text=Latest Breaking News',
      search.search.bind(search, 'bReakio', 'forward'));

  // No results for the search.
  handler.expect('boundary=Latest Breaking News');
});
