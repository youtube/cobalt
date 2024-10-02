// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview
 * UI glue code and instances (including singletons).
 */

(function() {
  /** @type {ProgressBar} */
  const _progress = new ProgressBar(g_el.progAppbar);

  /** @type {!SymbolTreeUi} */
  const _symbolTreeUi = new SymbolTreeUi();
  _symbolTreeUi.init();

  /** @type {!MetricsTreeModel} */
  const _metricsTreeModel = new MetricsTreeModel();

  /** @type {!MetricsTreeUi} */
  const _metricsTreeUi = new MetricsTreeUi(_metricsTreeModel);
  _metricsTreeUi.init();

  /** @param {TreeProgress} message */
  function onProgressMessage(message) {
    const {error, percent} = message;
    _progress.setValue(percent);
    document.body.classList.toggle('error', Boolean(error));
  }

  /**
   * Processes response of an initial load / upload.
   * @param {BuildTreeResults} message
   */
  function processLoadTreeResponse(message) {
    const {diffMode} = message;
    const {beforeBlobUrl, loadBlobUrl, isMultiContainer, metadata} =
        message.loadResults;
    console.log(
        '%cPro Tip: %cawait supersize.worker.openNode("$FILE_PATH")',
        'font-weight:bold;color:red;', '')

    displayOrHideDownloadButton(beforeBlobUrl, loadBlobUrl);

    state.setDiffMode(diffMode);
    document.body.classList.toggle('diff', Boolean(diffMode));

    processBuildTreeResponse(message);
    renderAndShowMetricsTree(metadata);
    setMetadataContent(metadata);
    g_el.divMetadataView.classList.toggle('active', true);
    setReviewInfo(metadata);
  }

  /**
   * Sets the review URL and title from message to the HTML element.
   * @param {MetadataType} metadata
   */
  function setReviewInfo(metadata) {
    const processReviewInfo = (field) => {
      const urlExists = Boolean(
          field?.hasOwnProperty('url') && field?.hasOwnProperty('title'));
      if (urlExists) {
        g_el.linkReviewText.href = field['url'];
        g_el.linkReviewText.textContent = field['title'];
      }
      g_el.divReviewInfo.style.display = urlExists ? '' : 'none';
    };
    const sizeFile = metadata['size_file'];
    if (sizeFile?.hasOwnProperty('build_config')) {
      processReviewInfo(sizeFile['build_config'])
    }
  }

  /**
   * Processes the result of a buildTree() message.
   * @param {BuildTreeResults} message
   */
  function processBuildTreeResponse(message) {
    const {root} = message;
    _progress.setValue(1);

    const noSymbols = (Object.keys(root.childStats).length === 0);
    _symbolTreeUi.toggleNoSymbolsMessage(noSymbols);

    /** @type {?DocumentFragment} */
    let rootElement = null;
    if (root) {
      rootElement = _symbolTreeUi.makeNodeElement(root);
      /** @type {!HTMLAnchorElement} */
      const link = rootElement.querySelector('.node');
      // Expand the root UI node.
      link.click();
      link.tabIndex = 0;
    }

    // Double requestAnimationFrame ensures that the code inside executes in a
    // different frame than the above tree element creation.
    requestAnimationFrame(() => {
      requestAnimationFrame(() => {
        dom.replace(g_el.ulSymbolTree, rootElement);
      });
    });
  }

  /**
   * Displays/hides download buttons for loadUrl.size and beforeUrl.size.
   * @param {?string=} beforeUrl
   * @param {?string=} loadUrl
   */
  function displayOrHideDownloadButton(beforeUrl = null, loadUrl = null) {
    const updateAnchor = (anchor, url) => {
      anchor.style.display = url ? '' : 'none';
      if (anchor.href && anchor.href.startsWith('blob:')) {
        URL.revokeObjectURL(anchor.href);
      }
      anchor.href = url;
    };
    updateAnchor(g_el.linkDownloadBefore, beforeUrl);
    updateAnchor(g_el.linkDownloadLoad, loadUrl);

    if (/** @type {string} */ (state.stLoadUrl.get()).includes('.sizediff')) {
      g_el.linkDownloadLoad.title = 'Download .sizediff file';
      g_el.linkDownloadLoad.download = 'load_size.sizediff';
    }
  }

  /**
   * Extracts metrics data and renders the Metrics Tree.
   * @param {?Object} metadata Used to compute Metrics Tree data. If null, reuse
   *     cached data. Otherwise new data is saved to cache.
   */
  function renderAndShowMetricsTree(metadata) {
    _metricsTreeModel.updateFilter();
    _metricsTreeModel.extractAndStoreRoot(metadata);

    /** @type {?DocumentFragment} */
    let rootElement = null;
    if (_metricsTreeModel.rootNode) {
      rootElement = _metricsTreeUi.makeNodeElement(_metricsTreeModel.rootNode);
      /** @type {!HTMLAnchorElement} */
      const link = rootElement.querySelector('.node');
      // Expand the root UI node.
      link.click();
      link.tabIndex = 0;
    }

    dom.replace(g_el.ulMetricsTree, rootElement);
    g_el.divMetricsView.classList.toggle('active', true);
  }

  /**
   * Modifies metadata in-place so they render better.
   * @param {Object} metadata
   */
  function formatMetadataInPlace(metadata) {
    if (metadata?.hasOwnProperty('elf_mtime')) {
      const date = new Date(metadata['elf_mtime'] * 1000);
      metadata['elf_mtime'] = date.toString();
    }
  }

  /**
   * Renders the metadata for provided size file.
   * @param {MetadataType} sizeMetadata
   * @return {string}
   */
  function renderMetadata(sizeMetadata) {
    const processContainer = (container) => {
      if (container?.hasOwnProperty('metadata')) {
        formatMetadataInPlace(container['metadata']);
      }
      // Strip section_sizes because it is already shown in tree.
      if (container?.hasOwnProperty('section_sizes')) {
        delete container['section_sizes'];
      }
    };
    if (sizeMetadata?.hasOwnProperty('containers')) {
      for (const container of sizeMetadata['containers']) {
        processContainer(container);
      }
    } else {
      // Covers the case if the metadata is in old schema.
      processContainer(sizeMetadata);
    }
    return JSON.stringify(sizeMetadata, null, 2);
  }

  /**
   * Sets the metadata from message to the HTML element.
   * @param {MetadataType} metadata
   */
  function setMetadataContent(metadata) {
    let metadataStr = '';
    if (metadata) {
      const sizeMetadata = metadata['size_file'];
      const sizeMetadataStr = renderMetadata(sizeMetadata);
      if (metadata.hasOwnProperty('before_size_file')) {
        const beforeMetadata = metadata['before_size_file'];
        const beforeMetadataStr = renderMetadata(beforeMetadata);
        metadataStr =
            'Metadata for Before Size File:\n' + beforeMetadataStr + '\n\n\n';
      }
      metadataStr += 'Metadata for Load Size File:\n' + sizeMetadataStr;
    }
    g_el.preMetadataContent.textContent = metadataStr;
  }

  /** @param {!Array<!URL>} urlsToLoad */
  async function performInitialLoad(urlsToLoad) {
    let accessToken = null;
    _progress.setValue(0.1);
    if (requiresAuthentication(urlsToLoad)) {
      accessToken = await fetchAccessToken();
      _progress.setValue(0.2);
    }
    const worker = restartWorker(onProgressMessage);
    _progress.setValue(0.3);
    const message = await worker.loadAndBuildTree('from-url://', accessToken);
    processLoadTreeResponse(message);
  }

  async function rebuildTree() {
    _progress.setValue(0);
    const message = await window.supersize.worker.buildTree();
    processBuildTreeResponse(message);
    renderAndShowMetricsTree(null);
  }

  g_el.fileUpload.addEventListener('change', async (event) => {
    _progress.setValue(0.1);
    const input = /** @type {HTMLInputElement} */ (event.currentTarget);
    const file = input.files.item(0);
    const fileUrl = URL.createObjectURL(file);

    state.stLoadUrl.set(fileUrl);

    const worker = restartWorker(onProgressMessage);
    _progress.setValue(0.3);
    const message = await worker.loadAndBuildTree(fileUrl);
    URL.revokeObjectURL(fileUrl);
    processLoadTreeResponse(message);
    // Clean up afterwards so new files trigger event.
    input.value = '';
  });

  g_el.frmOptions.addEventListener('change', event => {
    // Update the tree when options change.
    // Some options update the tree themselves, don't regenerate when those
    // options (marked by "data-dynamic") are changed.
    if (!/** @type {HTMLElement} */ (event.target)
             .dataset.hasOwnProperty('dynamic')) {
      rebuildTree();
    }
  });
  g_el.frmOptions.addEventListener('submit', event => {
    event.preventDefault();
    rebuildTree();
  });

  // Toggles the metadata HTML element on click.
  g_el.divMetadataView.addEventListener('click', () => {
    g_el.preMetadataContent.classList.toggle('active');
  });

  const urlsToLoad = [];
  for (const url of [state.stBeforeUrl.get(), state.stLoadUrl.get()]) {
    if (url)
      urlsToLoad.push(new URL(/** @type {string} */ (url), document.baseURI));
  }
  if (urlsToLoad.length > 0)
    performInitialLoad(urlsToLoad);
})();
