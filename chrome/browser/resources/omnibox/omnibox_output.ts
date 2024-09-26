// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';

import {ACMatchClassification, AutocompleteAdditionalInfo, AutocompleteMatch, OmniboxResponse} from './omnibox.mojom-webui.js';
import {OmniboxElement} from './omnibox_element.js';
import {DisplayInputs, OmniboxInput} from './omnibox_input.js';

interface ResultsDetails {
  cursorPosition: number;
  time: number;
  done: boolean;
  type: string;
  host: string;
  isTypedHost: boolean;
}

function clearChildren(element: Element) {
  while (element.firstChild) {
    element.firstChild.remove();
  }
}

export class OmniboxOutput extends OmniboxElement {
  private selectedResponseIndex: number = 0;
  responsesHistory: OmniboxResponse[][] = [];
  private resultsGroups: OutputResultsGroup[] = [];
  private displayInputs: DisplayInputs = OmniboxInput.defaultDisplayInputs;
  private filterText: string = '';

  constructor() {
    super('omnibox-output-template');
  }

  updateDisplayInputs(displayInputs: DisplayInputs) {
    this.displayInputs = displayInputs;
    this.updateDisplay();
  }

  updateFilterText(filterText: string) {
    this.filterText = filterText;
    this.updateFilterHighlights();
  }

  setResponsesHistory(responsesHistory: OmniboxResponse[][]) {
    this.responsesHistory = responsesHistory;
    this.dispatchEvent(new CustomEvent(
        'responses-count-changed', {detail: responsesHistory.length}));
    this.updateSelectedResponseIndex(this.selectedResponseIndex);
  }

  updateSelectedResponseIndex(selection: number) {
    const response = this.responsesHistory[selection];
    if (!response) {
      // `selection` may be out of bounds when e.g. the user clears the
      // corresponding input field.
      return;
    }
    this.selectedResponseIndex = selection;
    this.clearResultsGroups();
    response.forEach(this.createResultsGroup.bind(this));
  }

  prepareNewQuery() {
    this.responsesHistory.push([]);
    this.dispatchEvent(new CustomEvent(
        'responses-count-changed', {detail: this.responsesHistory.length}));
  }

  addAutocompleteResponse(response: OmniboxResponse) {
    const lastIndex = this.responsesHistory.length - 1;
    const responses = this.responsesHistory[lastIndex];
    assert(responses);
    responses.push(response);
    if (lastIndex === this.selectedResponseIndex) {
      this.createResultsGroup(response);
    }
  }

  /**
   * Clears result groups from the UI.
   */
  private clearResultsGroups() {
    this.resultsGroups = [];
    clearChildren(this.$<HTMLElement>('#contents')!);
  }

  /**
   * Creates and adds a result group to the UI.
   */
  private createResultsGroup(response: OmniboxResponse) {
    const resultsGroup = OutputResultsGroup.create(response);
    this.resultsGroups.push(resultsGroup);
    const contents = this.$<HTMLElement>('#contents');
    assert(contents);
    contents.appendChild(resultsGroup);

    this.updateDisplay();
    this.updateFilterHighlights();
  }

  updateAnswerImage(url: string, data: string) {
    this.outputMatches.forEach(match => match.updateAnswerImage(url, data));
  }

  private updateDisplay() {
    this.updateVisibility();
    this.updateEliding();
    this.updateRowHeights();
  }

  /**
   * Show or hide various output elements depending on display inputs.
   * 1) Show non-last result groups only if showIncompleteResults is true.
   * 2) Show the details section above each table if showDetails or
   * showIncompleteResults are true.
   * 3) Show individual results when showAllProviders is true.
   * 4) Show certain columns and headers only if they showDetails is true.
   */
  private updateVisibility() {
    // Show non-last result groups only if showIncompleteResults is true.
    this.resultsGroups.forEach((resultsGroup, i) => {
      resultsGroup.hidden = !this.displayInputs.showIncompleteResults &&
          i !== this.resultsGroups.length - 1;
    });

    this.resultsGroups.forEach(resultsGroup => {
      resultsGroup.updateVisibility(
          this.displayInputs.showDetails, this.displayInputs.showAllProviders);
    });
  }

