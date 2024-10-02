// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//
// CDPClient
//
class CDPClient {
  constructor() {
    this._requestId = 0;
    this._sessions = new Map();
  }

  nextRequestId() {
    return ++this._requestId;
  }

  addSession(session) {
    this._sessions.set(session.sessionId(), session);
  }

  getSession(sessionId) {
    this._sessions.get(sessionId);
  }

  async dispatchMessage(message) {
    const messageObject = JSON.parse(message);
    const session = this._sessions.get(messageObject.sessionId || '');
    if (session) {
      session.dispatchMessage(messageObject);
    }
  }

  reportError(message, error) {
    if (error) {
      console.error(`${message}: ${error}\n${error.stack}`);
    } else {
      console.error(message);
    }
  }
}

const cdpClient = new CDPClient();

//
// CDPSession
//
class CDPSession {
  constructor(sessionId) {
    this._sessionId = sessionId || '';
    this._parentSessionId = null;
    this._dispatchTable = new Map();
    this._eventHandlers = new Map();
    this._protocol = this._getProtocol();
    cdpClient.addSession(this);
  }

  sessionId() {
    return this._sessionId;
  }

  protocol() {
    return this._protocol;
  }

  createSession(sessionId) {
    const session = new CDPSession(sessionId);
    session._parentSessionId = this._sessionId;
    return session;
  }

  async sendCommand(method, params) {
    const requestId = cdpClient.nextRequestId();
    const messageObject = {'id': requestId, 'method': method, 'params': params};
    if (this._sessionId) {
      messageObject.sessionId = this._sessionId;
    }
    sendDevToolsMessage(JSON.stringify(messageObject));
    return new Promise(f => this._dispatchTable.set(requestId, f));
  }

  async dispatchMessage(message) {
    try {
      const messageId = message.id;
      if (typeof messageId === 'number') {
        const handler = this._dispatchTable.get(messageId);
        if (handler) {
          this._dispatchTable.delete(messageId);
          handler(message);
        } else {
          cdpClient.reportError(`Unexpected result id ${messageId}`);
        }
      } else {
        const eventName = message.method;
        for (const handler of (this._eventHandlers.get(eventName) || [])) {
          handler(message);
        }
      }
    } catch (e) {
      cdpClient.reportError(
          `Exception when dispatching message\n' +
        '${JSON.stringify(message)}`,
          e);
    }
  }

  _getProtocol() {
    return new Proxy({}, {
      get: (target, domainName, receiver) => new Proxy({}, {
        get: (target, methodName, receiver) => {
          const eventPattern = /^(on(ce)?|off)([A-Z][A-Za-z0-9]*)/;
          const match = eventPattern.exec(methodName);
          if (!match) {
            return args => this.sendCommand(
                       `${domainName}.${methodName}`, args || {});
          }
          let eventName = match[3];
          eventName = eventName.charAt(0).toLowerCase() + eventName.slice(1);
          if (match[1] === 'once') {
            return eventMatcher => this._waitForEvent(
                       `${domainName}.${eventName}`, eventMatcher);
          }
          if (match[1] === 'off') {
            return listener => this._removeEventHandler(
                       `${domainName}.${eventName}`, listener);
          }
          return listener => this._addEventHandler(
                     `${domainName}.${eventName}`, listener);
        },
      }),
    });
  }

  _waitForEvent(eventName, eventMatcher) {
    return new Promise(callback => {
      const handler = result => {
        if (eventMatcher && !eventMatcher(result)) {
          return;
        }
        this._removeEventHandler(eventName, handler);
        callback(result);
      };
      this._addEventHandler(eventName, handler);
    });
  }

  _addEventHandler(eventName, handler) {
    const handlers = this._eventHandlers.get(eventName) || [];
    handlers.push(handler);
    this._eventHandlers.set(eventName, handlers);
  }

  _removeEventHandler(eventName, handler) {
    const handlers = this._eventHandlers.get(eventName) || [];
    const index = handlers.indexOf(handler);
    if (index === -1) {
      return;
    }
    handlers.splice(index, 1);
    this._eventHandlers.set(eventName, handlers);
  }
}

