import * as Promise from 'bluebird';

import {renderComponent} from './components/component';
import {ErrorLogger} from './components/error_logger';
import {Router} from './components/router';
import {isCobalt} from './utils/shared_values';

if (isCobalt) {
  // Use bluebird as the default promise because Cobalt does not support
  // unhandledrejection natively.
  window.Promise = Promise as any;
}

renderComponent(ErrorLogger, {}, document.querySelector('#error-logger')!);
renderComponent(Router, {}, document.querySelector('#router')!);