  private updateEliding() {
    this.resultsGroups.forEach(
        resultsGroup =>
            resultsGroup.updateEliding(this.displayInputs.elideCells));
  }

  private updateRowHeights() {
    this.resultsGroups.forEach(
        resultsGroup =>
            resultsGroup.updateRowHeights(this.displayInputs.thinRows));
  }

  private updateFilterHighlights() {
    this.outputMatches.forEach(match => match.filter(this.filterText));
  }

  private get outputMatches(): OutputMatch[] {
    return this.resultsGroups.flatMap(
        resultsGroup => resultsGroup.outputMatches);
  }
}

/**
 * Helps track and render a results group. C++ Autocomplete typically returns
 * 3 result groups per query. It may return less if the next query is
 * submitted before all 3 have been returned. Each result group contains
 * top level information (e.g., how long the result took to generate), as well
 * as a single list of combined results and multiple lists of individual
 * results. Each of these lists is tracked and rendered by OutputResultsTable
 * below.
 */
class OutputResultsGroup extends OmniboxElement {
  private details: ResultsDetails;
  private headers: OutputHeader[];
  private combinedResults: OutputResultsTable;
  private individualResultsList: OutputResultsTable[];
  private innerHeaders: HTMLElement[];

  static create(resultsGroup: OmniboxResponse): OutputResultsGroup {
    const outputResultsGroup = new OutputResultsGroup();
    outputResultsGroup.setResultsGroup(resultsGroup);
    return outputResultsGroup;
  }

  constructor() {
    super('output-results-group-template');
  }

  setResultsGroup(resultsGroup: OmniboxResponse) {
    this.details = {
      cursorPosition: resultsGroup.cursorPosition,
      time: resultsGroup.timeSinceOmniboxStartedMs,
      done: resultsGroup.done,
      type: resultsGroup.type,
      host: resultsGroup.host,
      isTypedHost: resultsGroup.isTypedHost,
    };
    this.headers = COLUMNS.map(column => new OutputHeader(column));
    this.combinedResults = new OutputResultsTable(resultsGroup.combinedResults);
    this.individualResultsList =
        resultsGroup.resultsByProvider
            .map(resultsWrapper => resultsWrapper.results)
            .filter(results => results.length > 0)
            .map(result => new OutputResultsTable(result));

    clearChildren(this);

    this.innerHeaders = [];

    const outputResultsDetails =
        this.$<OutputResultsDetails>('output-results-details');
    assert(outputResultsDetails);
    customElements.whenDefined(outputResultsDetails.localName)
        .then(() => outputResultsDetails.setDetails(this.details));

    const head = document.createElement('thead');
    head.classList.add('head');
    const row = document.createElement('tr');
    this.headers.forEach(cell => row.appendChild(cell));
    head.appendChild(row);
    const table = this.$('#table');
    assert(table);
    table.appendChild(head);

    table.appendChild(this.combinedResults);
    this.individualResultsList.forEach(results => {
      const innerHeader = this.renderInnerHeader(results);
      this.innerHeaders.push(innerHeader);
      table.appendChild(innerHeader);
      table.appendChild(results);
    });
  }

  private renderInnerHeader(results: OutputResultsTable): HTMLElement {
    const head = document.createElement('tbody');
    head.classList.add('head');
    const row = document.createElement('tr');
    const cell = document.createElement('th');
    cell.colSpan = COLUMNS.length;
    cell.textContent = results.innerHeaderText;
    row.appendChild(cell);
    head.appendChild(row);
    return head;
  }

