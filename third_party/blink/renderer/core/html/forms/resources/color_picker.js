// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Color picker used by <input type='color' />
 *
 * This can be debugged with manual_tests/forms/color-suggestion-picker.html
 */

function initializeColorPicker() {
  if (global.params.selectedColor === undefined) {
    global.params.selectedColor = DefaultColor;
  }
  const colorPicker = new ColorPicker(new Color(global.params.selectedColor));
  main.append(colorPicker);
  const width = colorPicker.offsetWidth;
  const height = colorPicker.offsetHeight;
  resizeWindow(width, height);
}

/**
 * @param {!Object} args
 * @return {?string} An error message, or null if the argument has no errors.
 */
function validateColorPickerArguments(args) {
  if (args.shouldShowColorSuggestionPicker)
    return 'Should be showing the color suggestion picker.';
  if (!args.selectedColor)
    return 'No selectedColor.';
  return null;
}

/**
 * Supported movement directions.
 * @enum {number}
 */
const Direction = {
  UNDEFINED: 0,
  LEFT: 1,
  RIGHT: 2,
  UP: 3,
  DOWN: 4,
}

/**
 * Supported color channels.
 * @enum {number}
 */
const ColorChannel = {
  HEX: 0,
  R: 1,
  G: 2,
  B: 3,
  H: 4,
  S: 5,
  L: 6,
};

/**
 * Supported color formats.
 * @enum {number}
 */
const ColorFormat = {
  HEX: 0,
  RGB: 1,
  HSL: 2,
};

/**
 * Color: Helper class to get color values in different color formats.
 */
class Color {
  /**
   * @param {string|!ColorFormat} colorStringOrFormat
   * @param  {...number} colorValues ignored if colorStringOrFormat is a string
   */
  constructor(colorStringOrFormat, ...colorValues) {
    if (typeof colorStringOrFormat === 'string') {
      colorStringOrFormat = colorStringOrFormat.toLowerCase();
      if (colorStringOrFormat.startsWith('#')) {
        this.hexValue_ = colorStringOrFormat.substr(1);
      } else if (colorStringOrFormat.startsWith('rgb')) {
        // Ex. 'rgb(255, 255, 255)' => [255,255,255]
        colorStringOrFormat = colorStringOrFormat.replace(/\s+/g, '');
        [this.rValue_, this.gValue_, this.bValue_] =
            colorStringOrFormat.substring(4, colorStringOrFormat.length - 1)
                .split(',')
                .map(Number);
      } else if (colorStringOrFormat.startsWith('hsl')) {
        colorStringOrFormat = colorStringOrFormat.replace(/%|\s+/g, '');
        [this.hValue_, this.sValue_, this.lValue_] =
            colorStringOrFormat.substring(4, colorStringOrFormat.length - 1)
                .split(',')
                .map(Number);
      }
    } else {
      switch (colorStringOrFormat) {
        case ColorFormat.HEX:
          this.hexValue_ = colorValues[0].toLowerCase();
          break;
        case ColorFormat.RGB:
          [this.rValue_, this.gValue_, this.bValue_] = colorValues.map(Number);
          break;
        case ColorFormat.HSL:
          [this.hValue_, this.sValue_, this.lValue_] = colorValues.map(Number);
          break;
      }
    }
  }

  /**
   * @param {!Color} other
   */
  equals(other) {
    return (this.hexValue === other.hexValue);
  }

  /**
   * @returns {string}
   */
  get hexValue() {
    this.computeHexValue_();
    return this.hexValue_;
  }

  computeHexValue_() {
    if (this.hexValue_ !== undefined) {
      // Already computed.
    } else if (this.rValue_ !== undefined) {
      this.hexValue_ = Color.rgbToHex(this.rValue_, this.gValue_, this.bValue_);
    } else if (this.hValue_ !== undefined) {
      this.hexValue_ = Color.hslToHex(this.hValue_, this.sValue_, this.lValue_);
    }
  }

  asHex() {
    return '#' + this.hexValue;
  }

  /**
   * @returns {number} between 0 and 255
   */
  get rValue() {
    this.computeRGBValues_();
    return this.rValue_;
  }

  /**
   * @returns {number} between 0 and 255
   */
  get gValue() {
    this.computeRGBValues_();
    return this.gValue_;
  }

  /**
   * @returns {number} between 0 and 255
   */
  get bValue() {
    this.computeRGBValues_();
    return this.bValue_;
  }

  computeRGBValues_() {
    if (this.rValue_ !== undefined) {
      // Already computed.
    } else if (this.hexValue_ !== undefined) {
      [this.rValue_, this.gValue_, this.bValue_] =
          Color.hexToRGB(this.hexValue_);
    } else if (this.hValue_ !== undefined) {
      [this.rValue_, this.gValue_, this.bValue_] =
          Color.hslToRGB(this.hValue_, this.sValue_, this.lValue_);
    }
  }

  rgbValues() {
    return [this.rValue, this.gValue, this.bValue];
  }

  asRGB() {
    return 'rgb(' + this.rgbValues().join() + ')';
  }

  /**
   * @returns {number} between 0 and 359
   */
  get hValue() {
    this.computeHSLValues_();
    return this.hValue_;
  }

  /**
   * @returns {number} between 0 and 100
   */
  get sValue() {
    this.computeHSLValues_();
    return this.sValue_;
  }

  /**
   * @returns {number} between 0 and 100
   */
  get lValue() {
    this.computeHSLValues_();
    return this.lValue_;
  }

  computeHSLValues_() {
    if (this.hValue_ !== undefined) {
      // Already computed.
    } else if (this.rValue_ !== undefined) {
      [this.hValue_, this.sValue_, this.lValue_] =
          Color.rgbToHSL(this.rValue_, this.gValue_, this.bValue_);
    } else if (this.hexValue_ !== undefined) {
      [this.hValue_, this.sValue_, this.lValue_] =
          Color.hexToHSL(this.hexValue_);
    }
  }

  hslValues() {
    return [this.hValue, this.sValue, this.lValue];
  }

  asHSL() {
    return 'hsl(' + this.hValue + ',' + this.sValue + '%,' + this.lValue + '%)';
  }

  /**
   * @param {string} hexValue
   * @returns {number[]}
   */
  static hexToRGB(hexValue) {
    // Ex. 'ffffff' => '[255,255,255]'
    const colorValue = parseInt(hexValue, 16);
    return [
      (colorValue >> 16) & 255, (colorValue >> 8) & 255, colorValue & 255
    ];
  }

  /**
   * @param {...number} rgbValues
   * @returns {string}
   */
  static rgbToHex(...rgbValues) {
    // Ex. '[255,255,255]' => 'ffffff'
    return rgbValues.reduce((cumulativeHexValue, rgbValue) => {
      let hexValue = Number(rgbValue).toString(16);
      if (hexValue.length == 1) {
        hexValue = '0' + hexValue;
      }
      return (cumulativeHexValue + hexValue);
    }, '');
  }

  /**
   * The algorithm has been written based on the mathematical formula found at:
   * https://en.wikipedia.org/wiki/HSL_and_HSV#HSL_to_RGB.
   * @param {...number} hslValues
   * @returns {number[]}
   */
  static hslToRGB(...hslValues) {
    let [hValue, sValue, lValue] = hslValues;
    hValue /= 60;
    sValue /= 100;
    lValue /= 100;

    let rValue = lValue;
    let gValue = lValue;
    let bValue = lValue;
    let match = 0;
    if (sValue !== 0) {
      const chroma = (1 - Math.abs(2 * lValue - 1)) * sValue;
      const x = chroma * (1 - Math.abs(hValue % 2 - 1));
      match = lValue - chroma / 2;
      if ((0 <= hValue) && (hValue <= 1)) {
        rValue = chroma;
        gValue = x;
        bValue = 0;
      } else if ((1 < hValue) && (hValue <= 2)) {
        rValue = x;
        gValue = chroma;
        bValue = 0;
      } else if ((2 < hValue) && (hValue <= 3)) {
        rValue = 0;
        gValue = chroma;
        bValue = x;
      } else if ((3 < hValue) && (hValue <= 4)) {
        rValue = 0;
        gValue = x;
        bValue = chroma;
      } else if ((4 < hValue) && (hValue <= 5)) {
        rValue = x;
        gValue = 0;
        bValue = chroma;
      } else {
        // (5 < hValue) && (hValue < 6)
        rValue = chroma;
        gValue = 0;
        bValue = x;
      }
    }
    rValue = Math.round((rValue + match) * 255);
    gValue = Math.round((gValue + match) * 255);
    bValue = Math.round((bValue + match) * 255);
    return [rValue, gValue, bValue];
  }

