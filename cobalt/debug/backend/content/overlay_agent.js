// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

(function(debugBackend) {

// Attach methods to handle commands in the 'Overlay' devtools domain.
// https://chromedevtools.github.io/devtools-protocol/tot/Overlay
let commands = debugBackend.Overlay = {};

// Returns non-overlapping rectangles to highlight for the box model of a node.
// This pseudo-command in the Overlay domain is a helper for the C++
// |OverlayAgent::HighlightNode|.
commands._highlightNodeRects = function(params) {
  let node = debugBackend.DOM._findNode(params);
  let config = params.highlightConfig || {};
  let highlights = [];
  if (node && node.getBoundingClientRect) {
    let styles = window.getComputedStyle(node);
    let content = node.getBoundingClientRect();
    let color;
    let box;

    let transformed = isNodeTransformed(node);

    if (!transformed) {
      // Margin
      color = config.marginColor;
      box = styleBox(styles, 'margin');
      if (color) {
        boxRects(content, box).forEach(
            rect => highlights.push(highlightParams(rect, color)));
      }

      // Border
      color = config.borderColor;
      box = styleBox(styles, 'border');
      if (color) {
        boxRects(content, box).forEach(
            rect => highlights.push(highlightParams(rect, color)));
      }
      content = insetRect(content, box);

      // Padding
      color = config.paddingColor;
      box = styleBox(styles, 'padding');
      if (color) {
        boxRects(content, box).forEach(
            rect => highlights.push(highlightParams(rect, color)));
      }
      content = insetRect(content, box);
    }

    // Content
    color = config.contentColor;
    if (color) {
      let highlight = highlightParams(content, color);
      if (transformed) {
        highlight.outlineColor = {r: 255, g: 0, b: 255, a: 1.0};
      }
      highlights.push(highlight);
    }
  }
  return JSON.stringify({highlightRects: highlights});
}

// Returns the inset width of the 4 sides of a box in the computed style, with
// margin being negative to place it outside the rect.
function styleBox(styles, boxName) {
  let suffix = boxName == 'border' ? '-width' : '';
  let sign = boxName == 'margin' ? -1 : 1;
  let box = {};
  ['top', 'right', 'bottom', 'left'].forEach(side => {
    let width = styles.getPropertyValue(`${boxName}-${side}${suffix}`);
    box[side] = sign * parseFloat(width) || 0;
  });
  return box;
}

function isNodeTransformed(node) {
  while (node) {
    if (window.getComputedStyle(node).transform !== 'none') return true;
    node = node.offsetParent;
  }
  return false;
}

// Returns an array of non-overlapping rectangles for the box around the inside
// of |rect|, but the rectangle for any box side with negative width will be
// on the outside of |rect|.
function boxRects(rect, box) {
  // Start out assuming box widths are all positive.
  let outerT = rect.y;
  let outerB = rect.y + rect.height;
  let outerL = rect.x;
  let outerR = rect.x + rect.width;
  let innerT = outerT + box.top;
  let innerB = outerB - box.bottom;
  let innerL = outerL + box.left;
  let innerR =  outerR - box.right;

  // Swap any inner/outer "inverted" by a negative box side.
  if (outerT > innerT) [outerT, innerT] = [innerT, outerT];
  if (outerB < innerB) [outerB, innerB] = [innerB, outerB];
  if (outerL > innerL) [outerL, innerL] = [innerL, outerL];
  if (outerR < innerR) [outerR, innerR] = [innerR, outerR];

  // +--------------+
  // |              |
  // +--+--------+--+
  // |  |        |  |
  // |  |        |  |
  // +--+--------+--+
  // |              |
  // +--------------+
  return [
    // top
    { x: outerL,
      y: outerT,
      width: outerR - outerL,
      height: innerT - outerT },
    // bottom
    { x: outerL,
      y: innerB,
      width: outerR - outerL,
      height: outerB - innerB },
    // left
    { x: outerL,
      y: innerT,
      width: innerL - outerL,
      height: innerB - innerT },
    // right
    { x: innerR,
      y: innerT,
      width: outerR - innerR,
      height: innerB - innerT },
  ];
}

// Returns the rectangle with the box around the insides removed.
function insetRect(rect, box) {
  return {
    x: rect.x + box.left,
    y: rect.y + box.top,
    width: rect.width - box.left - box.right,
    height: rect.height - box.top - box.bottom };
}

// Returns parameters matching the DevTools protocol "Overlay.highlightRect"
// parameters, as expected by the native overlay agent.
function highlightParams(rect, color) {
  // Copy each property rather than whole objects to ensure this can be
  // converted with JSON.serialize().
  return {
    x: rect.x,
    y: rect.y,
    width: rect.width,
    height: rect.height,
    color: {
      r: color.r,
      g: color.g,
      b: color.b,
      a: color.a,
    },
  };
}

// TODO: Pass debugBackend from C++ instead of getting it from the window.
})(window.debugBackend);
