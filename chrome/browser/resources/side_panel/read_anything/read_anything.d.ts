// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Definitions for chrome.readingMode API */

declare namespace chrome {
  export namespace readingMode {
    /////////////////////////////////////////////////////////////////////
    // Implemented in read_anything_app_controller.cc and consumed by ts.
    /////////////////////////////////////////////////////////////////////

    // The root AXNodeID of the tree to be displayed.
    let rootId: number;

    // Selection information. The selection must be a forward selection, with
    // the start node and offset before the end node and offset. Through
    // experimentation, it was observed that programmatically created backwards
    // ranges are collapsed to the start node. A test was added in
    // read_anything_app_test to confirm this assumption.
    let startNodeId: number;
    let startOffset: number;
    let endNodeId: number;
    let endOffset: number;

    // Items in the ReadAnythingTheme struct, see read_anything.mojom for info.
    let fontName: string;
    let fontSize: number;
    let foregroundColor: number;
    let backgroundColor: number;
    let lineSpacing: number;
    let letterSpacing: number;

    // The current color theme value.
    let colorTheme: number;

    // Current audio settings values.
    let speechRate: number;
    let highlightGranularity: number;

    // Enum values for various visual theme changes.
    let standardLineSpacing: number;
    let looseLineSpacing: number;
    let veryLooseLineSpacing: number;
    let standardLetterSpacing: number;
    let wideLetterSpacing: number;
    let veryWideLetterSpacing: number;
    let defaultTheme: number;
    let lightTheme: number;
    let darkTheme: number;
    let yellowTheme: number;
    let blueTheme: number;
    let highlightOn: number;

    // Whether the WebUI toolbar feature flag is enabled.
    let isWebUIToolbarVisible: boolean;

    // Whether the Read Aloud feature flag is enabled.
    let isReadAloudEnabled: boolean;

    // Indicates if select-to-distill works on the web page. Used to
    // determine which empty state to display.
    let isSelectable: boolean;

    // Fonts supported by the browser's preferred language.
    let supportedFonts: string[];

    // The language code that should be used for speech synthesis voices.
    let speechSynthesisLanguageCode: string;

    // Returns the stored user voice preference for the given language.
    function getStoredVoice(lang: string): string;

    // Returns a list of AXNodeIDs corresponding to the unignored children of
    // the AXNode for the provided AXNodeID. If there is a selection contained
    // in this node, only returns children which are partially or entirely
    // contained within the selection.
    function getChildren(nodeId: number): number[];

    // Returns the HTML tag of the AXNode for the provided AXNodeID.
    function getHtmlTag(nodeId: number): string;

    // Returns the language of the AXNode for the provided AXNodeID.
    function getLanguage(nodeId: number): string;

    // Returns the text content of the AXNode for the provided AXNodeID. If a
    // selection begins or ends in this node, truncates the text to only return
    // the selected text.
    function getTextContent(nodeId: number): string;

    // Returns the text direction of the AXNode for the provided AXNodeID.
    function getTextDirection(nodeId: number): string;

    // Returns the url of the AXNode for the provided AXNodeID.
    function getUrl(nodeId: number): string;

    // Returns true if the text node / element should be bolded.
    function shouldBold(nodeId: number): boolean;

    // Returns true if the element has overline text styling.
    function isOverline(nodeId: number): boolean;

    // Connects to the browser process. Called by ts when the read anything
    // element is added to the document.
    function onConnected(): void;

    // Called when a user tries to copy text from reading mode with keyboard
    // shortcuts.
    function onCopy(): void;

    // Called when the Read Anything panel is scrolled.
    function onScroll(onSelection: boolean): void;

    // Called when a user clicks a link. NodeID is an AXNodeID which identifies
    // the link's corresponding AXNode in the main pane.
    function onLinkClicked(nodeId: number): void;

    // Called when the line spacing is changed via the webui toolbar.
    function onStandardLineSpacing(): void;
    function onLooseLineSpacing(): void;
    function onVeryLooseLineSpacing(): void;