  /**
   * The algorithm has been written based on the mathematical formula found at:
   * https://en.wikipedia.org/wiki/HSL_and_HSV#From_RGB.
   * @param {...number} rgbValues
   * @returns {number[]}
   */
  static rgbToHSL(...rgbValues) {
    const [rValue, gValue, bValue] = rgbValues.map((value) => value / 255);
    const max = Math.max(rValue, gValue, bValue);
    const min = Math.min(rValue, gValue, bValue);
    let hValue = 0;
    let sValue = 0;
    let lValue = (max + min) / 2;
    if (max !== min) {
      const diff = max - min;
      if (max === rValue) {
        hValue = ((gValue - bValue) / diff);
      } else if (max === gValue) {
        hValue = ((bValue - rValue) / diff) + 2;
      } else {
        // max === bValue
        hValue = ((rValue - gValue) / diff) + 4;
      }
      hValue = Math.round(hValue * 60);
      if (hValue < 0) {
        hValue += 360;
      }
      sValue = Math.round((diff / (1 - Math.abs(2 * lValue - 1))) * 100);
    }
    lValue = Math.round(lValue * 100);
    return [hValue, sValue, lValue];
  }

  /**
   * @param {...number} rgbValues
   * @returns {string}
   */
  static hslToHex(...hslValues) {
    return Color.rgbToHex(...Color.hslToRGB(...hslValues));
  }

  /**
   * @param {string} hexValue
   * @returns {...number}
   */
  static hexToHSL(hexValue) {
    return Color.rgbToHSL(...Color.hexToRGB(hexValue));
  }

  /**
   * @param {number[]} colorTripleA RGB or HSL values
   * @param {number[]} colorTripleB RGB or HSL values
   * Both color triples must be of the same color format.
   */
  static distance(colorTripleA, colorTripleB) {
    return Math.sqrt(
        Math.pow(colorTripleA[0] - colorTripleB[0], 2) +
        Math.pow(colorTripleA[1] - colorTripleB[1], 2) +
        Math.pow(colorTripleA[2] - colorTripleB[2], 2));
  }
}

/**
 * Point: Helper class to get/set coordinates for two dimensional points.
 */
class Point {
  /**
   * @param {number} x
   * @param {number} y
   */
  constructor(x, y) {
    this.set(x, y);
  }

  /**
   * @param {number} x
   * @param {number} y
   */
  set(x, y) {
    this.x = x;
    this.y = y;
  }

  get x() {
    return this.x_;
  }

  /**
   * @param {number} x
   */
  set x(x) {
    this.x_ = x;
  }

  get y() {
    return this.y_;
  }

  /**
   * @param {number} y
   */
  set y(y) {
    this.y_ = y;
  }
}

/**
 * ColorPicker: Custom element providing a color picker implementation.
 *              A color picker is comprised of three main parts: a visual color
 *              picker to allow visual selection of colors, a manual color
 *              picker to allow numeric selection of colors, and submission
 *              controls to save/discard new color selections.
 */
class ColorPicker extends HTMLElement {
  /**
   * @param {!Color} initialColor
   */
  constructor(initialColor) {
    super();

    if (global.params.isBorderTransparent) {
      this.style.borderColor = 'transparent';
    }

    this.selectedColor_ = initialColor;
    this.colorWhenOpened_ = initialColor;

    this.visualColorPicker_ = new VisualColorPicker(initialColor);
    this.manualColorPicker_ = new ManualColorPicker(initialColor);
    this.systemColorPicker_ = new SystemColorPicker();
    this.colorValueAXAnnouncer_ = new ColorValueAXAnnouncer();
    this.append(
        this.visualColorPicker_, this.manualColorPicker_,
        this.systemColorPicker_, this.colorValueAXAnnouncer_);

    this.visualColorPicker_.addEventListener(
        'visual-color-picker-initialized',
        this.onVisualColorPickerInitialized_);

    window.addEventListener('resize', this.onWindowResize_, {once: true});
  }

  onVisualColorPickerInitialized_ = () => {
    this.manualColorPicker_
        .addEventListener('manual-color-change', this.onManualColorChange_);

    this.addEventListener('visual-color-change', this.onVisualColorChange_);

    this.addEventListener('format-change', this.onFormatChange_);

    this.addEventListener('focusin', this.onFocusin_);

    window.addEventListener('message', this.onMessageReceived_);

    document.documentElement.addEventListener('keydown', this.onKeyDown_);

    // Announce color now as any fired visual-color-change event would not have
    // been caught by the listener as it was just added in this method.
    this.colorValueAXAnnouncer_.announceColor(this.selectedColor);
  };

  get selectedColor() {
    return this.selectedColor_;
  }

  /**
   * @param {!Color} newColor
   */
  set selectedColor(newColor) {
    this.selectedColor_ = newColor;
  }

  /**
   * @param {!Event} event
   */
  onManualColorChange_ = (event) => {
    const newColor = event.detail.color;
    if (!this.selectedColor.equals(newColor)) {
      this.selectedColor = newColor;
      this.updateVisualColorPicker(newColor);

      const selectedValue = newColor.asHex();
      window.pagePopupController.setValue(selectedValue);
    }
  };

  /**
   * @param {!Event} event
   */
  onVisualColorChange_ = (event) => {
    const newColor = event.detail.color;
    if (!this.selectedColor.equals(newColor)) {
      if (!this.processingManualColorChange_) {
        this.selectedColor = newColor;
        this.manualColorPicker_.color = newColor;

        this.colorValueAXAnnouncer_.announceColor(newColor);

        const selectedValue = newColor.asHex();
        window.pagePopupController.setValue(selectedValue);
      } else {
        // We are making a visual color change in response to a manual color
        // change. So we do not overwrite the manually specified values and do
        // not change the selected color.
      }
    }
  };

  /**
   * @param {!Color} newColor
   */
  updateVisualColorPicker(newColor) {
    // There may not be an exact match for newColor in the HueSlider or
    // ColorWell, in which case we will display the closest match. When this
    // happens though, we want the manually chosen values to remain the
    // selected values (as they were explicitly specified by the user).
    // Therefore, we need to prevent them from getting overwritten when
    // onVisualColorChange_ runs. We do this by setting the
    // processingManualColorChange_ flag here and checking for it inside
    // onVisualColorChange_. If the flag is set, the manual color values
    // will not be updated with the color shown in the visual color picker.
    this.processingManualColorChange_ = true;
    this.visualColorPicker_.color = newColor;
    this.processingManualColorChange_ = false;
  }


  /**
   * @param {!Event} event
   */
  onKeyDown_ = (event) => {
    switch (event.key) {
      case 'Enter':
        window.pagePopupController.closePopup();
        break;
      case 'Escape':
        if (this.selectedColor.equals(this.colorWhenOpened_)) {
          window.pagePopupController.closePopup();
        } else {
          this.manualColorPicker_.dispatchEvent(new CustomEvent(
              'manual-color-change',
              {bubbles: true, detail: {color: this.colorWhenOpened_}}));
        }
        break;
      case 'Tab':
        event.preventDefault();
        if (this.focusableElements_ === undefined) {
          this.updateFocusableElements_();
        }
        const length = this.focusableElements_.length;
        if (length > 0) {
          const currentFocusIndex =
              this.focusableElements_.indexOf(document.activeElement);
          let nextFocusIndex;
          if (event.shiftKey) {
            nextFocusIndex =
                (currentFocusIndex > 0) ? currentFocusIndex - 1 : length - 1;
          } else {
            nextFocusIndex = (currentFocusIndex + 1) % length;
          }
          this.focusableElements_[nextFocusIndex].focus({preventScroll: true});
        }
        break;
    }
  };

  onFormatChange_ = (event) => {
    this.updateFocusableElements_();
    this.colorValueAXAnnouncer_.updateColorFormat(event.detail.colorFormat);
  };

  onFocusin_ = (event) => {
    if (event.target instanceof ColorSelectionRing) {
      // Announce the current color when the user focuses the ColorWell or the
      // HueSlider.
      this.colorValueAXAnnouncer_.announceColor(this.selectedColor);
    } else if (event.target instanceof FormatToggler) {
      // Announce the current color format when the user focuses the
      // FormatToggler.
      this.colorValueAXAnnouncer_.announceColorFormat();
    }
  };

  updateFocusableElements_ = () => {
    this.focusableElements_ = Array.from(this.querySelectorAll(
        'color-value-container:not(.hidden-color-value-container) > input,' +
        '[tabindex]:not([tabindex=\'-1\'])'));
  };

