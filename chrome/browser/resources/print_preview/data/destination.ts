// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {assert} from 'chrome://resources/js/assert.js';

import type {Cdd, ColorCapability, ColorOption, CopiesCapability, DpiOption, DuplexType, MediaSizeOption, MediaTypeOption} from './cdd.js';
/**
 * Enumeration of the origin types for destinations.
 */
export enum DestinationOrigin {
  LOCAL = 'local',
  // Note: Cookies, device and privet are deprecated, but used to filter any
  // legacy entries in the recent destinations, since we can't guarantee all
  // such recent printers have been overridden.
  COOKIES = 'cookies',
  PRIVET = 'privet',
  EXTENSION = 'extension',
  CROS = 'chrome_os',
}

/**
 * Printer types for capabilities and printer list requests.
 * Must match PrinterType in printing/mojom/print.mojom
 */
export enum PrinterType {
  PRIVET_PRINTER_DEPRECATED = 0,
  EXTENSION_PRINTER = 1,
  PDF_PRINTER = 2,
  LOCAL_PRINTER = 3,
  CLOUD_PRINTER_DEPRECATED = 4
}

/**
 * Enumeration of color modes used by Chromium.
 */
export enum ColorMode {
  GRAY = 1,
  COLOR = 2,
}

export interface RecentDestination {
  id: string;
  origin: DestinationOrigin;
  capabilities: Cdd|null;
  displayName: string;
  extensionId: string;
  extensionName: string;
  icon?: string;
}

export function isPdfPrinter(id: string): boolean {
  return id === GooglePromotedDestinationId.SAVE_AS_PDF;
}

/**
 * Creates a |RecentDestination| to represent |destination| in the app
 * state.
 */
export function makeRecentDestination(destination: Destination):
    RecentDestination {
  return {
    id: destination.id,
    origin: destination.origin,
    capabilities: destination.capabilities,
    displayName: destination.displayName || '',
    extensionId: destination.extensionId || '',
    extensionName: destination.extensionName || '',
    icon: destination.icon || '',
  };
}

/**
 * @return key that maps to a destination with the selected |id| and |origin|.
 */
export function createDestinationKey(
    id: string, origin: DestinationOrigin): string {
  return `${id}/${origin}/`;
}

/**
 * @return A key that maps to a destination with parameters matching
 *     |recentDestination|.
 */
export function createRecentDestinationKey(
    recentDestination: RecentDestination): string {
  return createDestinationKey(recentDestination.id, recentDestination.origin);
}

export interface DestinationOptionalParams {
  isEnterprisePrinter?: boolean;
  extensionId?: string;
  extensionName?: string;
  description?: string;
  location?: string;
}

/**
 * List of capability types considered color.
 */
const COLOR_TYPES: string[] = ['STANDARD_COLOR', 'CUSTOM_COLOR'];

/**
 * List of capability types considered monochrome.
 */
const MONOCHROME_TYPES: string[] = ['STANDARD_MONOCHROME', 'CUSTOM_MONOCHROME'];


/**
 * Print destination data object.
 */
export class Destination {
  /**
   * ID of the destination.
   */
  private id_: string;

  /**
   * Origin of the destination.
   */
  private origin_: DestinationOrigin;

  /**
   * Display name of the destination.
   */
  private displayName_: string;

  /**
   * Print capabilities of the destination.
   */
  private capabilities_: Cdd|null = null;

  /**
   * Whether the destination is an enterprise policy controlled printer.
   */
  private isEnterprisePrinter_: boolean;

  /**
   * Destination location.
   */
  private location_: string = '';

  /**
   * Printer description.
   */
  private description_: string;

  /**
   * Extension ID for extension managed printers.
   */
  private extensionId_: string;

  /**
   * Extension name for extension managed printers.
   */
  private extensionName_: string;

  private type_: PrinterType;