//
// TargetPage
//
class TargetPage {
  constructor(browserSession) {
    this._browserSession = browserSession;
    this._targetId = '';
    this._session;
  }

  static async createAndNavigate(browserSession, url) {
    const targetPage = new TargetPage(browserSession);

    const dp = browserSession.protocol();
    const params = {url: 'about:blank'};
    const createContextOptions = {};
    params.browserContextId =
        (await dp.Target.createBrowserContext(createContextOptions))
            .result.browserContextId;
    targetPage._targetId =
        (await dp.Target.createTarget(params)).result.targetId;

    const sessionId = (await dp.Target.attachToTarget({
                        targetId: targetPage._targetId,
                        flatten: true,
                      })).result.sessionId;
    targetPage._session = browserSession.createSession(sessionId);

    await targetPage._navigate(url);

    return targetPage;
  }

  targetId() {
    return this._targetId;
  }

  session() {
    return this._session;
  }

  async close() {
    const dp = this._session.protocol();
    dp.Target.closeTarget({targetId: this._targetId});
  }

  async _navigate(url) {
    const dp = this._session.protocol();
    await dp.Page.enable();
    await dp.Page.setLifecycleEventsEnabled({enabled: true});
    const frameId = (await dp.Page.navigate({url})).result.frameId;
    await dp.Page.onceLifecycleEvent(
        event =>
            event.params.name === 'load' && event.params.frameId === frameId);
  }
}

//
// Command handlers
//
async function dumpDOM(dp) {
  const script = '(document.doctype ? new ' +
      'XMLSerializer().serializeToString(document.doctype) + \'\\n\' : \'\')' +
      ' + document.documentElement.outerHTML';

  const response = await dp.Runtime.evaluate({expression: script});
  return response.result.result.value;
}

async function printToPDF(dp, params) {
  const displayHeaderFooter = !params.noHeaderFooter;

  const printToPDFParams = {
    displayHeaderFooter,
    printBackground: true,
    preferCSSPageSize: true,
  };

  const response = await dp.Page.printToPDF(printToPDFParams);
  return response.result.data;
}

async function screenshot(dp, params) {
  const format = params.format || 'png';
  const screenshotParams = {
    format,
  };
  const response = await dp.Page.captureScreenshot(screenshotParams);
  return response.result.data;
}

async function handleCommands(dp, commands) {
  const result = {};
  if ('dumpDom' in commands) {
    result.dumpDomResult = await dumpDOM(dp);
  }

  if ('printToPDF' in commands) {
    result.printToPdfResult = await printToPDF(dp, commands.printToPDF);
  }

  if ('screenshot' in commands) {
    result.screenshotResult = await screenshot(dp, commands.screenshot);
  }

  return result;
}

//
// Target.exposeDevToolsProtocol() communication functions.
//
window.cdp.onmessage = json => {
  // console.log('[recv] ' + json);
  cdpClient.dispatchMessage(json);
};

function sendDevToolsMessage(json) {
  // console.log('[send] ' + json);
  window.cdp.send(json);
}

//
// This is called from the host.
//
async function executeCommands(commands) {
  const browserSession = new CDPSession();
  const targetPage =
      await TargetPage.createAndNavigate(browserSession, commands.targetUrl);
  const dp = targetPage.session().protocol();

  if ('defaultBackgroundColor' in commands) {
    await dp.Emulation.setDefaultBackgroundColorOverride(
        {color: commands.defaultBackgroundColor});
  }

  const promises = [];
  if ('timeout' in commands) {
    const timeoutPromise = new Promise(resolve => {
      setTimeout(resolve, commands.timeout);
    });
    promises.push(timeoutPromise);
  }

  if ('virtualTimeBudget' in commands) {
    await dp.Emulation.setVirtualTimePolicy({
      budget: commands.virtualTimeBudget,
      maxVirtualTimeTaskStarvationCount: 9999,
      policy: 'pauseIfNetworkFetchesPending',
    });
    promises.push(dp.Emulation.onceVirtualTimeBudgetExpired());
  }

  if (promises.length > 0) {
    await Promise.race(promises);
  }

  const result = await handleCommands(dp, commands);

  await targetPage.close();

  return result;
}