  onWindowResize_ = () => {
    // Set focus on the first focusable element.
    if (this.focusableElements_ === undefined) {
      this.updateFocusableElements_();
    }
    this.focusableElements_[0].focus({preventScroll: true});
  };

  onMessageReceived_ = (event) => {
    eval(event.data);
    if (window.updateData && window.updateData.success) {
      // Update the popup with the color selected using the eye dropper.
      const selectedValue = new Color(window.updateData.color);
      this.selectedColor = selectedValue;
      this.manualColorPicker_.color = selectedValue;
      this.updateVisualColorPicker(selectedValue);

      const hexValue = selectedValue.asHex();
      window.pagePopupController.setValue(hexValue);
    }
    this.visualColorPicker_.eyeDropper.finished();
    delete window.updateData;
  }
}
window.customElements.define('color-picker', ColorPicker);

/**
 * VisualColorPicker: Provides functionality to see the selected color and
 *                    select a different color visually.
 */
class VisualColorPicker extends HTMLElement {
  /**
   * @param {!Color} initialColor
   */
  constructor(initialColor) {
    super();

    let visualColorPickerStrip = document.createElement('span');
    visualColorPickerStrip.setAttribute('id', 'visual-color-picker-strip');
    this.eyeDropper_ = new EyeDropper();
    this.colorViewer_ = new ColorViewer(initialColor);
    this.hueSlider_ = new HueSlider(initialColor);
    visualColorPickerStrip.append(
        this.eyeDropper_, this.colorViewer_, this.hueSlider_);
    this.append(visualColorPickerStrip);

    this.colorWell_ = new ColorWell(initialColor);
    this.prepend(this.colorWell_);

    this.colorWell_.addEventListener('color-well-initialized', () => {
      this.initializeListeners_();
    });

    this.hueSlider_.addEventListener('hue-slider-initialized', () => {
      this.initializeListeners_();
    });
  }

  initializeListeners_ = () => {
    if (this.colorWell_.initialized && this.hueSlider_.initialized) {
      this.addEventListener('hue-slider-update', this.onHueSliderUpdate_);
      this.addEventListener('visual-color-change', this.onVisualColorChange_);
      this.colorWell_
          .addEventListener('mousedown', this.onColorWellMouseDown_);
      this.hueSlider_
          .addEventListener('mousedown', this.onHueSliderMouseDown_);
      document.documentElement
          .addEventListener('mousedown', this.onMouseDown_);
      document.documentElement
          .addEventListener('mousemove', this.onMouseMove_);
      document.documentElement.addEventListener('mouseup', this.onMouseUp_);
      this.colorWell_
          .addEventListener('touchstart', this.onColorWellTouchStart_);
      this.hueSlider_
          .addEventListener('touchstart', this.onHueSliderTouchStart_);
      document.documentElement
          .addEventListener('touchstart', this.onTouchStart_);
      document.documentElement
          .addEventListener('touchmove', this.onTouchMove_);
      document.documentElement.addEventListener('touchend', this.onTouchEnd_);
      document.documentElement.addEventListener('keydown', this.onKeyDown_);

      this.dispatchEvent(new CustomEvent('visual-color-picker-initialized'));
    }
  }

  onHueSliderUpdate_ = () => {
    this.colorWell_.fillColor = this.hueSlider_.color;
  }

  /**
   * @param {!Event} event
   */
  onVisualColorChange_ = (event) => {
    this.colorViewer_.color = event.detail.color;
  }

  /**
   * @param {!Event} event
   */
  onColorWellMouseDown_ = (event) => {
    event.preventDefault();
    event.stopPropagation();
    this.hueSlider_.focused = false;
    this.colorWell_.pointerDown(new Point(event.clientX, event.clientY));
  }

  /**
   * @param {!Event} event
   */
  onHueSliderMouseDown_ = (event) => {
    event.preventDefault();
    event.stopPropagation();
    this.colorWell_.focused = false;
    this.hueSlider_.pointerDown(new Point(event.clientX, event.clientY));
  }

  onMouseDown_ = () => {
    this.colorWell_.focused = false;
    this.hueSlider_.focused = false;
  }

  /**
   * @param {!Event} event
   */
  onMouseMove_ = (event) => {
    const point = new Point(event.clientX, event.clientY);
    this.colorWell_.pointerMove(point);
    this.hueSlider_.pointerMove(point);
  }

  onMouseUp_ = () => {
    this.colorWell_.pointerUp();
    this.hueSlider_.pointerUp();
  }

    /**
   * @param {!Event} event
   */
  onColorWellTouchStart_ = (event) => {
    event.preventDefault();
    event.stopPropagation();
    this.hueSlider_.focused = false;
    this.colorWell_.pointerDown(new Point(Math.round(event.touches[0].clientX), Math.round(event.touches[0].clientY)));
  }

  /**
   * @param {!Event} event
   */
  onHueSliderTouchStart_ = (event) => {
    event.preventDefault();
    event.stopPropagation();
    this.colorWell_.focused = false;
    this.hueSlider_.pointerDown(new Point(Math.round(event.touches[0].clientX), Math.round(event.touches[0].clientY)));
  }

  onTouchStart_ = () => {
    this.colorWell_.focused = false;
    this.hueSlider_.focused = false;
  }

  /**
   * @param {!Event} event
   */
  onTouchMove_ = (event) => {
    const point = new Point(Math.round(event.touches[0].clientX), Math.round(event.touches[0].clientY));
    this.colorWell_.pointerMove(point);
    this.hueSlider_.pointerMove(point);
  }

  onTouchEnd_ = () => {
    this.colorWell_.pointerUp();
    this.hueSlider_.pointerUp();
  }

  /**
   * @param {!Event} event
   */
  onKeyDown_ = (event) => {
    let moveDirection = Direction.UNDEFINED;
    switch(event.key) {
      case 'ArrowUp':
        moveDirection = Direction.UP;
        break;
      case 'ArrowDown':
        moveDirection = Direction.DOWN;
        break;
      case 'ArrowLeft':
        moveDirection = Direction.LEFT;
        break;
      case 'ArrowRight':
        moveDirection = Direction.RIGHT;
        break;
    }
    if (moveDirection !== Direction.UNDEFINED) {
      const acceleratedMove = event.ctrlKey;
      this.hueSlider_.move(moveDirection, acceleratedMove);
      this.colorWell_.move(moveDirection, acceleratedMove);
    }
  }

  /**
   * @param {!Color} newColor
   */
  set color(newColor) {
    this.hueSlider_.color = newColor;
    this.colorWell_.selectedColor = newColor;
  }

  get eyeDropper() {
    return this.eyeDropper_;
  }
}
window.customElements.define('visual-color-picker', VisualColorPicker);

/**
 * EyeDropper: Allows color selection from content outside the color picker.
 *             (This is currently just a placeholder for a future
 *             implementation.)
 * TODO(http://crbug.com/992297): Implement eye dropper
 */
class EyeDropper extends HTMLElement {
  constructor() {
    super();

    if (!global.params.isEyeDropperEnabled) {
      this.classList.add('hidden');
      return;
    }

    this.setAttribute('tabIndex', 0);
    this.setAttribute('role', 'button');
    this.setAttribute('aria-label', global.params.axEyedropperLabel);
    this.addEventListener('click', this.onClick_);
    this.addEventListener('keydown', this.onKeyDown_);
  }

  onClick_ = () => {
    event.preventDefault();
    event.stopPropagation();
    this.classList.add('selected');
    window.pagePopupController.openEyeDropper();
  };

  /**
   * @param {!Event} event
   */
  onKeyDown_ = (event) => {
    switch (event.key) {
      case 'Enter':
        this.onClick_();
        break;
    }
  };

  finished = () => {
    this.classList.remove('selected');
  }
}
window.customElements.define('eye-dropper', EyeDropper);

/**
 * ColorViewer: Provides a view of the selected color.
 */
class ColorViewer extends HTMLElement {
  /**
   * @param {!Color} initialColor
   */
  constructor(initialColor) {
    super();

    this.color = initialColor;

    // Leave the ColorViewer out of the accessibility tree; it's redundant
    // with the updates from ColorValueAXAnnouncer.
    this.setAttribute('aria-hidden', 'true');
  }

  get color() {
    return this.color_;
  }

  /**
   * @param {!Color} color
   */
  set color(color) {
    if (this.color_ === undefined || !this.color_.equals(color)) {
      this.color_ = color;
      this.style.backgroundColor = color.asRGB();
    }
  }
}
window.customElements.define('color-viewer', ColorViewer);

