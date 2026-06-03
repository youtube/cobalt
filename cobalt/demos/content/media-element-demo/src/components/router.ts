import {Component, renderComponent} from './component';

import {Watch} from './watch';

/** A component that parses URL params and decide what page to load. */
export class Router extends Component<{}> {
  /** @override */
  render() {
    return `<div class="router-content"></div>`;
  }

  /** @override */
  afterRender() {
    const params = parseParams();
    renderComponent(Watch, params, this.el.querySelector('.router-content')!);
  }
}

/**
 * Converts the GET params to an object.
 * @return The parsed params object.
 */
function parseParams(): Record<string, string|string[]> {
  const params: Record<string, string|string[]> = {};
  if (!location.search) {
    return params;
  }
  const terms = location.search.slice(1).split('&');
  for (const term of terms) {
    const [key, value] = term.split('=');
    if (params[key] !== undefined) {
      throw new Error(`Detected multiple values for ${key}`);
    }
    if (value.includes(',')) {
      params[key] = value.split(',');
    } else {
      params[key] = value;
    }
  }
  return params;
}