  updateVisibility(showDetails: boolean, showAllProviders: boolean) {
    // TODO(manukh): Replace with `this.classList.toggle('show-details',
    //  showDetails);` (and likewise for `showAllProviders`) and use CSS to
    //  apply to the children. Show individual results when `showAllProviders`
    //  is true.
    this.individualResultsList.forEach(
        individualResults => individualResults.hidden = !showAllProviders);
    this.innerHeaders.forEach(
        innerHeader => innerHeader.hidden = !showAllProviders);

    // Show certain column headers only if they `showDetails` is true.
    COLUMNS.forEach(({displayAlways}, i) => {
      const header = this.headers[i];
      assert(header);
      header.hidden = !showDetails && !displayAlways;
    });

    // Show certain columns only if `showDetails` is true.
    this.outputMatches.forEach(match => match.updateVisibility(showDetails));
  }

  updateEliding(elideCells: boolean) {
    // TODO(manukh): Replace with `this.classList.toggle('elide-cells',
    //  elideCells);` and use CSS to apply to the children.
    this.outputMatches.forEach(match => match.updateEliding(elideCells));
  }

  updateRowHeights(thinRows: boolean) {
    // TODO(manukh): Replace with `this.classList.toggle('thin', thinRows);` and
    //  use CSS to apply to the children.
    this.outputMatches.forEach(
        match => match.classList.toggle('thin', thinRows));
  }

  get outputMatches(): OutputMatch[] {
    return [this.combinedResults]
        .concat(this.individualResultsList)
        .flatMap(results => results.outputMatches);
  }
}

class OutputResultsDetails extends OmniboxElement {
  constructor() {
    super('output-results-details-template');
  }

  setDetails(details: ResultsDetails) {
    const cursorPosition = this.$('#cursor-position');
    assert(cursorPosition);
    cursorPosition.textContent = String(details.cursorPosition);
    const time = this.$('#time');
    assert(time);
    time.textContent = String(details.time);
    const done = this.$('#done');
    assert(done);
    done.textContent = String(details.done);
    const type = this.$('#type');
    assert(type);
    type.textContent = details.type;
    const host = this.$('#host');
    assert(host);
    host.textContent = details.host;
    const isTypedHost = this.$('#is-typed-host');
    assert(isTypedHost);
    isTypedHost.textContent = String(details.isTypedHost);
  }
}

/**
 * Helps track and render a list of results. Each result is tracked and
 * rendered by OutputMatch below.
 */
class OutputResultsTable extends HTMLTableSectionElement {
  private autocompleteMatches: AutocompleteMatch[];
  readonly outputMatches: OutputMatch[];

  constructor(matches: AutocompleteMatch[]) {
    super();
    this.autocompleteMatches = matches;
    this.classList.add('body');
    this.outputMatches = matches.map(match => new OutputMatch(match));
    this.outputMatches.forEach(this.appendChild.bind(this));
  }

  get innerHeaderText(): string {
    return this.autocompleteMatches[0]?.providerName || '';
  }
}

/** Helps track and render a single match. */
class OutputMatch extends HTMLTableRowElement {
  private contentsAndDescription: OutputAnswerProperty;

  constructor(match: AutocompleteMatch) {
    super();
    let mouseMoved = false;
    this.addEventListener('mousedown', () => mouseMoved = false);
    this.addEventListener('mousemove', () => mouseMoved = true);
    this.addEventListener(
        'click', () => !mouseMoved && this.classList.toggle('expanded'));

    COLUMNS.forEach(column => {
      const property = column.create(match);
      property.classList.add(column.cellClassName);
      this.appendChild(property);
      if (property instanceof OutputAnswerProperty) {
        this.contentsAndDescription = property;
      }
    });
  }

  updateAnswerImage(url: string, data: string) {
    if (this.contentsAndDescription.image === url) {
      this.contentsAndDescription.setAnswerImageData(data);
    }
  }

  updateVisibility(showDetails: boolean) {
    // Show certain columns only if they showDetails is true.
    COLUMNS.forEach(({displayAlways}, i) => {
      const outputProperty = this.outputProperties[i];
      assert(outputProperty);
      return outputProperty!.hidden = !showDetails && !displayAlways;
    });
  }