/**
 * ColorSelectionArea: Base class for ColorWell and HueSlider that encapsulates
 *                     a ColorPalette and a ColorSelectionRing.
 */
class ColorSelectionArea extends HTMLElement {
  constructor() {
    super();

    this.colorPalette_ = new ColorPalette();
    this.colorSelectionRing_ = new ColorSelectionRing(this.colorPalette_);
    this.append(this.colorPalette_, this.colorSelectionRing_);
    this.initialized_ = false;

    this.colorSelectionRing_.addEventListener(
        'focus', this.onColorSelectionRingFocus_);
    this.colorSelectionRing_.addEventListener(
        'blur', this.onColorSelectionRingBlur_);
  }

  get initialized() {
    return this.initialized_;
  }

  onColorSelectionRingFocus_ = () => {
    this.focused_ = true;
  };

  onColorSelectionRingBlur_ = () => {
    this.focused_ = false;
  };

  /**
   * @param {!Point} point
   */
  pointerDown(point) {
    this.colorSelectionRing_.focus({preventScroll: true});
    this.colorSelectionRing_.drag = true;
    this.moveColorSelectionRingTo_(point);
  }

  /**
   * @param {!Point} point
   */
  pointerMove(point) {
    if (this.colorSelectionRing_.drag) {
      this.moveColorSelectionRingTo_(point);
    }
  }

  pointerUp() {
    this.colorSelectionRing_.drag = false;
  }

  /**
   * @param {!Direction} direction
   * @param {bool} accelerated
   */
  move(direction, accelerated) {
    if (this.focused) {
      this.colorSelectionRing_.move(direction, accelerated);
    }
  }

  get focused() {
    return this.focused_;
  }

  /**
   * @param {bool} focused
   */
  set focused(focused) {
    this.focused_ = focused;
  }
}
window.customElements.define('color-selection-area', ColorSelectionArea);

/**
 * ColorPalette: Displays a range of colors.
 */
class ColorPalette extends HTMLCanvasElement {
  constructor() {
    super();

    this.gradients_ = [];
  }

  /**
   * @param {...CanvasGradient} gradients
   */
  initialize(...gradients) {
    this.width = this.offsetWidth;
    this.height = this.offsetHeight;
    this.renderingContext.rect(0, 0, this.width, this.height);
    this.gradients_.push(...gradients);
    this.fillColor = new Color('hsl(0, 100%, 50%)');
  }

  get hslImageData() {
    if (this.pendingColorChange_) {
      const rgbaImageData =
          this.renderingContext.getImageData(0, 0, this.width, this.height)
              .data;
      this.hslImageData_ =
          rgbaImageData.reduce((hslArray, {}, currentIndex, rgbaArray) => {
            if ((currentIndex % 4) === 0) {
              hslArray.push(...Color.rgbToHSL(
                  rgbaArray[currentIndex], rgbaArray[currentIndex + 1],
                  rgbaArray[currentIndex + 2]));
            }
            return hslArray;
          }, []);
      this.pendingColorChange_ = false;
    }
    if (this.pendingHueChange_) {
      const hValueToSet = this.fillColor.hValue;
      this.hslImageData_.forEach(({}, currentIndex, hslArray) => {
        if ((currentIndex % 3) === 0) {
          hslArray[currentIndex] = hValueToSet;
        }
      });

      this.pendingHueChange_ = false;
    }
    return this.hslImageData_;
  }

  /**
   * @param {!Point} point
   */
  colorAtPoint(point) {
    const hslImageDataAtPoint =
        this.hslImageDataAtPoint_(point.x - this.left, point.y - this.top);
    return new Color(
        ColorFormat.HSL, hslImageDataAtPoint[0], hslImageDataAtPoint[1],
        hslImageDataAtPoint[2]);
  }

  /**
   * @param {number} x
   * @param {number} y
   */
  hslImageDataAtPoint_(x, y) {
    let offset = (y * this.width + x) * 3;
    // It is possible that the computed offset is larger than the hslImageData
    // array's length. This can happen at certain zoom levels (ex. 150%), where
    // the height of the color well is not a round number. The getImageData API
    // only works with integer values and will truncate decimal values. As
    // such, if the color well's selection ring is placed at the bottom of the
    // color well at such a zoom level, a valid data point for the ring's
    // position will not be found in the hslImageData array. When this happens,
    // we just report the color at the end of the hslImageData array. This will
    // be the same color that is seen at the bottom of the color well (black).
    offset = Math.min(offset, this.hslImageData.length - 3);
    return this.hslImageData.slice(offset, offset + 3);
  }

  get renderingContext() {
    return this.getContext('2d');
  }

  get fillColor() {
    return this.fillColor_;
  }

  /**
   * @param {!Color} color
   */
  set fillColor(color) {
    this.fillColor_ = color;
    this.fillColorAndGradients_();
    this.pendingColorChange_ = true;
  }

  /**
   * @param {!Color} color
   */
  fillHue(color) {
    this.fillColor_ = new Color(
        ColorFormat.HSL, color.hValue, this.fillColor_.sValue,
        this.fillColor_.lValue);
    this.fillColorAndGradients_();
    this.pendingHueChange_ = true;
  }

  fillColorAndGradients_() {
    this.fillWithStyle_(this.fillColor_.asRGB());
    this.gradients_.forEach((gradient) => this.fillWithStyle_(gradient));
  }

  /**
   * @param {string|!CanvasGradient} fillStyle
   */
  fillWithStyle_(fillStyle) {
    let colorPaletteCtx = this.renderingContext;
    colorPaletteCtx.fillStyle = fillStyle;
    colorPaletteCtx.fill();
  }

  /**
   * @param {!Point} point
   */
  nearestPointOnColorPalette(point) {
    if (!this.isXCoordinateOnColorPalette_(point)) {
      if (point.x > this.right) {
        point.x = this.right;
      } else if (point.x < this.left) {
        point.x = this.left;
      }
    }
    if (!this.isYCoordinateOnColorPalette_(point)) {
      if (point.y > this.bottom) {
        point.y = this.bottom;
      } else if (point.y < this.top) {
        point.y = this.top;
      }
    }
    return point;
  }

  /**
   * @param {!Point} point
   */
  isXCoordinateOnColorPalette_(point) {
    return (point.x >= this.left) && (point.x <= this.right);
  }

  /**
   * @param {!Point} point
   */
  isYCoordinateOnColorPalette_(point) {
    return (point.y >= this.top) && (point.y <= this.bottom);
  }

  get left() {
    return Math.ceil(this.getBoundingClientRect().left);
  }

  get right() {
    return Math.ceil(this.getBoundingClientRect().right - 1);
  }

  get top() {
    return Math.ceil(this.getBoundingClientRect().top);
  }

  get bottom() {
    return Math.ceil(this.getBoundingClientRect().bottom - 1);
  }
}
window.customElements.define(
    'color-palette', ColorPalette, {extends: 'canvas'});

/**
 * ColorSelectionRing: Provides movement and color selection functionality to
 *                     pick colors from a given ColorPalette.
 */
class ColorSelectionRing extends HTMLElement {
  /**
   * @param {!ColorPalette} backingColorPalette
   */
  constructor(backingColorPalette) {
    super();

    this.setAttribute('tabIndex', 0);
    this.backingColorPalette_ = backingColorPalette;
    this.position_ = new Point(0, 0);
    this.drag_ = false;
  }

  static get ACCELERATED_MOVE_DISTANCE() {
    return 20;
  }

  static get MOVE_DISTANCE() {
    return 1;
  }

  initialize() {
    this.set(this.backingColorPalette_.left, this.backingColorPalette_.top);
    this.onPositionChange_();
  }

  /**
   * @param {!Point} newPosition
   */
  moveTo(newPosition) {
    this.set(newPosition.x, newPosition.y);
  }

  /**
   * @param {number} x
   * @param {number} y
   */
  set(x, y) {
    if ((x !== this.position_.x) || (y !== this.position_.y)) {
      this.position_.x = x;
      this.position_.y = y;
      this.onPositionChange_();
    }
  }

  /**
   * @param {number} x
   */
  setX(x) {
    if (x !== this.position_.x) {
      this.position_.x = x;
      this.onPositionChange_();
    }
  }

  /**
   * @param {number} x
   */
  setY(y) {
    if (y !== this.position_.y) {
      this.position_.y = y;
      this.onPositionChange_();
    }
  }