  constructor(
      id: string, origin: DestinationOrigin, displayName: string,
      params?: DestinationOptionalParams) {
    this.id_ = id;
    this.origin_ = origin;
    this.displayName_ = displayName || '';
    this.isEnterprisePrinter_ = (params && params.isEnterprisePrinter) || false;
    this.description_ = (params && params.description) || '';
    this.extensionId_ = (params && params.extensionId) || '';
    this.extensionName_ = (params && params.extensionName) || '';
    this.location_ = (params && params.location) || '';
    this.type_ = this.computeType_(id, origin);
  }

  private computeType_(id: string, origin: DestinationOrigin): PrinterType {
    if (isPdfPrinter(id)) {
      return PrinterType.PDF_PRINTER;
    }

    return origin === DestinationOrigin.EXTENSION ?
        PrinterType.EXTENSION_PRINTER :
        PrinterType.LOCAL_PRINTER;
  }

  get type(): PrinterType {
    return this.type_;
  }

  get id(): string {
    return this.id_;
  }

  get origin(): DestinationOrigin {
    return this.origin_;
  }

  get displayName(): string {
    return this.displayName_;
  }

  /**
   * @return Whether the destination is an extension managed printer.
   */
  get isExtension(): boolean {
    return this.origin_ === DestinationOrigin.EXTENSION;
  }

  /**
   * @return Most relevant string to help user to identify this
   *     destination.
   */
  get hint(): string {
    return this.location_ || this.extensionName || this.description_;
  }

  /**
   * @return Extension ID associated with the destination. Non-empty
   *     only for extension managed printers.
   */
  get extensionId(): string {
    return this.extensionId_;
  }

  /**
   * @return Extension name associated with the destination.
   *     Non-empty only for extension managed printers.
   */
  get extensionName(): string {
    return this.extensionName_;
  }

  /** @return Print capabilities of the destination. */
  get capabilities(): Cdd|null {
    return this.capabilities_;
  }

  set capabilities(capabilities: Cdd|null) {
    if (capabilities) {
      this.capabilities_ = capabilities;
    }
  }

  /** @return Path to the SVG for the destination's icon. */
  get icon(): string {
    if (this.id_ === GooglePromotedDestinationId.SAVE_AS_PDF) {
      return 'cr:insert-drive-file';
    }
    if (this.isEnterprisePrinter) {
      return 'print-preview:business';
    }
    return 'print-preview:print';
  }

  /**
   * @return Properties (besides display name) to match search queries against.
   */
  get extraPropertiesToMatch(): string[] {
    return [this.location_, this.description_];
  }

  /**
   * Matches a query against the destination.
   * @param query Query to match against the destination.
   * @return Whether the query matches this destination.
   */
  matches(query: RegExp): boolean {
    return !!this.displayName_.match(query) ||
        !!this.extensionName_.match(query) || !!this.location_.match(query) ||
        !!this.description_.match(query);
  }

  /**
   * Whether the printer is enterprise policy controlled printer.
   */
  get isEnterprisePrinter(): boolean {
    return this.isEnterprisePrinter_;
  }

  private copiesCapability_(): CopiesCapability|null {
    return this.capabilities && this.capabilities.printer &&
            this.capabilities.printer.copies ?
        this.capabilities.printer.copies :
        null;
  }

  private colorCapability_(): ColorCapability|null {
    return this.capabilities && this.capabilities.printer &&
            this.capabilities.printer.color ?
        this.capabilities.printer.color :
        null;
  }

  /** @return Whether the printer supports copies. */
  get hasCopiesCapability(): boolean {
    const capability = this.copiesCapability_();
    if (!capability) {
      return false;
    }
    return capability.max ? capability.max > 1 : true;
  }

  /**
   * @return Whether the printer supports both black and white and
   *     color printing.
   */
  get hasColorCapability(): boolean {
    const capability = this.colorCapability_();
    if (!capability || !capability.option) {
      return false;
    }
    let hasColor = false;
    let hasMonochrome = false;
    capability.option.forEach(option => {
      assert(option.type);
      hasColor = hasColor || COLOR_TYPES.includes(option.type);
      hasMonochrome = hasMonochrome || MONOCHROME_TYPES.includes(option.type);
    });
    return hasColor && hasMonochrome;
  }