  updateEliding(elideCells: boolean) {
    this.outputProperties.forEach(
        property => property.classList.toggle('elided', elideCells));
  }

  filter(filterText: string) {
    this.classList.remove('filtered-highlighted');
    this.outputProperties.forEach(
        property => property.classList.remove('filtered-highlighted-nested'));

    if (!filterText) {
      return;
    }

    const matchedProperties = this.outputProperties.filter(
        property => FilterUtil.filterText(property.filterText, filterText));
    const isMatch = matchedProperties.length > 0;
    this.classList.toggle('filtered-highlighted', isMatch);
    matchedProperties.forEach(
        property => property.classList.add('filtered-highlighted-nested'));
  }

  private get outputProperties(): OutputProperty[] {
    return [...this.children] as OutputProperty[];
  }
}

class OutputHeader extends HTMLTableCellElement {
  constructor(column: Column) {
    super();
    this.classList.add(column.headerClassName);

    let container: HTMLAnchorElement|HTMLDivElement;
    if (column.url) {
      container = document.createElement('a');
      container.href = column.url;
    } else {
      container = document.createElement('div');
    }
    container.classList.add('header-container');
    column.headerText.forEach(text => {
      const part = document.createElement('span');
      part.textContent = text;
      container.appendChild(part);
    });
    this.appendChild(container);

    this.title = column.tooltip;
  }
}

abstract class OutputProperty extends HTMLTableCellElement {
  readonly filterText: string;

  constructor(filterText: string) {
    super();
    this.filterText = filterText;
  }
}

abstract class FlexWrappingOutputProperty extends OutputProperty {
  protected readonly container: HTMLElement;

  protected constructor(filterText: string) {
    super(filterText);
    // margin-right is used on .pair-item's to separate them. To compensate,
    // .pair-container has negative margin-right. This means .pair-container's
    // overflow their parent. Overflowing a table cell is problematic, as 1)
    // scroll bars overlay adjacent cell, and 2) the page receives a
    // horizontal scroll bar when the right most column overflows. To avoid
    // this, the parent of any element with negative margins (e.g.
    // .pair-container) must not be a table cell; hence, the use of
    // scrollContainer.
    // Flex gutters may provide a cleaner alternative once implemented.
    // https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_Flexible_Box_Layout/Mastering_Wrapping_of_Flex_Items#Creating_gutters_between_items
    const scrollContainer = document.createElement('div');
    this.appendChild(scrollContainer);

    this.container = document.createElement('div');
    this.container.classList.add('pair-container');
    scrollContainer.appendChild(this.container);

    return this;
  }
}

class OutputPairProperty extends FlexWrappingOutputProperty {
  constructor(value1: string, value2: string) {
    super(`${value1}.${value2}`);

    const first = document.createElement('div');
    first.classList.add('pair-item');
    first.textContent = value1;
    this.container.appendChild(first);

    const second = document.createElement('div');
    second.classList.add('pair-item');
    second.textContent = value2;
    this.container.appendChild(second);

    return this;
  }
}

class OutputOverlappingPairProperty extends OutputPairProperty {
  constructor(value1: string, value2: string) {
    const overlap = value1.endsWith(value2);
    super(value2 && overlap ? value1.slice(0, -value2.length) : value1, value2);

    const notOverlapWarning = document.createElement('div');
    notOverlapWarning.classList.add('overlap-warning');
    notOverlapWarning.textContent = overlap ?
        '' :
        `btw, these texts do not overlap; '${
            value2}' was expected to be a suffix of '${value1}'`;
    this.container.appendChild(notOverlapWarning);

    return this;
  }
}

class OutputAnswerProperty extends FlexWrappingOutputProperty {
  readonly image: string;
  private readonly imageElement: HTMLImageElement;