  /**
   * @param {number} shiftFactor
   */
  shiftX(shiftFactor) {
    this.setX(this.position_.x + shiftFactor);
  }

  onPositionChange_() {
    this.setElementPosition_();
    this.updatePositionForAria_();
    this.updateColor();
  }

  initializeAria(isForColorWell) {
    this.setAttribute('role', 'slider');
    this.isForColorWell = isForColorWell;
    this.setAttribute('aria-valuemin', 0);
    if (isForColorWell) {
      this.setAttribute('aria-label', global.params.axColorWellLabel);
      this.setAttribute(
          'aria-roledescription', global.params.axColorWellRoleDescription);
      this.setAttribute(
          'aria-valuemax',
          this.backingColorPalette_.offsetHeight *
              this.backingColorPalette_.offsetWidth);
    } else {
      this.setAttribute('aria-label', global.params.axHueSliderLabel);
      this.setAttribute(
          'aria-valuemax',
          this.backingColorPalette_.right - this.backingColorPalette_.left);
    }
    this.updatePositionForAria_();
  }

  updatePositionForAria_() {
    if (this.isForColorWell) {
      let positionX = (this.position_.x - this.backingColorPalette_.left);
      let positionY = (this.position_.y - this.backingColorPalette_.top);
      let colorWellWidth =
          (this.backingColorPalette_.right - this.backingColorPalette_.left);

      // aria-valuenow only takes a single numeric value, so we use this
      // scheme to collapse the 2-D coordinates into a 1-D slider value.
      this.setAttribute(
          'aria-valuenow', (positionY * colorWellWidth) + positionX);

      this.setAttribute('aria-valuetext', `X: ${positionX}, Y: ${positionY}`);
    } else {
      this.setAttribute(
          'aria-valuenow', this.position_.x - this.backingColorPalette_.left);
    }
  }

  setElementPosition_() {
    if (this.height > this.backingColorPalette_.height) {
      this.style.top = this.top -
          (this.height - this.backingColorPalette_.height) / 2 -
          this.backingColorPalette_.top + 'px';
    } else {
      this.style.top =
          this.top - this.radius - this.backingColorPalette_.top + 'px';
    }
    if (this.width > this.backingColorPalette_.width) {
      this.style.left = this.left -
          (this.width - this.backingColorPalette_.width) / 2 -
          this.backingColorPalette_.left + 'px';
    } else {
      this.style.left =
          this.left - this.radius - this.backingColorPalette_.left + 'px';
    }
  }

  updateColor() {
    this.color = this.backingColorPalette_.colorAtPoint(this.position_);
    this.dispatchEvent(new CustomEvent('color-selection-ring-update'));
  }

  get color() {
    return this.color_;
  }

  /**
   * @param {!Color} color
   */
  set color(color) {
    if (this.color_ === undefined || !this.color_.equals(color)) {
      this.color_ = color;
      this.style.backgroundColor = color.asRGB();
    }
  }

  get canMoveHorizontally_() {
    return this.width < this.backingColorPalette_.width;
  }

  get canMoveVertically_() {
    return this.height < this.backingColorPalette_.height;
  }

  /**
   * @param {!Direction} direction
   * @param {bool} accelerated
   */
  move(direction, accelerated) {
    let shiftFactor = accelerated ?
        ColorSelectionRing.ACCELERATED_MOVE_DISTANCE :
        ColorSelectionRing.MOVE_DISTANCE;
    if ((direction === Direction.UP) || (direction === Direction.LEFT)) {
      shiftFactor *= -1;
    }
    if (this.canMoveHorizontally_ &&
        ((direction === Direction.LEFT) || (direction === Direction.RIGHT))) {
      let newX = this.position_.x + shiftFactor;
      if (direction === Direction.LEFT) {
        if (this.position_.x + shiftFactor < this.backingColorPalette_.left) {
          newX = this.backingColorPalette_.left;
        }
      } else {
        // direction === Direction.RIGHT
        if (this.position_.x + shiftFactor > this.backingColorPalette_.right) {
          newX = this.backingColorPalette_.right;
        }
      }
      this.setX(newX);
    } else if (
        this.canMoveVertically_ &&
        ((direction === Direction.UP) || (direction === Direction.DOWN))) {
      let newY = this.position_.y + shiftFactor;
      if (direction === Direction.UP) {
        if (this.position_.y + shiftFactor < this.backingColorPalette_.top) {
          newY = this.backingColorPalette_.top;
        }
      } else {
        // direction === Direction.DOWN
        if (this.position_.y + shiftFactor > this.backingColorPalette_.bottom) {
          newY = this.backingColorPalette_.bottom;
        }
      }
      this.setY(newY);
    }
  }

  get drag() {
    return this.drag_;
  }

  /**
   * @param {boolean} drag
   */
  set drag(drag) {
    this.drag_ = drag;
  }

  get radius() {
    return this.width / 2;
  }

  get width() {
    return Math.floor(this.getBoundingClientRect().width);
  }

  get height() {
    return Math.floor(this.getBoundingClientRect().height);
  }

  get left() {
    return this.position_.x;
  }

  get top() {
    return this.position_.y;
  }
}
window.customElements.define('color-selection-ring', ColorSelectionRing);

/**
 * ColorWell: Allows selection from a range of colors, between black and white,
 *            that have the same hue value.
 */
class ColorWell extends ColorSelectionArea {
  /**
   * @param {!Color} initialColor
   */
  constructor(initialColor) {
    super();

    this.fillColor_ = new Color(ColorFormat.HSL, initialColor.hValue, 100, 50);
    this.selectedColor_ = initialColor;

    this.resizeObserver_ = new ResizeObserver(() => {
      let whiteGradient =
          this.colorPalette_.renderingContext.createLinearGradient(
              0, 0, this.colorPalette_.offsetWidth, 0);
      whiteGradient.addColorStop(0.01, 'hsla(0, 0%, 100%, 1)');
      whiteGradient.addColorStop(0.99, 'hsla(0, 0%, 100%, 0)');
      let blackGradient =
          this.colorPalette_.renderingContext.createLinearGradient(
              0, this.colorPalette_.offsetHeight, 0, 0);
      blackGradient.addColorStop(0.01, 'hsla(0, 0%, 0%, 1)');
      blackGradient.addColorStop(0.99, 'hsla(0, 0%, 0%, 0)');
      this.colorPalette_.initialize(whiteGradient, blackGradient);
      this.colorPalette_.fillHue(this.fillColor_);
      this.colorSelectionRing_.initialize();

      this.colorSelectionRing_.addEventListener(
          'color-selection-ring-update', this.onColorSelectionRingUpdate_);

      this.moveColorSelectionRingTo_(this.selectedColor_);
      this.colorSelectionRing_.initializeAria(/*isForColorWell*/ true);

      this.resizeObserver_.disconnect();
      this.resizeObserver_ = null;

      this.initialized_ = true;
      this.dispatchEvent(new CustomEvent('color-well-initialized'));
    });
    this.resizeObserver_.observe(this);
  }

  /**
   * @param {!Point|!Color} newPositionOrColor
   */
  moveColorSelectionRingTo_(newPositionOrColor) {
    if (newPositionOrColor instanceof Point) {
      const point =
          this.colorPalette_.nearestPointOnColorPalette(newPositionOrColor);
      this.colorSelectionRing_.moveTo(point);
    } else {
      const closestHSLValueIndex = this.colorPalette_.hslImageData.reduce(
          (closestSoFar, {}, index, array) => {
            if ((index % 3) === 0) {
              const currentHSLValueDistance = Color.distance(
                  [array[index], array[index + 1], array[index + 2]],
                  newPositionOrColor.hslValues());
              const closestHSLValueDistance = Color.distance(
                  [
                    array[closestSoFar], array[closestSoFar + 1],
                    array[closestSoFar + 2]
                  ],
                  newPositionOrColor.hslValues());
              if (currentHSLValueDistance < closestHSLValueDistance) {
                return index;
              }
            }
            return closestSoFar;
          },
          0);
      const offsetX = (closestHSLValueIndex / 3) % this.colorPalette_.width;
      const offsetY =
          Math.floor((closestHSLValueIndex / 3) / this.colorPalette_.width);
      this.colorSelectionRing_.set(
          this.colorPalette_.left + offsetX, this.colorPalette_.top + offsetY);
    }
  }

  get selectedColor() {
    return this.selectedColor_;
  }

  /**
   * @param {!Color} newColor
   */
  set selectedColor(newColor) {
    if (!this.selectedColor_.equals(newColor)) {
      this.selectedColor_ = newColor;
      this.moveColorSelectionRingTo_(newColor);
    }
  }

