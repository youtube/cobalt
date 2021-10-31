type ComponentCtor<Props> = new (props: Props) => Component<Props>;
type ComponentFunc<Props> = (props: Props) => string;

/** Base component class. */
export class Component<Props> {
  el!: Element;

  constructor(readonly props: Props) {
    this.props = props;
  }

  /** Lifecycle method to generate static templates. */
  render(props: Props): string {
    throw new Error('Must be overridden.');
  }

  /**
   * Lifecycle method called after `render()`. It's a good opportunity to add
   * dynamic content and add event listeners.
   */
  afterRender() {}
}

/** Caches the component instances hooked to DOM elements. */
const componentMap = new WeakMap<Element, Component<unknown>>();

function isComponentCtor<Props>(
    CtorOrFunc: ComponentCtor<Props>|
    ComponentFunc<Props>): CtorOrFunc is ComponentCtor<Props> {
  if (CtorOrFunc.prototype instanceof Component) {
    return true;
  }
  return false;
}

/**
 * Renders a component to the container element.
 * @param CtorOrFunc Can be either a component contructor or a component
 *     function. The component function is easier to write but does not keep
 *     track of internal states.
 * @param props Component props.
 * @param container The container element of the component.
 */
export function renderComponent<Props>(
    CtorOrFunc: ComponentCtor<Props>|ComponentFunc<Props>, props: Props,
    container: Element) {
  if (!isComponentCtor(CtorOrFunc)) {
    // Function based component
    container.innerHTML = CtorOrFunc(props);
    return;
  }
  let component = componentMap.get(container) as Component<Props>| undefined;
  if (!(component instanceof CtorOrFunc)) {
    component = new CtorOrFunc(props);
    componentMap.set(container, component);
    component.el = container;
  }
  container.innerHTML = component.render(props);
  component.afterRender();
}