  /**
   * @param isColor Whether to use a color printing mode.
   * @return Native color model of the destination.
   */
  getNativeColorModel(isColor: boolean): number {
    // For printers without capability, native color model is ignored.
    const capability = this.colorCapability_();
    if (!capability || !capability.option) {
      return isColor ? ColorMode.COLOR : ColorMode.GRAY;
    }
    const selected = this.getColor(isColor);
    const mode = parseInt(selected ? selected.vendor_id! : '', 10);
    if (isNaN(mode)) {
      return isColor ? ColorMode.COLOR : ColorMode.GRAY;
    }
    return mode;
  }

  /**
   * @return The default color option for the destination.
   */
  get defaultColorOption(): ColorOption|null {
    const capability = this.colorCapability_();
    if (!capability || !capability.option) {
      return null;
    }
    const defaultOptions = capability.option.filter(option => {
      return option.is_default;
    });
    return defaultOptions.length !== 0 ? defaultOptions[0] : null;
  }

  /**
   * @return Color option value of the destination with the given binary color
   * value. Returns null if the destination doesn't support such color value.
   */
  getColor(isColor: boolean): ColorOption|null {
    const typesToLookFor = isColor ? COLOR_TYPES : MONOCHROME_TYPES;
    const capability = this.colorCapability_();
    if (!capability || !capability.option) {
      return null;
    }
    for (let i = 0; i < typesToLookFor.length; i++) {
      const matchingOptions = capability.option.filter(option => {
        return option.type === typesToLookFor[i];
      });
      if (matchingOptions.length > 0) {
        return matchingOptions[0];
      }
    }
    return null;
  }

  /**
   * @return Media size value of the destination with the given width and height
   * values. Returns undefined if there is no such media size value.
   */
  getMediaSize(width: number, height: number): MediaSizeOption|undefined {
    return this.capabilities?.printer.media_size?.option.find(o => {
      return o.width_microns === width && o.height_microns === height;
    });
  }

  /**
   * @return Media type value of the destination with the given vendor id.
   * Returns undefined if there is no such media type value.
   */
  getMediaType(vendorId: string): MediaTypeOption|undefined {
    return this.capabilities?.printer.media_type?.option.find(o => {
      return o.vendor_id === vendorId;
    });
  }

  /**
   * @return DPI (Dots per Inch) value of the destination with the given
   * horizontal and vertical resolutions. Returns undefined if there is no such
   * DPI value.
   */
  getDpi(horizontal: number, vertical: number): DpiOption|undefined {
    return this.capabilities?.printer.dpi?.option.find(o => {
      return o.horizontal_dpi === horizontal && o.vertical_dpi === vertical;
    });
  }

  /**
   * @return Returns true if the current printing destination supports the given
   * duplex value. Returns false in all other cases.
   */
  supportsDuplex(duplex: DuplexType): boolean {
    const availableDuplexOptions = this.capabilities?.printer.duplex?.option;
    if (!availableDuplexOptions) {
      // There are no duplex capabilities reported by the printer.
      return false;
    }

    return availableDuplexOptions.some(o => {
      return o.type === duplex;
    });
  }

  /** @return A unique identifier for this destination. */
  get key(): string {
    return `${this.id_}/${this.origin_}/`;
  }
}

/**
 * Enumeration of Google-promoted destination IDs.
 * @enum {string}
 */
export enum GooglePromotedDestinationId {
  SAVE_AS_PDF = 'Save as PDF',
}

/* Unique identifier for the Save as PDF destination */
export const PDF_DESTINATION_KEY: string =
    `${GooglePromotedDestinationId.SAVE_AS_PDF}/${DestinationOrigin.LOCAL}/`;
