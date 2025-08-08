// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
 * Returns is the tag of an `element` is tag.
 *
 * It is based on the logic in
 *     bool HasTagName(const WebNode& node, const blink::WebString& tag)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param node Node to examine.
 * @param tag Tag name.
 * @return Whether the tag of node is tag.
 */
gCrWeb.fill.hasTagName = function(node: Element, tag: string): boolean {
  return node.nodeType === Node.ELEMENT_NODE &&
      (node).tagName === tag.toUpperCase();
};

/**
 * Checks if an element is autofillable.
 *
 * It is based on the logic in
 *     bool IsAutofillableElement(const WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param element An element to examine.
 * @return Whether element is one of the element types that can be
 *     autofilled.
 */
gCrWeb.fill.isAutofillableElement = function(element: Element): boolean {
  return gCrWeb.fill.isAutofillableInputElement(element) ||
      gCrWeb.fill.isSelectElement(element) || isTextAreaElement(element);
};

/**
 * Trims whitespace from the start of the input string.
 * Simplified version of string_util::TrimWhitespace.
 * @param input String to trim.
 * @return The `input` string without leading whitespace.
 */
function trimWhitespaceLeading(input: string): string {
  return input.replace(/^\s+/gm, '');
}

/**
 * Trims whitespace from the end of the input string.
 * Simplified version of string_util::TrimWhitespace.
 * @param input String to trim.
 * @return The `input` string without trailing whitespace.
 */
function trimWhitespaceTrailing(input: string): string {
  return input.replace(/\s+$/gm, '');
}

/**
 * Appends `suffix` to `prefix` so that any intermediary whitespace is collapsed
 * to a single space.  If `force_whitespace` is true, then the resulting string
 * is guaranteed to have a space between `prefix` and `suffix`.  Otherwise, the
 * result includes a space only if `prefix` has trailing whitespace or `suffix`
 * has leading whitespace.
 *
 * A few examples:
 *     CombineAndCollapseWhitespace('foo', 'bar', false)       -> 'foobar'
 *     CombineAndCollapseWhitespace('foo', 'bar', true)        -> 'foo bar'
 *     CombineAndCollapseWhitespace('foo ', 'bar', false)      -> 'foo bar'
 *     CombineAndCollapseWhitespace('foo', ' bar', false)      -> 'foo bar'
 *     CombineAndCollapseWhitespace('foo', ' bar', true)       -> 'foo bar'
 *     CombineAndCollapseWhitespace('foo   ', '   bar', false) -> 'foo bar'
 *     CombineAndCollapseWhitespace(' foo', 'bar ', false)     -> ' foobar '
 *     CombineAndCollapseWhitespace(' foo', 'bar ', true)      -> ' foo bar '
 *
 * It is based on the logic in
 * const string16 CombineAndCollapseWhitespace(const string16& prefix,
 *                                             const string16& suffix,
 *                                             bool force_whitespace)
 * @param prefix The prefix string in the string combination.
 * @param suffix The suffix string in the string combination.
 * @param forceWhitespace A boolean indicating if whitespace should
 *     be added as separator in the combination.
 * @return The combined string.
 */
gCrWeb.fill.combineAndCollapseWhitespace = function(
    prefix: string, suffix: string, forceWhitespace: boolean): string {
  const prefixTrimmed = trimWhitespaceTrailing(prefix);
  const prefixTrailingWhitespace = prefixTrimmed !== prefix;
  const suffixTrimmed = trimWhitespaceLeading(suffix);
  const suffixLeadingWhitespace = suffixTrimmed !== suffix;
  if (prefixTrailingWhitespace || suffixLeadingWhitespace || forceWhitespace) {
    return prefixTrimmed + ' ' + suffixTrimmed;
  } else {
    return prefixTrimmed + suffixTrimmed;
  }
};

/**
 * This is a helper function for the findChildText() function (see below).
 * Search depth is limited with the `depth` parameter.
 *
 * Based on form_autofill_util::FindChildTextInner().
 *
 * @param {Node} node The node to fetch the text content from.
 * @param depth The maximum depth to descend on the DOM.
 * @param divsToSkip List of <div> tags to ignore if encountered.
 * @return The discovered and adapted string.
 */