  get fillColor() {
    return this.fillColor_;
  }

  /**
   * @param {!Color} color
   */
  set fillColor(color) {
    if (!this.fillColor_.equals(color)) {
      this.fillColor_ = color;
      this.colorPalette_.fillHue(color);
      this.colorSelectionRing_.updateColor();
    }
  }

  onColorSelectionRingUpdate_ = () => {
    this.selectedColor_ = this.colorSelectionRing_.color;
    this.dispatchEvent(new CustomEvent(
        'visual-color-change',
        {bubbles: true, detail: {color: this.selectedColor}}));
  }
}
window.customElements.define('color-well', ColorWell);

/**
 * HueSlider: Allows selection from a range of colors with distinct hue values.
 */
class HueSlider extends ColorSelectionArea {
  /**
   * @param {!Color} initialColor
   */
  constructor(initialColor) {
    super();

    this.color_ = new Color(ColorFormat.HSL, initialColor.hValue, 100, 50);

    this.resizeObserver_ = new ResizeObserver(() => {
      let hueSliderPaletteGradient =
          this.colorPalette_.renderingContext.createLinearGradient(
              0, 0, this.colorPalette_.offsetWidth, 0);
      hueSliderPaletteGradient.addColorStop(0.01, 'hsl(0, 100%, 50%)');
      hueSliderPaletteGradient.addColorStop(0.17, 'hsl(300, 100%, 50%)');
      hueSliderPaletteGradient.addColorStop(0.33, 'hsl(240, 100%, 50%)');
      hueSliderPaletteGradient.addColorStop(0.5, 'hsl(180, 100%, 50%)');
      hueSliderPaletteGradient.addColorStop(0.67, 'hsl(120, 100%, 50%)');
      hueSliderPaletteGradient.addColorStop(0.83, 'hsl(60, 100%, 50%)');
      hueSliderPaletteGradient.addColorStop(0.99, 'hsl(0, 100%, 50%)');
      this.colorPalette_.initialize(hueSliderPaletteGradient);
      this.colorSelectionRing_.initialize();

      this.colorSelectionRing_.addEventListener(
          'color-selection-ring-update', this.onColorSelectionRingUpdate_);

      this.moveColorSelectionRingTo_(this.color_);
      this.colorSelectionRing_.initializeAria(/*isForColorWell*/ false);

      this.resizeObserver_.disconnect();
      this.resizeObserver_ = null;

      this.initialized_ = true;
      this.dispatchEvent(new CustomEvent('hue-slider-initialized'));
    });
    this.resizeObserver_.observe(this);
  }

  /**
   * @param {!Point|!Color} newPositionOrColor
   */
  moveColorSelectionRingTo_(newPositionOrColor) {
    if (newPositionOrColor instanceof Point) {
      const point =
          this.colorPalette_.nearestPointOnColorPalette(newPositionOrColor);
      this.colorSelectionRing_.shiftX(point.x - this.colorSelectionRing_.left);
    } else {
      const targetHValue = newPositionOrColor.hValue;
      if (targetHValue !== this.colorSelectionRing_.color.hValue) {
        const closestHValueIndex = this.colorPalette_.hslImageData.reduce(
            (closestHValueIndexSoFar, currentHValue, index, array) => {
              if ((index % 3 === 0) &&
                  (Math.abs(currentHValue - targetHValue) <
                   Math.abs(array[closestHValueIndexSoFar] - targetHValue))) {
                return index;
              }
              return closestHValueIndexSoFar;
            },
            0);
        const offsetX = (closestHValueIndex / 3) % this.colorPalette_.width;
        this.colorSelectionRing_.setX(this.colorPalette_.left + offsetX);
      }
    }
  }

  get color() {
    return this.color_;
  }

  /**
   * @param {!Color} newColor
   */
  set color(newColor) {
    if (this.color_.hValue !== newColor.hValue) {
      this.color_ = new Color(ColorFormat.HSL, newColor.hValue, 100, 50);
      this.moveColorSelectionRingTo_(this.color_);
    }
  }

  onColorSelectionRingUpdate_ = () => {
    this.color_ = this.colorSelectionRing_.color;
    this.dispatchEvent(new CustomEvent('hue-slider-update', {bubbles: true}));
  }
}
window.customElements.define('hue-slider', HueSlider);

/**
 * ManualColorPicker: Provides functionality to change the selected color by
 *                    manipulating its numeric values.
 */
class ManualColorPicker extends HTMLElement {
  /**
   * @param {!Color} initialColor
   */
  constructor(initialColor) {
    super();

    this.hexValueContainer_ =
        new ColorValueContainer(ColorChannel.HEX, initialColor);
    this.rgbValueContainer_ =
        new ColorValueContainer(ColorFormat.RGB, initialColor);
    this.hslValueContainer_ =
        new ColorValueContainer(ColorFormat.HSL, initialColor);
    this.colorValueContainers_ = [
      this.hexValueContainer_,
      this.rgbValueContainer_,
      this.hslValueContainer_,
    ];
    this.currentColorFormat_ = ColorFormat.RGB;
    this.adjustValueContainerVisibility_();
    this.formatToggler_ = new FormatToggler(this.currentColorFormat_);
    this.append(...this.colorValueContainers_, this.formatToggler_);

    this.formatToggler_.addEventListener('format-change', this.onFormatChange_);

    this.addEventListener('manual-color-change', this.onManualColorChange_);
  }

  adjustValueContainerVisibility_() {
    this.colorValueContainers_.forEach((colorValueContainer) => {
      if (colorValueContainer.colorFormat === this.currentColorFormat_) {
        colorValueContainer.show();
      } else {
        colorValueContainer.hide();
      }
    });
  }

  /**
   * @param {!Event} event
   */
  onFormatChange_ = (event) => {
    this.currentColorFormat_ = event.detail.colorFormat;
    this.adjustValueContainerVisibility_();
  }

  /**
   * @param {!Event} event
   */
  onManualColorChange_ = (event) => {
    this.color = event.detail.color;
  }

  /**
   * @param {!Color} newColor
   */
  set color(newColor) {
    this.colorValueContainers_.forEach(
        (colorValueContainer) => colorValueContainer.color = newColor);
  }
}
window.customElements.define('manual-color-picker', ManualColorPicker);

/**
 * ColorValueContainer: Maintains a set of channel values that make up a given
 *                      color format, and tracks value changes.
 */
class ColorValueContainer extends HTMLElement {
  /**
   * @param {!ColorFormat} colorFormat
   * @param {!Color} initialColor
   */
  constructor(colorFormat, initialColor) {
    super();

    this.colorFormat_ = colorFormat;
    this.channelValueContainers_ = [];
    if (this.colorFormat_ === ColorFormat.HEX) {
      const hexValueContainer =
          new ChannelValueContainer(ColorChannel.HEX, initialColor);
      this.channelValueContainers_.push(hexValueContainer);
    } else if (this.colorFormat_ === ColorFormat.RGB) {
      const rValueContainer =
          new ChannelValueContainer(ColorChannel.R, initialColor);
      const gValueContainer =
          new ChannelValueContainer(ColorChannel.G, initialColor);
      const bValueContainer =
          new ChannelValueContainer(ColorChannel.B, initialColor);
      this.channelValueContainers_.push(
          rValueContainer, gValueContainer, bValueContainer);
    } else if (this.colorFormat_ === ColorFormat.HSL) {
      const hValueContainer =
          new ChannelValueContainer(ColorChannel.H, initialColor);
      const sValueContainer =
          new ChannelValueContainer(ColorChannel.S, initialColor);
      const lValueContainer =
          new ChannelValueContainer(ColorChannel.L, initialColor);
      this.channelValueContainers_.push(
          hValueContainer, sValueContainer, lValueContainer);
    }
    this.append(...this.channelValueContainers_);

    this.channelValueContainers_.forEach(
        (channelValueContainer) => channelValueContainer.addEventListener(
            'input', this.onChannelValueChange_));
  }

  get colorFormat() {
    return this.colorFormat_;
  }

  get color() {
    return new Color(
        this.colorFormat_,
        ...this.channelValueContainers_.map(
            (channelValueContainer) => channelValueContainer.channelValue));
  }

  /**
   * @param {!Color} color
   */
  set color(color) {
    this.channelValueContainers_.forEach(
        (channelValueContainer) => channelValueContainer.setValue(color));
  }

  show() {
    return this.classList.remove('hidden-color-value-container');
  }