  constructor(
      image: string, contents: string, description: string, answer: string,
      contentsClassification: ACMatchClassification[],
      descriptionClassification: ACMatchClassification[]) {
    super([image, contents, description, answer].join('.'));

    this.image = image;

    this.imageElement = document.createElement('img');
    this.imageElement.classList.add('pair-item');
    this.container.appendChild(this.imageElement);

    const contentsDiv = document.createElement('div');
    contentsDiv.classList.add('pair-item', 'contents');
    OutputAnswerProperty.renderClassifiedText(
        contentsDiv, contents, contentsClassification);
    this.container.appendChild(contentsDiv);

    const descriptionDiv = document.createElement('div');
    descriptionDiv.classList.add('pair-item', 'description');
    OutputAnswerProperty.renderClassifiedText(
        descriptionDiv, description, descriptionClassification);
    this.container.appendChild(descriptionDiv);

    const answerDiv = document.createElement('div');
    answerDiv.classList.add('pair-item', 'answer');
    answerDiv.textContent = answer;
    this.container.appendChild(answerDiv);

    const imageUrl = document.createElement('a');
    imageUrl.classList.add('pair-item', 'image-url');
    imageUrl.textContent = image;
    imageUrl.href = image;
    this.container.appendChild(imageUrl);
  }

  setAnswerImageData(imageData: string) {
    this.imageElement.src = imageData;
  }

  private static renderClassifiedText(
      container: HTMLElement, string: string,
      classes: ACMatchClassification[]) {
    clearChildren(container);
    OutputAnswerProperty.classify(string, classes)
        .map(
            ({string, style}) => OutputJsonProperty.renderJsonWord(
                string, OutputAnswerProperty.styleToClasses(style)))
        .forEach(span => container.appendChild(span));
  }

  private static classify(string: string, classes: ACMatchClassification[]):
      Array<{string: string, style: number}> {
    return classes.map(({offset, style}, i) => {
      const next = classes[i + 1];
      const end = next ? next.offset : string.length;
      return {string: string.substring(offset, end), style};
    });
  }

  private static styleToClasses(style: number): string[] {
    // Maps the bitmask enum AutocompleteMatch::ACMatchClassification::Style
    // to strings. See autocomplete_match.h for more details.
    // E.g., maps the style 5 to classes ['style-url', 'style-dim'].
    return ['style-url', 'style-match', 'style-dim'].filter(
        (_, i) => (style >> i) % 2);
  }
}

class OutputBooleanProperty extends OutputProperty {
  constructor(value: boolean, filterName: string) {
    super((value ? 'is: ' : 'not: ') + filterName);

    const icon = document.createElement('div');
    icon.classList.add(value ? 'check-mark' : 'x-mark');
    this.appendChild(icon);

    return this;
  }
}

class OutputJsonProperty extends OutputProperty {
  private static classifications: Array<{re: RegExp, clazz: string}>;
  private static spaceRegex: RegExp;

  constructor(json: string) {
    super(JSON.stringify(json, null, 2));

    const pre = document.createElement('pre');
    pre.classList.add('json');
    json.split(/("(?:[^"\\]|\\.)*":?|\w+)/)
        .map(word => {
          return OutputJsonProperty.renderJsonWord(
              word, [OutputJsonProperty.classifyJsonWord(word)]);
        })
        .forEach(jsonSpan => pre.appendChild(jsonSpan));
    this.appendChild(pre);

    return this;
  }

  static renderJsonWord(word: string, classes: string[]): HTMLElement {
    const span = document.createElement('span');
    span.classList.add(...classes);
    span.textContent = word;
    return span;
  }

  static classifyJsonWord(word: string): string {
    // Statically creating the regexes only once.
    OutputJsonProperty.classifications = OutputJsonProperty.classifications || [
      {re: /^"[^]*":$/, clazz: 'key'},
      {re: /^"[^]*"$/, clazz: 'string'},
      {re: /true|false/, clazz: 'boolean'},
      {re: /null/, clazz: 'null'},
    ];
    OutputJsonProperty.spaceRegex = OutputJsonProperty.spaceRegex || /^\s*$/;

    if (Number.isNaN(Number(word))) {
      const classification =
          OutputJsonProperty.classifications.find(({re}) => re.test(word));
      return classification && classification.clazz || '';
    } else if (!OutputJsonProperty.spaceRegex.test(word)) {
      return 'number';
    }
    return '';
  }
}

