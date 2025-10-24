(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {

  var {page, session, dp} = await testRunner.startBlank(
      `Tests that cookies are set, updated and removed.`);

  async function logCookies(opt_data) {
    var data = opt_data || (await dp.Network.getAllCookies()).result;
    testRunner.log('Num of cookies ' + data.cookies.length);
    data.cookies.sort((a, b) => a.name.localeCompare(b.name));
    for (var cookie of data.cookies) {
      var suffix = ''
      if (cookie.partitionKeyOpaque)
        suffix += `, partitionKey: <opaque>`;
      else if (cookie.partitionKey)
      suffix += `, partitionKey: ${JSON.stringify(cookie.partitionKey)}`;
      if (cookie.secure)
        suffix += `, secure`;
      if (cookie.httpOnly)
        suffix += `, httpOnly`;
      if (cookie.session)
        suffix += `, session`;
      if (cookie.sameSite)
        suffix += `, ${cookie.sameSite}`;
      if (cookie.expires !== -1)
        suffix += `, expires`;
      testRunner.log(`name: ${cookie.name}, value: ${cookie.value}, domain: ${cookie.domain}, path: ${cookie.path}${suffix}`);
    }
  }

  async function setCookie(cookie) {
    testRunner.log('Setting Cookie');
    var response = await dp.Network.setCookie(cookie);
    if (response.error)
      testRunner.log(`setCookie failed: ${response.error.message}`);
    await logCookies();
  }

  async function deleteCookie(cookie) {
    testRunner.log('Deleting Cookie');
    await dp.Network.deleteCookies(cookie);
    await logCookies();
  }

  async function setCookies(cookies) {
    testRunner.log('Adding multiple cookies');
    var response = await dp.Network.setCookies({cookies});
    await logCookies();
  }

  async function deleteAllCookies() {
    var data = (await dp.Network.getAllCookies()).result;
    var promises = [];
    for (var cookie of data.cookies) {
      var url = (cookie.secure ? 'https://' : 'http://') + cookie.domain + cookie.path;
      promises.push(dp.Network.deleteCookies(cookie));
    }
    await Promise.all(promises);
  }

  async function setCookieViaFetch() {
    await dp.Runtime.evaluate({
        expression: `fetch('/inspector-protocol/network/resources/cookie.pl', { credentials: 'same-origin' })`,
        awaitPromise: true
    });
    await logCookies();
  }

  async function printCookieViaFetch() {
    await dp.Network.setCookie({url: 'http://127.0.0.1/', name: 'foo', value: 'bar1'});
    const body = (await dp.Runtime.evaluate({
        expression: `
            fetch('/inspector-protocol/network/resources/echo-headers.php?headers=HTTP_COOKIE',
                { credentials: 'same-origin' })
            .then(r => r.text())`,
        awaitPromise: true,
        returnByValue: true
    })).result.result.value;
    testRunner.log(`Cookies as seen on server: ${JSON.stringify(body)}`);
  }

  class Barrier {
    constructor(nExpectedEvents) {
      ({promise: this.promise, resolve: this.resolve} = Promise.withResolvers());
      this.count = 0;
      this.nExpectedEvents = nExpectedEvents;
    }

    onEvent() {
      this.count++;
      if (this.count == this.nExpectedEvents) {
        this.resolve();
      }
    }
  }

  function listenerWithBarrier(nExpectedEvents, eventListenerCb) {
    const barrier = new Barrier(nExpectedEvents);
    return {
      promise: barrier.promise,
      listener(event) {
        eventListenerCb(event);
        barrier.onEvent();
      },
    };
  }

  function listenForSiteHasCookieInOtherPartition(event) {
    testRunner.log(
      'Site has cookie in other partition: '
          + event.params.siteHasCookieInOtherPartition);
  }

  function listenForResponsePartitionKey(event) {
    const partitionKey = event.params.cookiePartitionKeyOpaque ?
      '<opaque>' : event.params.cookiePartitionKey;
    testRunner.log(
        'Current cookie partition key: ' + JSON.stringify(partitionKey));
  }

  async function getPartitionedCookies() {
    const reqBarrier = listenerWithBarrier(2, listenForSiteHasCookieInOtherPartition);
    const respBarrier = listenerWithBarrier(2, listenForResponsePartitionKey);
    dp.Network.onRequestWillBeSentExtraInfo(reqBarrier.listener);
    dp.Network.onResponseReceivedExtraInfo(respBarrier.listener);

    // This will set a partitioned cookie
    await page.navigate('https://devtools.test:8443/inspector-protocol/resources/iframe-third-party-cookie-parent.php');

    await Promise.all([reqBarrier.promise, respBarrier.promise]);
    dp.Network.offRequestWillBeSentExtraInfo(reqBarrier.listener);
    dp.Network.offResponseReceivedExtraInfo(respBarrier.listener);

    await logCookies();
  }

  testRunner.log('Test started');
  testRunner.log('Enabling network');
  await dp.Network.enable();

  await testRunner.runTestSuite([
    deleteAllCookies,

    async function nonUnicodeCookie() {
      await setCookies([{name: 'cookie1', value: 'привет', domain: '.chromium.org', path: '/path' }]);
    },

    deleteAllCookies,

    setCookieViaFetch,

    deleteAllCookies,

    printCookieViaFetch,

    deleteAllCookies,

    getPartitionedCookies,

    async function setPartitionedCookie() {
      await setCookie({
        url: 'https://devtools.test:8443',
        secure: true,
        name: '__Host-foo',
        value: 'bar',
        partitionKey: {
          topLevelSite: 'https://devtools.test:8443',
          hasCrossSiteAncestor: false
        },
        sameSite: 'None'
      });
      await setCookie({
        url: 'https://example.test:8443',
        secure: true,
        name: '__Host-foo',
        value: 'bar',
        partitionKey: {
          topLevelSite: 'https://devtools.test:8443',
          hasCrossSiteAncestor: true
        },
        sameSite: 'None'
      });
      await setCookie({
        url: 'https://example.test:8443',
        secure: true,
        name: '__Host-foo',
        value: 'bar',
        partitionKey: {
          topLevelSite: 'https://notinset.test:8443',
          hasCrossSiteAncestor: true
        },
        sameSite: 'None'
      });
    },

    deleteAllCookies,
    logCookies,

    async function partitionedAndUnpartitionedCookiesWithSameName() {
      await setCookie({url: 'https://devtools.test:8443', secure: true, name: '__Host-foo', value: 'bar', sameSite: 'None'});
      await setCookie({
        url: 'https://devtools.test:8443',
        secure: true,
        name: '__Host-foo',
        value: 'bar',
        partitionKey:
            {topLevelSite: 'https://example.test', hasCrossSiteAncestor: false},
        sameSite: 'None'
      });
      await setCookie({
        url: 'https://devtools.test:8443',


        secure: true,
        name: '__Host-foo',
        value: 'bar',
        partitionKey: {
          topLevelSite: 'https://notinset.test',
          hasCrossSiteAncestor: false
        },

        sameSite: 'None'
      });

      await deleteCookie({url: 'https://devtools.test:8443', name: '__Host-foo'});
      await deleteCookie({
        url: 'https://devtools.test:8443',
        name: '__Host-foo',
        partitionKey:
            {topLevelSite: 'https://example.test', hasCrossSiteAncestor: false},

      });
    },

    deleteAllCookies,
    logCookies,

    async function setPartitionedCookies() {
      await setCookies([
        {
          url: 'https://devtools.test:8443',
          secure: true,
          name: '__Host-foo',
          value: 'bar',
          partitionKey: {
            topLevelSite: 'https://example.test:8443',
            hasCrossSiteAncestor: false
          },

          sameSite: 'None'
        },
        {
          url: 'https://example.test:8443',
          secure: true,
          name: '__Host-foo',
          value: 'bar',
          partitionKey: {
            topLevelSite: 'https://devtools.test:8443',
            hasCrossSiteAncestor: false
          },

          sameSite: 'None'
        },
        {
          url: 'https://example.test:8443',
          secure: true,
          name: '__Host-foo',
          value: 'bar',
          partitionKey: {
            topLevelSite: 'https://notinset.test:8443',
            hasCrossSiteAncestor: false
          },

          sameSite: 'None'
        }
      ]);
    },

    getPartitionedCookies,
    deleteAllCookies,

    async function getPartitionedCookieFromOpaqueOrigin() {
      const reqBarrier = listenerWithBarrier(2, listenForSiteHasCookieInOtherPartition);
      const respBarrier = listenerWithBarrier(2, listenForResponsePartitionKey);
      dp.Network.onRequestWillBeSentExtraInfo(reqBarrier.listener);
      dp.Network.onResponseReceivedExtraInfo(respBarrier.listener);

      await page.navigate('https://devtools.test:8443/inspector-protocol/resources/iframe-third-party-cookie-parent.php?opaque');
      await logCookies((await dp.Network.getCookies()).result);

      await Promise.all([reqBarrier.promise, respBarrier.promise]);
      dp.Network.offRequestWillBeSentExtraInfo(reqBarrier.listener);
      dp.Network.offResponseReceivedExtraInfo(respBarrier.listener);
    },

    deleteAllCookies,
  ]);
})
