// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

<<<<<<< HEAD
// Keep this import first.
import z from 'zod';
import '../base/disposable_polyfill';
import '../base/static_initializers';
import NON_CORE_PLUGINS from '../gen/all_plugins';
import CORE_PLUGINS from '../gen/all_core_plugins';
import m from 'mithril';
import {defer} from '../base/deferred';
import {addErrorHandler, reportError} from '../base/logging';
import {featureFlags} from '../core/feature_flags';
import {initLiveReload} from '../core/live_reload';
import {raf} from '../core/raf_scheduler';
import {initWasm} from '../trace_processor/wasm_engine_proxy';
import {UiMain} from './ui_main';
=======
// Need to turn off Long
import '../common/query_result';

import {Patch, produce} from 'immer';
import m from 'mithril';

import {defer} from '../base/deferred';
import {assertExists, reportError, setErrorHandler} from '../base/logging';
import {Actions, DeferredAction, StateActions} from '../common/actions';
import {createEmptyState} from '../common/empty_state';
import {RECORDING_V2_FLAG} from '../common/feature_flags';
import {initializeImmerJs} from '../common/immer_init';
import {pluginManager, pluginRegistry} from '../common/plugins';
import {onSelectionChanged} from '../common/selection_observer';
import {State} from '../common/state';
import {initWasm} from '../common/wasm_engine_proxy';
import {initController, runControllers} from '../controller';
import {
  isGetCategoriesResponse,
} from '../controller/chrome_proxy_record_controller';

import {AnalyzePage} from './analyze_page';
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
import {initCssConstants} from './css_constants';
import {registerDebugGlobals} from './debug';
import {maybeShowErrorDialog} from './error_dialog';
import {installFileDropHandler} from './file_drop_handler';
<<<<<<< HEAD
import {globals} from './globals';
import {HomePage} from './home_page';
import {postMessageHandler} from './post_message_handler';
import {Route, Router} from '../core/router';
import {CheckHttpRpcConnection} from './rpc_http_dialog';
import {maybeOpenTraceFromRoute} from './trace_url_handler';
import {renderViewerPage} from './viewer_page/viewer_page';
import {HttpRpcEngine} from '../trace_processor/http_rpc_engine';
import {showModal} from '../widgets/modal';
import {IdleDetector} from './idle_detector';
import {IdleDetectorWindow} from './idle_detector_interface';
import {AppImpl} from '../core/app_impl';
import {addLegacyTableTab} from '../components/details/sql_table_tab';
import {configureExtensions} from '../components/extensions';
import {
  addDebugCounterTrack,
  addDebugSliceTrack,
} from '../components/tracks/debug_tracks';
import {addVisualizedArgTracks} from '../components/tracks/visualized_args_tracks';
import {addQueryResultsTab} from '../components/query_table/query_result_tab';
import {assetSrc, initAssets} from '../base/assets';
import {
  PERFETTO_SETTINGS_STORAGE_KEY,
  SettingsManagerImpl,
} from '../core/settings_manager';
import {LocalStorage} from '../core/local_storage';
import {DurationPrecision, TimestampFormat} from '../public/timeline';

const CSP_WS_PERMISSIVE_PORT = featureFlags.register({
  id: 'cspAllowAnyWebsocketPort',
  name: 'Relax Content Security Policy for 127.0.0.1:*',
  description:
    'Allows simultaneous usage of several trace_processor_shell ' +
    '-D --http-port 1234 by opening ' +
    'https://ui.perfetto.dev/#!/?rpc_port=1234',
  defaultValue: false,
});

