// tslint:disable:variable-name Describing an API that's defined elsewhere.
// tslint:disable:no-any describes the API as best we are able today

import {Class} from './class.js';

import {Polymer} from '../../polymer-legacy.js';

import {dedupingMixin} from '../utils/mixin.js';

import {templatize} from '../utils/templatize.js';

declare class UndefinedArgumentError extends Error {
  constructor(message: any, arg: any);
}

export {LegacyDataMixin};


/**
 * Mixin to selectively add back Polymer 1.x's `undefined` rules
 * governing when observers & computing functions run based
 * on all arguments being defined (reference https://www.polymer-project.org/1.0/docs/devguide/observers#multi-property-observers).
 *
 * When loaded, all legacy elements (defined with `Polymer({...})`)
 * will have the mixin applied. The mixin only restores legacy data handling
 * if `_legacyUndefinedCheck: true` is set on the element's prototype.
 *
 * This mixin is intended for use to help migration from Polymer 1.x to
 * 2.x+ by allowing legacy code to work while identifying observers and
 * computing functions that need undefined checks to work without
 * the mixin in Polymer 2.
 */
declare function LegacyDataMixin<T extends new (...args: any[]) => {}>(base: T): T & LegacyDataMixinConstructor;

interface LegacyDataMixinConstructor {
  new(...args: any[]): LegacyDataMixin;
}

export {LegacyDataMixinConstructor};

interface LegacyDataMixin {
}

declare function TemplatizeMixin<T extends new (...args: any[]) => {}>(base: T): T & TemplatizeMixinConstructor;

interface TemplatizeMixinConstructor {
  new(...args: any[]): TemplatizeMixin;
}

export {TemplatizeMixinConstructor};

interface TemplatizeMixin {
}

declare class legacyBase extends HTMLElement {
}