// TODO(crbug.com/1492539): Replace all `any` types with a specific type.
function findChildTextInner(
    node: any, depth: number, divsToSkip: Node[]):string {
  if (depth <= 0 || !node) {
    return '';
  }

  // Skip over comments.
  if (node.nodeType === Node.COMMENT_NODE) {
    return findChildTextInner(node.nextSibling, depth - 1, divsToSkip);
  }

  if (node.nodeType !== Node.ELEMENT_NODE && node.nodeType !== Node.TEXT_NODE) {
    return '';
  }

  // Ignore elements known not to contain inferable labels.
  let skipNode = false;
  if (node.nodeType === Node.ELEMENT_NODE) {
    if (node.tagName === 'OPTION') {
      return '';
    }
    if (gCrWeb.form.isFormControlElement(/** @type {Element} */ (node))) {
      const input = /** @type {FormControlElement} */ (node);
      if (gCrWeb.fill.isAutofillableElement(input)) {
        return '';
      }
    }
    skipNode = node.tagName === 'SCRIPT' || node.tagName === 'NOSCRIPT';
  }

  if (node.tagName === 'DIV') {
    for (let i = 0; i < divsToSkip.length; ++i) {
      if (node === divsToSkip[i]) {
        return '';
      }
    }
  }

  // Extract the text exactly at this node.
  let nodeText = '';
  if (!skipNode) {
    nodeText = gCrWeb.fill.nodeValue(node);
    if (node.nodeType === Node.TEXT_NODE && !nodeText) {
      // In the C++ version, this text node would have been stripped completely.
      // Just pass the buck.
      return findChildTextInner(node.nextSibling, depth, divsToSkip);
    }

    // Recursively compute the children's text.
    // Preserve inter-element whitespace separation.
    const childText =
        findChildTextInner(node.firstChild, depth - 1, divsToSkip);
    let addSpace = node.nodeType === Node.TEXT_NODE && !nodeText;
    // Emulate apparently incorrect Chromium behavior tracked in
    // https://crbug.com/239819.
    addSpace = false;
    nodeText = gCrWeb.fill.combineAndCollapseWhitespace(
        nodeText, childText, addSpace);
  }

  // Recursively compute the siblings' text.
  // Again, preserve inter-element whitespace separation.
  const siblingText =
      findChildTextInner(node.nextSibling, depth - 1, divsToSkip);
  let addSpace = node.nodeType === Node.TEXT_NODE && !nodeText;
  // Emulate apparently incorrect Chromium behavior tracked in
  // https://crbug.com/239819.
  addSpace = false;
  nodeText = gCrWeb.fill.combineAndCollapseWhitespace(
      nodeText, siblingText, addSpace);

  return nodeText;
}

/**
 * Same as findChildText() below, but with a list of div nodes to skip.
 *
 * It is based on the logic in
 *    string16 FindChildTextWithIgnoreList(
 *        const WebNode& node,
 *        const std::set<WebNode>& divs_to_skip)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param node A node of which the child text will be return.
 * @param divsToSkip List of <div> tags to ignore if encountered.
 * @return The child text.
 */
function findChildTextWithIgnoreList(node: Node, divsToSkip: Node[]): string {
  if (node.nodeType === Node.TEXT_NODE) {
    return gCrWeb.fill.nodeValue(node);
  }

  const child = node.firstChild;
  const kChildSearchDepth = 10;
  let nodeText = findChildTextInner(child, kChildSearchDepth, divsToSkip);
  nodeText = nodeText.trim();
  return nodeText;
}

/**
 * Returns the aggregated values of the descendants of `element` that are
 * non-empty text nodes.
 *
 * It is based on the logic in
 *    string16 FindChildText(const WebNode& node)
 * chromium/src/components/autofill/content/renderer/form_autofill_util.cc,
 * which is a faster alternative to `innerText()` for performance critical
 * operations.
 *
 * @param node A node of which the child text will be return.
 * @return The child text.
 */
function findChildText(node: Node): string {
  return findChildTextWithIgnoreList(node, []);
}

/**
 * Returns true if `node` is an element and it is a container type that
 * inferLabelForElement() can traverse.
 *
 * It is based on the logic in
 *     bool IsTraversableContainerElement(const WebNode& node);
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {!Node} node The node to be examined.
 * @return Whether it can be traversed.
 */