function routeChange(route: Route) {
  raf.scheduleFullRedraw(() => {
    if (route.fragment) {
      // This needs to happen after the next redraw call. It's not enough
      // to use setTimeout(..., 0); since that may occur before the
      // redraw scheduled above.
      const e = document.getElementById(route.fragment);
      if (e) {
        e.scrollIntoView();
      }
    }
  });
  maybeOpenTraceFromRoute(route);
=======
import {FlagsPage} from './flags_page';
import {globals} from './globals';
import {HomePage} from './home_page';
import {initLiveReloadIfLocalhost} from './live_reload';
import {MetricsPage} from './metrics_page';
import {postMessageHandler} from './post_message_handler';
import {RecordPage, updateAvailableAdbDevices} from './record_page';
import {RecordPageV2} from './record_page_v2';
import {Router} from './router';
import {CheckHttpRpcConnection} from './rpc_http_dialog';
import {TraceInfoPage} from './trace_info_page';
import {maybeOpenTraceFromRoute} from './trace_url_handler';
import {ViewerPage} from './viewer_page';
import {WidgetsPage} from './widgets_page';

const EXTENSION_ID = 'lfmkphfpdbjijhpomgecfikhfohaoine';

class FrontendApi {
  private state: State;

  constructor() {
    this.state = createEmptyState();
  }

  dispatchMultiple(actions: DeferredAction[]) {
    const oldState = this.state;
    const patches: Patch[] = [];
    for (const action of actions) {
      const originalLength = patches.length;
      const morePatches = this.applyAction(action);
      patches.length += morePatches.length;
      for (let i = 0; i < morePatches.length; ++i) {
        patches[i + originalLength] = morePatches[i];
      }
    }

    if (this.state === oldState) {
      return;
    }

    // Update overall state.
    globals.state = this.state;

    // If the visible time in the global state has been updated more recently
    // than the visible time handled by the frontend @ 60fps, update it. This
    // typically happens when restoring the state from a permalink.
    globals.frontendLocalState.mergeState(this.state.frontendLocalState);

    // Only redraw if something other than the frontendLocalState changed.
    let key: keyof State;
    for (key in this.state) {
      if (key !== 'frontendLocalState' && key !== 'visibleTracks' &&
          oldState[key] !== this.state[key]) {
        globals.rafScheduler.scheduleFullRedraw();
        break;
      }
    }

    if (this.state.currentSelection !== oldState.currentSelection) {
      // TODO(altimin): Currently we are not triggering this when changing
      // the set of selected tracks via toggling per-track checkboxes.
      // Fix that.
      onSelectionChanged(
          this.state.currentSelection || undefined,
          oldState.currentSelection || undefined);
    }

    if (patches.length > 0) {
      // Need to avoid reentering the controller so move this to a
      // separate task.
      setTimeout(() => {
        runControllers();
      }, 0);
    }
  }

  private applyAction(action: DeferredAction): Patch[] {
    const patches: Patch[] = [];

    // 'produce' creates a immer proxy which wraps the current state turning
    // all imperative mutations of the state done in the callback into
    // immutable changes to the returned state.
    this.state = produce(
        this.state,
        (draft) => {
          (StateActions as any)[action.type](draft, action.args);
        },
        (morePatches, _) => {
          const originalLength = patches.length;
          patches.length += morePatches.length;
          for (let i = 0; i < morePatches.length; ++i) {
            patches[i + originalLength] = morePatches[i];
          }
        });
    return patches;
  }
}

function setExtensionAvailability(available: boolean) {
  globals.dispatch(Actions.setExtensionAvailable({
    available,
  }));
}

function initGlobalsFromQueryString() {
  const queryString = window.location.search;
  globals.embeddedMode = queryString.includes('mode=embedded');
  globals.hideSidebar = queryString.includes('hideSidebar=true');
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
}

function setupContentSecurityPolicy() {
  // Note: self and sha-xxx must be quoted, urls data: and blob: must not.
<<<<<<< HEAD

  let rpcPolicy = [
    'http://127.0.0.1:9001', // For trace_processor_shell --httpd.
    'ws://127.0.0.1:9001', // Ditto, for the websocket RPC.
    'ws://127.0.0.1:9167', // For Web Device Proxy.
  ];
  if (CSP_WS_PERMISSIVE_PORT.get()) {
    const route = Router.parseUrl(window.location.href);
    if (/^\d+$/.exec(route.args.rpc_port ?? '')) {
      rpcPolicy = [
        `http://127.0.0.1:${route.args.rpc_port}`,
        `ws://127.0.0.1:${route.args.rpc_port}`,
      ];
    }
  }
=======
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  const policy = {
    'default-src': [
      `'self'`,
      // Google Tag Manager bootstrap.
      `'sha256-LirUKeorCU4uRNtNzr8tlB11uy8rzrdmqHCX38JSwHY='`,
    ],
    'script-src': [
      `'self'`,
      // TODO(b/201596551): this is required for Wasm after crrev.com/c/3179051
      // and should be replaced with 'wasm-unsafe-eval'.
      `'unsafe-eval'`,
      'https://*.google.com',
      'https://*.googleusercontent.com',
      'https://www.googletagmanager.com',
<<<<<<< HEAD
      'https://*.google-analytics.com',
=======
      'https://www.google-analytics.com',
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    ],
    'object-src': ['none'],
    'connect-src': [
      `'self'`,
<<<<<<< HEAD
      'ws://127.0.0.1:8037', // For the adb websocket server.
      'https://*.google-analytics.com',
      'https://*.googleapis.com', // For Google Cloud Storage fetches.
      'blob:',
      'data:',
    ].concat(rpcPolicy),
=======
      'http://127.0.0.1:9001',  // For trace_processor_shell --httpd.
      'ws://127.0.0.1:9001',    // Ditto, for the websocket RPC.
      'ws://127.0.0.1:8037',    // For the adb websocket server.
      'https://www.google-analytics.com',
      'https://*.googleapis.com',  // For Google Cloud Storage fetches.
      'blob:',
      'data:',
    ],
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    'img-src': [
      `'self'`,
      'data:',
      'blob:',
<<<<<<< HEAD
      'https://*.google-analytics.com',
      'https://www.googletagmanager.com',
      'https://*.googleapis.com',
    ],
    'style-src': [`'self'`, `'unsafe-inline'`],
=======
      'https://www.google-analytics.com',
      'https://www.googletagmanager.com',
    ],
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
    'navigate-to': ['https://*.perfetto.dev', 'self'],
  };
  const meta = document.createElement('meta');
  meta.httpEquiv = 'Content-Security-Policy';
  let policyStr = '';
  for (const [key, list] of Object.entries(policy)) {
    policyStr += `${key} ${list.join(' ')}; `;
  }
  meta.content = policyStr;
  document.head.appendChild(meta);
}

