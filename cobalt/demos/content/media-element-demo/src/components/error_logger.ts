import {errors} from '../utils/shared_values';

import {Component} from './component';

/** A component that holds error logs. */
export class ErrorLogger extends Component<{}> {
  /** @override */
  render() {
    return '';
  }

  /** @override */
  afterRender() {
    errors.observe((messages: string[]) => {
      this.renderError(messages);
    });
    window.addEventListener('error', (message) => {
      if (typeof message === 'string') {
        logError(message);
        return;
      }
    });
    window.addEventListener('unhandledrejection', (evt: any) => {
      // In cobalt where bluebird is used, the reason is under evt.detail.reason
      const reason = evt.reason ?? evt.detail.reason;
      logError(reason);
    });
  }

  private renderError(messages: string[]) {
    const elements = messages.map(message => `
      <div class="error">${message}</div>
    `);
    this.el.innerHTML = elements.join('');
  }
}

/**
 * Logs an error message. The logged error will be automatically picked up by
 * the error logger.
 * @param message The error message.
 */
export function logError(message: string) {
  const current = errors.get();
  errors.set([...current, message]);
}
