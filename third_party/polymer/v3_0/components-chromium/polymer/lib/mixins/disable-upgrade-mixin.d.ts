// tslint:disable:variable-name Describing an API that's defined elsewhere.
// tslint:disable:no-any describes the API as best we are able today

import {ElementMixin} from './element-mixin.js';

import {dedupingMixin} from '../utils/mixin.js';

export {findObservedAttributesGetter};

declare function findObservedAttributesGetter(): any;

export {DisableUpgradeMixin};


/**
 * Element class mixin that allows the element to boot up in a non-enabled
 * state when the `disable-upgrade` attribute is present. This mixin is
 * designed to be used with element classes like PolymerElement that perform
 * initial startup work when they are first connected. When the
 * `disable-upgrade` attribute is removed, if the element is connected, it
 * boots up and "enables" as it otherwise would; if it is not connected, the
 * element boots up when it is next connected.
 *
 * Using `disable-upgrade` with PolymerElement prevents any data propagation
 * to the element, any element DOM from stamping, or any work done in
 * connected/disconnctedCallback from occuring, but it does not prevent work
 * done in the element constructor.
 *
 * Note, this mixin must be applied on top of any element class that
 * itself implements a `connectedCallback` so that it can control the work
 * done in `connectedCallback`. For example,
 *
 *     MyClass = DisableUpgradeMixin(class extends BaseClass {...});
 */
declare function DisableUpgradeMixin<T extends new (...args: any[]) => {}>(base: T): T & DisableUpgradeMixinConstructor & ElementMixinConstructor & PropertyEffectsConstructor & TemplateStampConstructor & PropertyAccessorsConstructor & PropertiesChangedConstructor & PropertiesMixinConstructor;

import {ElementMixinConstructor} from './element-mixin.js';

import {PropertyEffectsConstructor, PropertyEffects} from './property-effects.js';

import {TemplateStampConstructor, TemplateStamp} from './template-stamp.js';

import {PropertyAccessorsConstructor, PropertyAccessors} from './property-accessors.js';

import {PropertiesChangedConstructor, PropertiesChanged} from './properties-changed.js';

import {PropertiesMixinConstructor, PropertiesMixin} from './properties-mixin.js';

interface DisableUpgradeMixinConstructor {
  new(...args: any[]): DisableUpgradeMixin;
}

export {DisableUpgradeMixinConstructor};

interface DisableUpgradeMixin extends ElementMixin, PropertyEffects, TemplateStamp, PropertyAccessors, PropertiesChanged, PropertiesMixin {
  _initializeProperties(): void;
  _enableProperties(): void;

  /**
   * @param name Attribute name.
   * @param old The previous value for the attribute.
   * @param value The new value for the attribute.
   * @param namespace The XML namespace for the attribute.
   */
  attributeChangedCallback(name: string, old: string|null, value: string|null, namespace: string|null): void;
  connectedCallback(): void;
  disconnectedCallback(): void;
  _canApplyPropertyDefault(property: any): any;
}