function main() {
<<<<<<< HEAD
  // Setup content security policy before anything else.
  setupContentSecurityPolicy();
  initAssets();

  // Create settings Manager
  const settingsManager = new SettingsManagerImpl(
    new LocalStorage(PERFETTO_SETTINGS_STORAGE_KEY),
  );

  // Initialize core settings...
  const timestampFormatSetting = settingsManager.register({
    id: 'timestampFormat',
    name: 'Timestamp format',
    description: 'The format of timestamps throughout Perfetto.',
    schema: z.nativeEnum(TimestampFormat),
    defaultValue: TimestampFormat.Timecode,
  });

  const durationPrecisionSetting = settingsManager.register({
    id: 'durationPrecision',
    name: 'Duration precision',
    description: 'The precision of durations throughout Perfetto.',
    schema: z.nativeEnum(DurationPrecision),
    defaultValue: DurationPrecision.Full,
  });

  AppImpl.initialize({
    initialRouteArgs: Router.parseUrl(window.location.href).args,
    settingsManager,
    timestampFormatSetting,
    durationPrecisionSetting,
  });

  // Load the css. The load is asynchronous and the CSS is not ready by the time
  // appendChild returns.
  const cssLoadPromise = defer<void>();
  const css = document.createElement('link');
  css.rel = 'stylesheet';
  css.href = assetSrc('perfetto.css');
  css.onload = () => cssLoadPromise.resolve();
  css.onerror = (err) => cssLoadPromise.reject(err);
  const favicon = document.head.querySelector('#favicon');
  if (favicon instanceof HTMLLinkElement) {
    favicon.href = assetSrc('assets/favicon.png');
  }

  // Load the script to detect if this is a Googler (see comments on globals.ts)
  // and initialize GA after that (or after a timeout if something goes wrong).
  function initAnalyticsOnScriptLoad() {
    AppImpl.instance.analytics.initialize(globals.isInternalUser);
  }
  const script = document.createElement('script');
  script.src =
    'https://storage.cloud.google.com/perfetto-ui-internal/is_internal_user.js';
  script.async = true;
  script.onerror = () => initAnalyticsOnScriptLoad();
  script.onload = () => initAnalyticsOnScriptLoad();
  setTimeout(() => initAnalyticsOnScriptLoad(), 5000);

  document.head.append(script, css);

  // Route errors to both the UI bugreport dialog and Analytics (if enabled).
  addErrorHandler(maybeShowErrorDialog);
  addErrorHandler((e) => AppImpl.instance.analytics.logError(e));

  // Add Error handlers for JS error and for uncaught exceptions in promises.
  window.addEventListener('error', (e) => reportError(e));
  window.addEventListener('unhandledrejection', (e) => reportError(e));

  initWasm();
  AppImpl.instance.serviceWorkerController.install();
=======
  setupContentSecurityPolicy();

  // Load the css. The load is asynchronous and the CSS is not ready by the time
  // appenChild returns.
  const cssLoadPromise = defer<void>();
  const css = document.createElement('link');
  css.rel = 'stylesheet';
  css.href = globals.root + 'perfetto.css';
  css.onload = () => cssLoadPromise.resolve();
  css.onerror = (err) => cssLoadPromise.reject(err);
  const favicon = document.head.querySelector('#favicon') as HTMLLinkElement;
  if (favicon) favicon.href = globals.root + 'assets/favicon.png';

  // Load the script to detect if this is a Googler (see comments on globals.ts)
  // and initialize GA after that (or after a timeout if something goes wrong).
  const script = document.createElement('script');
  script.src =
      'https://storage.cloud.google.com/perfetto-ui-internal/is_internal_user.js';
  script.async = true;
  script.onerror = () => globals.logging.initialize();
  script.onload = () => globals.logging.initialize();
  setTimeout(() => globals.logging.initialize(), 5000);

  document.head.append(script, css);

  // Add Error handlers for JS error and for uncaught exceptions in promises.
  setErrorHandler((err: string) => maybeShowErrorDialog(err));
  window.addEventListener('error', (e) => reportError(e));
  window.addEventListener('unhandledrejection', (e) => reportError(e));

  const extensionLocalChannel = new MessageChannel();

  initWasm(globals.root);
  initializeImmerJs();
  initController(extensionLocalChannel.port1);

  const dispatch = (action: DeferredAction) => {
    frontendApi.dispatchMultiple([action]);
  };

  const router = new Router({
    '/': HomePage,
    '/viewer': ViewerPage,
    '/record': RECORDING_V2_FLAG.get() ? RecordPageV2 : RecordPage,
    '/query': AnalyzePage,
    '/flags': FlagsPage,
    '/metrics': MetricsPage,
    '/info': TraceInfoPage,
    '/widgets': WidgetsPage,
  });
  router.onRouteChanged = (route) => {
    globals.rafScheduler.scheduleFullRedraw();
    maybeOpenTraceFromRoute(route);
  };

  // This must be called before calling `globals.initialize` so that the
  // `embeddedMode` global is set.
  initGlobalsFromQueryString();

  globals.initialize(dispatch, router);
  globals.serviceWorkerController.install();

  const frontendApi = new FrontendApi();
  globals.publishRedraw = () => globals.rafScheduler.scheduleFullRedraw();

  // We proxy messages between the extension and the controller because the
  // controller's worker can't access chrome.runtime.
  const extensionPort = window.chrome && chrome.runtime ?
      chrome.runtime.connect(EXTENSION_ID) :
      undefined;

  setExtensionAvailability(extensionPort !== undefined);

  if (extensionPort) {
    extensionPort.onDisconnect.addListener((_) => {
      setExtensionAvailability(false);
      void chrome.runtime.lastError;  // Needed to not receive an error log.
    });
    // This forwards the messages from the extension to the controller.
    extensionPort.onMessage.addListener(
        (message: object, _port: chrome.runtime.Port) => {
          if (isGetCategoriesResponse(message)) {
            globals.dispatch(Actions.setChromeCategories(message));
            return;
          }
          extensionLocalChannel.port2.postMessage(message);
        });
  }

  // This forwards the messages from the controller to the extension
  extensionLocalChannel.port2.onmessage = ({data}) => {
    if (extensionPort) extensionPort.postMessage(data);
  };
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

  // Put debug variables in the global scope for better debugging.
  registerDebugGlobals();

  // Prevent pinch zoom.
<<<<<<< HEAD
  document.body.addEventListener(
    'wheel',
    (e: MouseEvent) => {
      if (e.ctrlKey) e.preventDefault();
    },
    {passive: false},
  );

  cssLoadPromise.then(() => onCssLoaded());

  if (AppImpl.instance.testingMode) {
    document.body.classList.add('testing');
  }

  (window as {} as IdleDetectorWindow).waitForPerfettoIdle = (ms?: number) => {
    return new IdleDetector().waitForPerfettoIdle(ms);
  };
}