  hide() {
    return this.classList.add('hidden-color-value-container');
  }

  onChannelValueChange_ = () => {
    this.dispatchEvent(new CustomEvent(
        'manual-color-change', {bubbles: true, detail: {color: this.color}}));
  }
}
window.customElements.define('color-value-container', ColorValueContainer);

/**
 * ChannelValueContainer: Maintains and displays the numeric value
 *                        for a given color channel.
 */
class ChannelValueContainer extends HTMLInputElement {
  /**
   * @param {!ColorChannel} colorChannel
   * @param {!Color} initialColor
   */
  constructor(colorChannel, initialColor) {
    super();

    this.setAttribute('type', 'text');
    this.colorChannel_ = colorChannel;

    switch (colorChannel) {
      case ColorChannel.HEX:
        this.setAttribute('id', 'hexValueContainer');
        this.setAttribute('maxlength', '7');
        this.setAttribute('aria-label', global.params.axHexadecimalEditLabel);
        break;
      case ColorChannel.R:
        this.setAttribute('id', 'rValueContainer');
        this.setAttribute('maxlength', '3');
        this.setAttribute('aria-label', global.params.axRedEditLabel);
        break;
      case ColorChannel.G:
        this.setAttribute('id', 'gValueContainer');
        this.setAttribute('maxlength', '3');
        this.setAttribute('aria-label', global.params.axGreenEditLabel);
        break;
      case ColorChannel.B:
        this.setAttribute('id', 'bValueContainer');
        this.setAttribute('maxlength', '3');
        this.setAttribute('aria-label', global.params.axBlueEditLabel);
        break;
      case ColorChannel.H:
        this.setAttribute('id', 'hValueContainer');
        this.setAttribute('maxlength', '3');
        this.setAttribute('aria-label', global.params.axHueEditLabel);
        break;
      case ColorChannel.S:
        // up to 3 digits plus '%'
        this.setAttribute('id', 'sValueContainer');
        this.setAttribute('maxlength', '4');
        this.setAttribute('aria-label', global.params.axSaturationEditLabel);
        break;
      case ColorChannel.L:
        // up to 3 digits plus '%'
        this.setAttribute('id', 'lValueContainer');
        this.setAttribute('maxlength', '4');
        this.setAttribute('aria-label', global.params.axLightnessEditLabel);
        break;
    }
    this.setValue(initialColor);

    this.addEventListener('input', this.onValueChange_);
    this.addEventListener('blur', this.onBlur_);
    this.addEventListener('focus', this.onFocus_);
  }

  get channelValue() {
    return this.channelValue_;
  }

  /**
   * @param {!Color} color
   */
  setValue(color) {
    switch (this.colorChannel_) {
      case ColorChannel.HEX:
        if (this.channelValue_ !== color.hexValue) {
          this.channelValue_ = color.hexValue;
          this.value = '#' + this.channelValue_;
        }
        break;
      case ColorChannel.R:
        if (this.channelValue_ !== color.rValue) {
          this.channelValue_ = color.rValue;
          this.value = this.channelValue_;
        }
        break;
      case ColorChannel.G:
        if (this.channelValue_ !== color.gValue) {
          this.channelValue_ = color.gValue;
          this.value = this.channelValue_;
        }
        break;
      case ColorChannel.B:
        if (this.channelValue_ !== color.bValue) {
          this.channelValue_ = color.bValue;
          this.value = this.channelValue_;
        }
        break;
      case ColorChannel.H:
        if (this.channelValue_ !== color.hValue) {
          this.channelValue_ = color.hValue;
          this.value = this.channelValue_;
        }
        break;
      case ColorChannel.S:
        if (this.channelValue_ !== color.sValue) {
          this.channelValue_ = color.sValue;
          this.value = this.channelValue_ + '%';
        }
        break;
      case ColorChannel.L:
        if (this.channelValue_ !== color.lValue) {
          this.channelValue_ = color.lValue;
          this.value = this.channelValue_ + '%';
        }
        break;
    }
  }

  onValueChange_ = () => {
    // Set this.channelValue_ based on the element's new value.
    let value = this.value;
    if (value) {
      switch (this.colorChannel_) {
        case ColorChannel.HEX:
          if (value.startsWith('#'))
            value = value.substr(1).toLowerCase();
          if (value.match(/^[0-9a-f]+$/)) {
            // Ex. 'ffffff' => this.channelValue_ == 'ffffff'
            // Ex. 'ff' => this.channelValue_ == '0000ff'
            this.channelValue_ = ('000000' + value).slice(-6);
          }
          break;
        case ColorChannel.R:
        case ColorChannel.G:
        case ColorChannel.B:
          if (value.match(/^\d+$/) && (0 <= value) && (value <= 255)) {
            this.channelValue_ = Number(value);
          }
          break;
        case ColorChannel.H:
          if (value.match(/^\d+$/) && (0 <= value) && (value < 360)) {
            this.channelValue_ = Number(value);
          }
          break;
        case ColorChannel.S:
        case ColorChannel.L:
          if (value.endsWith('%'))
            value = value.substring(0, value.length - 1);
          if (value.match(/^\d+$/) && (0 <= value) && (value <= 100)) {
            this.channelValue_ = Number(value);
          }
          break;
      }
    }
  }

  onBlur_ = () => {
    switch (this.colorChannel_) {
      case ColorChannel.HEX:
        if (this.channelValue_ !== Number(this.value.substr(1))) {
          this.value = '#' + this.channelValue_;
        }
        break;
      case ColorChannel.R:
      case ColorChannel.G:
      case ColorChannel.B:
      case ColorChannel.H:
        if (this.channelValue_ !== Number(this.value)) {
          this.value = this.channelValue_;
        }
        break;
      case ColorChannel.S:
      case ColorChannel.L:
        if (this.channelValue_ !==
            Number(this.value.substring(0, this.value.length - 1))) {
          this.value = this.channelValue_ + '%';
        }
        break;
    }
  }

  onFocus_ = () => {
    this.select();
  }
}
window.customElements.define(
    'channel-value-container', ChannelValueContainer, {extends: 'input'});

/**
 * FormatToggler: Button that powers switching between different color formats.
 */
class FormatToggler extends HTMLElement {
  /**
   * @param {!ColorFormat} initialColorFormat
   */
  constructor(initialColorFormat) {
    super();

    this.setAttribute('tabIndex', 0);
    this.setAttribute('role', 'spinbutton');
    this.setAttribute('aria-label', global.params.axFormatTogglerLabel);
    this.setAttribute('aria-valuenow', '1');
    this.setAttribute('aria-valuemin', '1');
    this.setAttribute('aria-valuemax', '3');
    this.currentColorFormat_ = initialColorFormat;
    this.rgbFormatLabel_ = new FormatLabel(ColorFormat.RGB);
    this.hexFormatLabel_ = new FormatLabel(ColorFormat.HEX);
    this.hslFormatLabel_ = new FormatLabel(ColorFormat.HSL);
    this.colorFormatLabels_ = [
      this.rgbFormatLabel_,
      this.hexFormatLabel_,
      this.hslFormatLabel_,
    ];
    this.adjustFormatLabelVisibility_();

    this.upDownIcon_ = document.createElement('span');
    this.upDownIcon_.setAttribute('id', 'up-down-icon');
    this.upDownIcon_.innerHTML =
        '<svg class="up-down-icon" width="6" height="8" viewBox="0 0 6 8" fill="none" ' +
        'xmlns="http://www.w3.org/2000/svg"><path d="M1.18359 ' +
        '3.18359L0.617188 2.61719L3 0.234375L5.38281 2.61719L4.81641 ' +
        '3.18359L3 1.36719L1.18359 3.18359ZM4.81641 4.81641L5.38281 ' +
        '5.38281L3 7.76562L0.617188 5.38281L1.18359 4.81641L3 ' +
        '6.63281L4.81641 4.81641Z" fill="WindowText"/></svg>';

    this.append(...this.colorFormatLabels_, this.upDownIcon_);

    this.addEventListener('click', this.onClick_);
    this.addEventListener('keydown', this.onKeyDown_);
    this.addEventListener('mousedown', (event) => event.preventDefault());
  }