    // Called when a user makes a font size change via the webui toolbar.
    function onFontSizeChanged(increase: boolean): void;
    function onFontSizeReset(): void;

    // Called when the letter spacing is changed via the webui toolbar.
    function onStandardLetterSpacing(): void;
    function onWideLetterSpacing(): void;
    function onVeryWideLetterSpacing(): void;

    // Called when the color theme is changed via the webui toolbar.
    function onDefaultTheme(): void;
    function onLightTheme(): void;
    function onDarkTheme(): void;
    function onYellowTheme(): void;
    function onBlueTheme(): void;

    // Called when the font is changed via the webui toolbar.
    function onFontChange(font: string): void;

    // Called when the speech rate is changed via the webui toolbar.
    function onSpeechRateChange(rate: number): void;

    // Called when the voice used for speech is changed via the webui toolbar.
    function onVoiceChange(voice: string, lang: string): void;

    // Called when the highlight granularity is changed via the webui toolbar.
    function turnedHighlightOn(): void;
    function turnedHighlightOff(): void;

    // Returns the actual spacing value to use based on the given lineSpacing
    // category.
    function getLineSpacingValue(lineSpacing: number): number;

    // Returns the actual spacing value to use based on the given letterSpacing
    // category.
    function getLetterSpacingValue(letterSpacing: number): number;

    // Called when a user makes a selection change. AnchorNodeID and
    // focusAXNodeID are AXNodeIDs which identify the anchor and focus AXNodes
    // in the main pane. The selection can either be forward or backwards.
    function onSelectionChange(
        anchorNodeId: number, anchorOffset: number, focusNodeId: number,
        focusOffset: number): void;
    // Called when a user collapses the selection. This is usually accomplished
    // by clicking.
    function onCollapseSelection(): void;

    // Set the content. Used by tests only.
    // SnapshotLite is a data structure which resembles an AXTreeUpdate. E.g.:
    //   const axTree = {
    //     rootId: 1,
    //     nodes: [
    //       {
    //         id: 1,
    //         role: 'rootWebArea',
    //         childIds: [2],
    //       },
    //       {
    //         id: 2,
    //         role: 'staticText',
    //         name: 'Some text.',
    //       },
    //     ],
    //   };
    function setContentForTesting(
        snapshotLite: Object, contentNodeIds: number[]): void;

    // Set the theme. Used by tests only.
    function setThemeForTesting(
        fontName: string, fontSize: number, foregroundColor: number,
        backgroundColor: number, lineSpacing: number,
        letterSpacing: number): void;

    // Sets the default language. Used by tests only.
    function setLanguageForTesting(code: string): void;

    ////////////////////////////////////////////////////////////////
    // Implemented in read_anything/app.ts and called by native c++.
    ////////////////////////////////////////////////////////////////

    // Display a loading screen to tell the user we are distilling the page.
    function showLoading(): void;

    // Display the empty state page to tell the user we can't distill the page.
    function showEmpty(): void;

    // Ping that an AXTree has been distilled for the active tab's render frame
    // and is available to consume.
    function updateContent(): void;

    // Ping that the selection has been updated.
    function updateSelection(): void;

    // Ping that the theme choices of the user have been changed using the
    // toolbar and are ready to consume.
    function updateTheme(): void;

    // Ping that the theme choices of the user have been retrieved from
    // preferences and can be used to set up the page.
    function restoreSettingsFromPrefs(): void;

    // Returns the index of the next sentence of the given text, such that the
    // next sentence is equivalent to text.substr(0, <returned_index>).
    // If the sentence exceeds the maximum text length, the sentence will be
    // cropped to the nearest word boundary that doesn't exceed the maximum
    // text length.
    function getNextSentence(value: string, maxTextLength: number): number;

    // Signal that the supported fonts should be updated i.e. that the brower's
    // preferred language has changed.
    function updateFonts(): void;
  }
}