class OutputAdditionalInfoProperty extends OutputProperty {
  constructor(value: AutocompleteAdditionalInfo[]) {
    super(value.map(({key, value}) => `${key}: ${value}`).join('\n'));

    const container = document.createElement('div');

    const pre = document.createElement('pre');
    pre.classList.add('json');
    value.forEach(({key, value}) => {
      pre.appendChild(OutputJsonProperty.renderJsonWord(key + ': ', ['key']));
      pre.appendChild(
          OutputJsonProperty.renderJsonWord(value + '\n', ['number']));
    });
    container.appendChild(pre);

    const link = document.createElement('a');
    link.download = 'AdditionalInfo.json';
    link.href = OutputAdditionalInfoProperty.createDownloadLink(value);
    container.appendChild(link);

    this.appendChild(container);

    return this;
  }

  private static createDownloadLink(value: AutocompleteAdditionalInfo[]):
      string {
    const obj = value.reduce((obj: Record<string, string>, {key, value}) => {
      obj[key] = value;
      return obj;
    }, {});
    const text = JSON.stringify(obj, null, 2);
    const obj64 = btoa(unescape(encodeURIComponent(text)));
    return `data:application/json;base64,${obj64}`;
  }
}

class OutputUrlProperty extends FlexWrappingOutputProperty {
  constructor(
      destinationUrl: string, isSearchType: boolean,
      strippedDestinationUrl: string) {
    super(destinationUrl);

    const iconAndUrlContainer = document.createElement('div');
    iconAndUrlContainer.classList.add('pair-item');
    this.container.appendChild(iconAndUrlContainer);

    if (!isSearchType) {
      const icon = document.createElement('img');
      icon.src = `chrome://favicon/${destinationUrl}`;
      iconAndUrlContainer.appendChild(icon);
    }

    const urlLink = document.createElement('a');
    urlLink.textContent = destinationUrl;
    urlLink.href = destinationUrl;
    iconAndUrlContainer.appendChild(urlLink);

    const strippedUrlLink = document.createElement('a');
    strippedUrlLink.classList.add('pair-item');
    strippedUrlLink.textContent = strippedDestinationUrl;
    strippedUrlLink.href = strippedDestinationUrl;
    this.container.appendChild(strippedUrlLink);

    return this;
  }
}

class OutputTextProperty extends OutputProperty {
  constructor(text: string) {
    super(text);

    const div = document.createElement('div');
    div.textContent = text;
    this.appendChild(div);

    return this;
  }
}

/** Responsible for highlighting and hiding rows using filter text. */
class FilterUtil {
  /**
   * Checks if a string fuzzy-matches a filter string. Each character
   * of filterText must be present in the search text, either adjacent to the
   * previous matched character, or at the start of a new word (see
   * textToWords).
   * E.g. `abc` matches `abc`, `a big cat`, `a-bigCat`, `a very big cat`, and
   * `an amBer cat`; but does not match `abigcat` or `an amber cat`.
   * `green rainbow` is matched by `gre rain`, but not by `gre bow`.
   * One exception is the first character, which may be matched mid-word.
   * E.g. `een rain` can also match `green rainbow`.
   */
  static filterText(searchText: string, filterText: string): boolean {
    const regexFilter =
        Array.from(filterText)
            .map(word => word.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'))
            .join('(.*\\.)?');
    const words = FilterUtil.textToWords(searchText).join('.');
    return words.match(new RegExp(regexFilter, 'i')) !== null;
  }

  /**
   * Splits a string into words, delimited by either capital letters, groups
   * of digits, or non alpha characters.
   * E.g., `https://google.com/the-dog-ate-134pies` will be split to:
   * https, :, /, /, google, ., com, /, the, -,  dog, -, ate, -, 134, pies
   * This differs from `Array.split` in that this groups digits, e.g. 134.
   */
  private static textToWords(text: string): string[] {
    const MAX_TEXT_LENGTH = 200;
    if (text.length > MAX_TEXT_LENGTH) {
      text = text.slice(0, MAX_TEXT_LENGTH);
      console.warn(`text to be filtered too long, truncated; max length: ${
          MAX_TEXT_LENGTH}, truncated text: ${text}`);
    }
    return text.match(/[a-z]+|[A-Z][a-z]*|\d+|./g) || [];
  }
}

class Column {
  headerText: string[];
  url: string;
  hyphenatedName: string;
  displayAlways: boolean;
  tooltip: string;
  create: (match: AutocompleteMatch) => OutputProperty;