// TODO(crbug.com/1492539): Replace all `any` types with a specific type.
function isTraversableContainerElement(node: any): boolean {
  if (node.nodeType !== Node.ELEMENT_NODE) {
    return false;
  }

  const tagName = /** @type {Element} */ (node).tagName;
  return (
      tagName === 'DD' || tagName === 'DIV' || tagName === 'FIELDSET' ||
      tagName === 'LI' || tagName === 'TD' || tagName === 'TABLE');
}

/**
 * Returns the element type for all ancestor nodes in CAPS, starting with the
 * parent node.
 *
 * It is based on the logic in
 *    std::vector<std::string> AncestorTagNames(
 *        const WebFormControlElement& element);
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 *
 * @param {FormControlElement} element An element to examine.
 * @return The element types for all ancestors.
 */
// TODO(crbug.com/1492539): Replace all `any` types with a specific type.
function ancestorTagNames(element: any): string[] {
  const tagNames: string[] = [];
  let parentNode = element.parentNode;
  while (parentNode) {
    if (parentNode.nodeType === Node.ELEMENT_NODE) {
      tagNames.push(parentNode.tagName);
    }
    parentNode = parentNode.parentNode;
  }
  return tagNames;
}

/**
 * Returns true if `element` is a text input element.
 *
 * It is based on the logic in
 *     bool IsTextInput(const blink::WebInputElement* element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param element An element to examine.
 * @return Whether element is a text input field.
 */
gCrWeb.fill.isTextInput = function(element: Element): boolean {
  if (!element) {
    return false;
  }
  return gCrWeb.common.isTextField(element);
};

/**
 * Returns true if `element` is a 'select' element.
 *
 * It is based on the logic in
 *     bool IsSelectElement(const blink::WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {FormControlElement|HTMLOptionElement} element An element to examine.
 * @return Whether element is a 'select' element.
 */
// TODO(crbug.com/1492539): Replace all `any` types with a specific type.
gCrWeb.fill.isSelectElement = function(element: any): boolean {
  if (!element) {
    return false;
  }
  return element.type === 'select-one';
};

/**
 * Returns true if `element` is a 'textarea' element.
 *
 * It is based on the logic in
 *     bool IsTextAreaElement(const blink::WebFormControlElement& element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {FormControlElement} element An element to examine.
 * @return Whether element is a 'textarea' element.
 */
// TODO(crbug.com/1492539): Replace all `any` types with a specific type.
function isTextAreaElement(element: any): boolean {
  if (!element) {
    return false;
  }
  return element.type === 'textarea';
}

/**
 * Returns true if `element` is a checkbox or a radio button element.
 *
 * It is based on the logic in
 *     bool IsCheckableElement(const blink::WebInputElement* element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param {FormControlElement} element An element to examine.
 * @return Whether element is a checkbox or a radio button.
 */
// TODO(crbug.com/1492539): Replace all `any` types with a specific type.
gCrWeb.fill.isCheckableElement = function(element: any): boolean {
  if (!element) {
    return false;
  }
  return element.type === 'checkbox' || element.type === 'radio';
};

/**
 * Returns true if `element` is one of the input element types that can be
 * autofilled. {Text, Radiobutton, Checkbox}.
 *
 * It is based on the logic in
 *    bool IsAutofillableInputElement(const blink::WebInputElement* element)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.h.
 *
 * @param element An element to examine.
 * @return Whether element is one of the input element types that
 *     can be autofilled.
 */
gCrWeb.fill.isAutofillableInputElement = function(element: Element): boolean {
  return gCrWeb.fill.isTextInput(element) ||
      gCrWeb.fill.isCheckableElement(element);
};

/**
 * Helper for `InferLabelForElement()` that tests if an inferred label is valid
 * or not. A valid label is a label that does not only contains special
 * characters.
 *
 * It is based on the logic in
 *     bool isLabelValid(base::StringPiece16 inferred_label,
 *         const std::vector<char16_t>& stop_words)
 * in chromium/src/components/autofill/content/renderer/form_autofill_util.cc.
 * The list of characters that are considered special is hard-coded in a regexp.
 *
 * @param label An element to examine.
 * @return Whether the label contains not special characters.
 */
function isLabelValid(label: string): boolean {
  return label.search(/[^ *:()\u2013-]/) >= 0;
}

export {
  findChildTextWithIgnoreList,
  findChildText,
  isTraversableContainerElement,
  ancestorTagNames,
  isTextAreaElement,
  isLabelValid,
};