=======
  document.body.addEventListener('wheel', (e: MouseEvent) => {
    if (e.ctrlKey) e.preventDefault();
  }, {passive: false});

  cssLoadPromise.then(() => onCssLoaded());

  if (globals.testing) {
    document.body.classList.add('testing');
  }

  // Initialize all plugins:
  for (const plugin of pluginRegistry.values()) {
    pluginManager.activatePlugin(plugin.pluginId);
  }
}


>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
function onCssLoaded() {
  initCssConstants();
  // Clear all the contents of the initial page (e.g. the <pre> error message)
  // And replace it with the root <main> element which will be used by mithril.
<<<<<<< HEAD
  document.body.innerHTML = '';

  const pages = AppImpl.instance.pages;
  pages.registerPage({route: '/', render: () => m(HomePage)});
  pages.registerPage({route: '/viewer', render: () => renderViewerPage()});
  const router = new Router();
  router.onRouteChanged = routeChange;

  // Mount the main mithril component. This also forces a sync render pass.
  raf.mount(document.body, UiMain);

  if (
    (location.origin.startsWith('http://localhost:') ||
      location.origin.startsWith('http://127.0.0.1:')) &&
    !AppImpl.instance.embeddedMode &&
    !AppImpl.instance.testingMode
  ) {
    initLiveReload();
=======
  document.body.innerHTML = '<main></main>';
  const main = assertExists(document.body.querySelector('main'));
  globals.rafScheduler.domRedraw = () => {
    m.render(main, globals.router.resolve());
  };

  initLiveReloadIfLocalhost();

  if (!RECORDING_V2_FLAG.get()) {
    updateAvailableAdbDevices();
    try {
      navigator.usb.addEventListener(
          'connect', () => updateAvailableAdbDevices());
      navigator.usb.addEventListener(
          'disconnect', () => updateAvailableAdbDevices());
    } catch (e) {
      console.error('WebUSB API not supported');
    }
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
  }

  // Will update the chip on the sidebar footer that notifies that the RPC is
  // connected. Has no effect on the controller (which will repeat this check
  // before creating a new engine).
  // Don't auto-open any trace URLs until we get a response here because we may
  // accidentially clober the state of an open trace processor instance
  // otherwise.
<<<<<<< HEAD
  maybeChangeRpcPortFromFragment();
  CheckHttpRpcConnection().then(() => {
    const route = Router.parseUrl(window.location.href);
    if (!AppImpl.instance.embeddedMode) {
=======
  CheckHttpRpcConnection().then(() => {
    if (!globals.embeddedMode) {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      installFileDropHandler();
    }

    // Don't allow postMessage or opening trace from route when the user says
    // that they want to reuse the already loaded trace in trace processor.
<<<<<<< HEAD
    const traceSource = AppImpl.instance.trace?.traceInfo.source;
    if (traceSource && traceSource.type === 'HTTP_RPC') {
=======
    const engine = globals.getCurrentEngine();
    if (engine && engine.source.type === 'HTTP_RPC') {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      return;
    }

    // Add support for opening traces from postMessage().
    window.addEventListener('message', postMessageHandler, {passive: true});

    // Handles the initial ?local_cache_key=123 or ?s=permalink or ?url=...
    // cases.
<<<<<<< HEAD
    routeChange(route);
  });

  // Initialize plugins, now that we are ready to go.
  const pluginManager = AppImpl.instance.plugins;
  CORE_PLUGINS.forEach((p) => pluginManager.registerPlugin(p));
  NON_CORE_PLUGINS.forEach((p) => pluginManager.registerPlugin(p));
  const route = Router.parseUrl(window.location.href);
  const overrides = (route.args.enablePlugins ?? '').split(',');
  pluginManager.activatePlugins(overrides);
}

// If the URL is /#!?rpc_port=1234, change the default RPC port.
// For security reasons, this requires toggling a flag. Detect this and tell the
// user what to do in this case.
function maybeChangeRpcPortFromFragment() {
  const route = Router.parseUrl(window.location.href);
  if (route.args.rpc_port !== undefined) {
    if (!CSP_WS_PERMISSIVE_PORT.get()) {
      showModal({
        title: 'Using a different port requires a flag change',
        content: m(
          'div',
          m(
            'span',
            'For security reasons before connecting to a non-standard ' +
              'TraceProcessor port you need to manually enable the flag to ' +
              'relax the Content Security Policy and restart the UI.',
          ),
        ),
        buttons: [
          {
            text: 'Take me to the flags page',
            primary: true,
            action: () => Router.navigate('#!/flags/cspAllowAnyWebsocketPort'),
          },
        ],
      });
    } else {
      HttpRpcEngine.rpcPort = route.args.rpc_port;
    }
  }
}

// TODO(primiano): this injection is to break a cirular dependency. See
// comment in sql_table_tab_interface.ts. Remove once we add an extension
// point for context menus.
configureExtensions({
  addDebugCounterTrack,
  addDebugSliceTrack,
  addVisualizedArgTracks,
  addLegacySqlTableTab: addLegacyTableTab,
  addQueryResultsTab,
});

=======
    maybeOpenTraceFromRoute(Router.parseUrl(window.location.href));
  });
}

>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
main();