  /**
   * @param {bool} choosePreviousFormat if true, choose previous format
   *                                    instead of next
   */
  updateColorFormat_(choosePreviousFormat) {
    const numFormats = Object.keys(ColorFormat).length;
    const newValue = choosePreviousFormat ? this.currentColorFormat_ - 1 :
                                            this.currentColorFormat_ + 1;
    const newColorFormatKey = Object.keys(ColorFormat).filter((key) => {
      return ColorFormat[key] ===
          (((newValue % numFormats) + numFormats) % numFormats);
    });
    this.currentColorFormat_ = ColorFormat[newColorFormatKey];

    if (this.currentColorFormat_ === ColorFormat.RGB) {
      this.setAttribute('aria-valuenow', '1');
    } else if (this.currentColorFormat_ === ColorFormat.HSL) {
      this.setAttribute('aria-valuenow', '2');
    } else if (this.currentColorFormat_ === ColorFormat.HEX) {
      this.setAttribute('aria-valuenow', '3');
    }

    this.adjustFormatLabelVisibility_();

    this.dispatchEvent(new CustomEvent(
        'format-change',
        {bubbles: true, detail: {colorFormat: this.currentColorFormat_}}));
  }

  adjustFormatLabelVisibility_() {
    this.colorFormatLabels_.forEach((colorFormatLabel) => {
      if (colorFormatLabel.colorFormat === this.currentColorFormat_) {
        colorFormatLabel.show();
      } else {
        colorFormatLabel.hide();
      }
    });
  }

  onClick_ = () => {
    this.focus();
    this.updateColorFormat_(false);
  };

  /**
   * @param {!Event} event
   */
  onKeyDown_ = (event) => {
    switch (event.key) {
      case 'ArrowUp':
        this.updateColorFormat_(true);
        break;
      case 'ArrowDown':
      case ' ':
        this.updateColorFormat_(false);
        break;
    }
  }
}
window.customElements.define('format-toggler', FormatToggler);

/**
 * FormatLabel: Label for a given color format.
 */
class FormatLabel extends HTMLElement {
  /**
   * @param {!ColorFormat} colorFormat
   */
  constructor(colorFormat) {
    super();

    this.colorFormat_ = colorFormat;
    if (colorFormat === ColorFormat.HEX) {
      this.hexChannelLabel_ = new ChannelLabel(ColorChannel.HEX);
      this.append(this.hexChannelLabel_);
    } else if (colorFormat === ColorFormat.RGB) {
      this.rChannelLabel_ = new ChannelLabel(ColorChannel.R);
      this.gChannelLabel_ = new ChannelLabel(ColorChannel.G);
      this.bChannelLabel_ = new ChannelLabel(ColorChannel.B);
      this.append(
          this.rChannelLabel_, this.gChannelLabel_, this.bChannelLabel_);
    } else if (colorFormat === ColorFormat.HSL) {
      this.hChannelLabel_ = new ChannelLabel(ColorChannel.H);
      this.sChannelLabel_ = new ChannelLabel(ColorChannel.S);
      this.lChannelLabel_ = new ChannelLabel(ColorChannel.L);
      this.append(
          this.hChannelLabel_, this.sChannelLabel_, this.lChannelLabel_);
    }
  }

  get colorFormat() {
    return this.colorFormat_;
  }

  show() {
    return this.classList.remove('hidden-format-label');
  }

  hide() {
    return this.classList.add('hidden-format-label');
  }
}
window.customElements.define('format-label', FormatLabel);

/**
 * ChannelLabel: Label for a color channel, to be used within a FormatLabel.
 */
class ChannelLabel extends HTMLElement {
  /**
   * @param {!ColorChannel} colorChannel
   */
  constructor(colorChannel) {
    super();

    if (colorChannel === ColorChannel.HEX) {
      this.textContent = 'HEX';
    } else if (colorChannel === ColorChannel.R) {
      this.textContent = 'R';
    } else if (colorChannel === ColorChannel.G) {
      this.textContent = 'G';
    } else if (colorChannel === ColorChannel.B) {
      this.textContent = 'B';
    } else if (colorChannel === ColorChannel.H) {
      this.textContent = 'H';
    } else if (colorChannel === ColorChannel.S) {
      this.textContent = 'S';
    } else if (colorChannel === ColorChannel.L) {
      this.textContent = 'L';
    }
  }
}
window.customElements.define('channel-label', ChannelLabel);

/**
 * ColorValueAXAnnouncer: Make announcements to be read out by accessibility tools
 * when the color value is changed by the ColorWell or HueSlider.
 * Ideally it would be sufficient to just set the right ARIA attributes on the elements
 * themselves, but the color control does not fit neatly into existing ARIA roles.
 * ColorValueAXAnnouncer fills this gap by reading out color value changes using an
 * ARIA live region.
 */
class ColorValueAXAnnouncer extends HTMLElement {
  constructor() {
    super();
    this.setAttribute('aria-live', 'polite');
    this.colorFormat_ = ColorFormat.RGB;

    // We don't want this element to be visible so hide it off the edge of the popup.
    this.style.position = 'absolute';
    this.style.left = '-99999ch';

    this.addEventListener('format-change', this.onFormatChange_);
  }

  announceColor(newColor) {
    let announcementString = null;
    if (this.colorFormat_ === ColorFormat.HEX) {
      announcementString =
          `${global.params.axHexadecimalEditLabel} ${newColor.hexValue}`;
    } else if (this.colorFormat_ === ColorFormat.RGB) {
      announcementString =
          `${global.params.axRedEditLabel} ${newColor.rValue}, ${
              global.params.axGreenEditLabel} ${newColor.gValue}, ${
              global.params.axBlueEditLabel} ${newColor.bValue}`;
    } else if (this.colorFormat_ === ColorFormat.HSL) {
      announcementString =
          `${global.params.axHueEditLabel} ${newColor.hValue}, ${
              global.params.axSaturationEditLabel} ${newColor.sValue}, ${
              global.params.axLightnessEditLabel} ${newColor.lValue}`;
    }
    this.announce_(announcementString)
  }

  // Announce format changes via the live region in order to work around an
  // issue where Windows Narrator does not support aria-valuetext for
  // spinbutton.  The behavior that this achieves is similar to updating the
  // FormatToggler spinbutton's aria-valuetext whenever the format changes,
  // but it dodges the Narrator bug.
  // TODO(crbug.com/1073188): Remove this workaround and use aria-valuetext
  // instead once the Narrator bug has been fixed.
  announceColorFormat() {
    // These are deliberately non-localized so that they match the
    // abbreviations of the text on the FormatToggler ChannelLabels,
    // which are also not localized.
    let announcementString = null;
    if (this.colorFormat_ === ColorFormat.HEX) {
      announcementString = 'Hex';
    } else if (this.colorFormat_ === ColorFormat.RGB) {
      announcementString = 'RGB';
    } else if (this.colorFormat_ === ColorFormat.HSL) {
      announcementString = 'HSL';
    }

    this.announce_(announcementString)
  }

  updateColorFormat(newColorFormat) {
    this.colorFormat_ = newColorFormat;
    this.announceColorFormat();
  }

  announce_(announcementString) {
    // Only cue one announcement at a time so that user isn't spammed with a backlog
    // of announcements after holding down an arrow key.
    // Announce after a delay so that the control announces its raw position before
    // the full announcement starts.
    window.clearTimeout(this.pendingAnnouncement_);
    this.pendingAnnouncement_ = window.setTimeout(() => {
      if (this.textContent === announcementString) {
        // The AT will only do an announcement if the live-region content has
        // changed, so make a no-op change to fool it into announcing every
        // time.  Normal whitespace is ignored by Narrator for this purpose,
        // so use a non-breaking space.
        this.textContent += String.fromCharCode(160);
      } else {
        this.textContent = announcementString;
      }
    }, ColorValueAXAnnouncer.announcementDelayMS);
  }

  static announcementDelayMS = 500;
}
window.customElements.define('color-value-ax-announcer', ColorValueAXAnnouncer);

class SystemColorPicker extends HTMLElement {
  constructor() {
    super();
    if (!global.params.isSystemColorChooserEnabled) {
      this.classList.add('hidden');
      return;
    }
    this.setAttribute('tabIndex', 0);
    this.setAttribute('role', 'button');
    this.textContent = global.params.systemColorChooserLabel;
    this.setAttribute('aria-label', global.params.systemColorChooserLabel);
    this.addEventListener('click', this.onClick_);
    this.addEventListener('keydown', this.onKeyDown_);
  }

  onClick_ = () => {
    this.classList.add('selected');
    window.pagePopupController.openSystemColorChooser();
  };

  /**
   * @param {!Event} event
   */
  onKeyDown_ = (event) => {
    switch (event.key) {
      case 'Enter':
        this.onClick_();
        break;
    }
  };

  finished = () => {
    this.classList.remove('selected');
  }
}
window.customElements.define('system-color-picker', SystemColorPicker);