  constructor(
      headerText: string[], url: string, hyphenatedName: string,
      displayAlways: boolean, tooltip: string,
      create: (match: AutocompleteMatch) => OutputProperty) {
    this.headerText = headerText;
    this.url = url;
    this.hyphenatedName = hyphenatedName;
    this.displayAlways = displayAlways;
    this.tooltip = tooltip;
    this.create = create;
  }

  get cellClassName(): string {
    return 'cell-' + this.hyphenatedName;
  }

  get headerClassName(): string {
    return 'header-' + this.hyphenatedName;
  }
}

/**
 * A constant that's used to decide what autocomplete result
 * properties to output in what order.
 */
const COLUMNS: Column[] = [
  new Column(
      ['Provider', 'Type'], '', 'provider-and-type', true,
      'Provider & Type\nThe AutocompleteProvider suggesting this result. / ' +
          'The type of the result.',
      match => new OutputPairProperty(match.providerName, match.type)),
  new Column(
      ['Relevance'], '', 'relevance', true,
      'Relevance\nThe result score. Higher is more relevant.',
      match => new OutputTextProperty(String(match.relevance))),
  new Column(
      ['Contents', 'Description', 'Answer'], '', 'contents-and-description',
      true,
      'Contents & Description & Answer\nURL classifications are styled ' +
          'blue.\nMATCH classifications are styled bold.\nDIM ' +
          'classifications are styled with a gray background.',
      match => new OutputAnswerProperty(
          match.image, match.contents, match.description, match.answer,
          match.contentsClass, match.descriptionClass)),
  new Column(
      ['sw'], '', 'swap-contents-and-description', false,
      'Swap Contents and Description',
      match => new OutputBooleanProperty(
          match.swapContentsAndDescription, 'Swap Contents and Description')),
  new Column(
      ['df'], '', 'allowed-to-be-default-match', true,
      'Can be Default\nA green checkmark indicates that the result can be ' +
          'the default match (i.e., can be the match that pressing enter ' +
          'in the omnibox navigates to).',
      match => new OutputBooleanProperty(
          match.allowedToBeDefaultMatch, 'Can be Default')),
  new Column(
      ['bk'], '', 'starred', false,
      'Bookmarked\nA green checkmark indicates that the result has been ' +
          'bookmarked.',
      match => new OutputBooleanProperty(match.starred, 'Bookmarked')),
  new Column(
      ['tb'], '', 'has-tab-match', false,
      'Has Tab Match\nA green checkmark indicates that the result URL ' +
          'matches an open tab.',
      match => new OutputBooleanProperty(match.hasTabMatch, 'Has Tab Match')),
  new Column(
      ['URL', 'Stripped URL'], '', 'destination-url', true,
      'URL & Stripped URL\nThe URL for the result. / The stripped URL for ' +
          'the result.',
      match => new OutputUrlProperty(
          match.destinationUrl, match.isSearchType,
          match.strippedDestinationUrl)),
  new Column(
      ['AQS Type & Subtypes'], '', 'aqs-type-subtypes', false,
      'The type and subtypes reported in the Assisted Query Stats (AQS) url ' +
          'query param.',
      match => new OutputTextProperty(match.aqsTypeSubtypes)),
  new Column(
      ['Fill', 'Inline'], '', 'fill-and-inline', false,
      'Fill & Inline\nThe text shown in the omnibox when the result is ' +
          'selected. / The text shown in the omnibox as a blue highlight ' +
          'selection following the cursor, if this match is shown inline.',
      match => new OutputOverlappingPairProperty(
          match.fillIntoEdit, match.inlineAutocompletion)),
  new Column(
      ['dl'], '', 'deletable', false,
      'Deletable\nA green checkmark indicates that the result can be ' +
          'deleted from the visit history.',
      match => new OutputBooleanProperty(match.deletable, 'Deletable')),
  new Column(
      ['pr'], '', 'from-previous', false,
      'From Previous\nTrue if this match is from a previous result.',
      match => new OutputBooleanProperty(match.fromPrevious, 'From Previous')),
  new Column(
      ['Tran'],
      'https://cs.chromium.org/chromium/src/ui/base/page_transition_types.h' +
          '?q=page_transition_types.h&sq=package:chromium&dr=CSs&l=14',
      'transition', false, 'Transition\nHow the user got to the result.',
      match => new OutputTextProperty(match.transition)),
  new Column(
      ['dn'], '', 'provider-done', false,
      'Done\nA green checkmark indicates that the provider is done looking ' +
          'for more results.',
      match => new OutputBooleanProperty(match.providerDone, 'Done')),
  new Column(
      ['Associated Keyword'], '', 'associated-keyword', false,
      'Associated Keyword\nIf non-empty, a "press tab to search" hint will ' +
          'be shown and will engage this keyword.',
      match => new OutputTextProperty(match.associatedKeyword)),
  new Column(
      ['Keyword'], '', 'keyword', false,
      'Keyword\nThe keyword of the search engine to be used.',
      match => new OutputTextProperty(match.keyword)),
  new Column(
      ['dp'], '', 'duplicates', false,
      'Duplicates\nThe number of matches that have been marked as ' +
          'duplicates of this match.',
      match => new OutputTextProperty(String(match.duplicates))),
  new Column(
      ['pi'],
      'https://source.chromium.org/chromium/chromium/src/+/main:components/omnibox/browser/omnibox_pedal_concepts.h;l=19;drc=c741e070dbfcc33b2369e7a5131be87c7b21bb99',
      'pedal-id', false, 'Pedal ID\nThe ID of attached Pedal, or zero if none.',
      match => new OutputTextProperty(String(match.pedalId))),
  new Column(
      ['Additional Info'], '', 'additional-info', true,
      'Additional Info\nProvider-specific information about the result.',
      match => new OutputAdditionalInfoProperty(match.additionalInfo)),
];

customElements.define('omnibox-output', OmniboxOutput);
customElements.define('output-results-group', OutputResultsGroup);
customElements.define('output-results-details', OutputResultsDetails);
customElements.define(
    'output-results-table', OutputResultsTable, {extends: 'tbody'});
customElements.define('output-match', OutputMatch, {extends: 'tr'});
customElements.define('output-header', OutputHeader, {extends: 'th'});
customElements.define(
    'output-pair-property', OutputPairProperty, {extends: 'td'});
customElements.define(
    'output-overlapping-pair-property', OutputOverlappingPairProperty,
    {extends: 'td'});
customElements.define(
    'output-answer-property', OutputAnswerProperty, {extends: 'td'});
customElements.define(
    'output-boolean-property', OutputBooleanProperty, {extends: 'td'});
customElements.define(
    'output-json-property', OutputJsonProperty, {extends: 'td'});
customElements.define(
    'output-additional-info-property', OutputAdditionalInfoProperty,
    {extends: 'td'});
customElements.define(
    'output-url-property', OutputUrlProperty, {extends: 'td'});
customElements.define(
    'output-text-property', OutputTextProperty, {extends: 'td'});

// TODO(manukh): Partition into smaller files.
