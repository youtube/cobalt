// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://intro/sign_in_promo_refresh.js';

import {IntroBrowserProxyImpl as IntroMojoBrowserProxyImpl} from 'chrome://intro/intro_browser_proxy.js';
import {SignInPromoBrowserProxyImpl} from 'chrome://intro/sign_in_promo_browser_proxy.js';
import type {SignInPromoRefreshElement} from 'chrome://intro/sign_in_promo_refresh.js';
import {Variation} from 'chrome://intro/sign_in_promo_refresh.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestIntroMojoBrowserProxy} from './test_intro_mojo_browser_proxy.js';
import {TestSignInPromoBrowserProxy} from './test_sign_in_promo_browser_proxy.js';

function assertSignInButtonsDisabled(
    element: SignInPromoRefreshElement, assertDeclineButton: boolean = true) {
  assertTrue(element.$.acceptSignInButton.disabled);
  if (assertDeclineButton) {
    assertTrue(element.$.declineSignInButton.disabled);
  }
}

function assertSignInButtonsEnabled(
    element: SignInPromoRefreshElement, assertDeclineButton: boolean = true) {
  assertFalse(element.$.acceptSignInButton.disabled);
  if (assertDeclineButton) {
    assertFalse(element.$.declineSignInButton.disabled);
  }
}

function variationToTestSuffix(variation: Variation): string {
  switch (variation) {
    case Variation.DEFAULT:
      return 'Default';
    case Variation.DONT_SIGN_IN_IN_TOP_RIGHT_CORNER:
      return 'DontSignInInTopRightCorner';
    case Variation.DONT_SIGN_IN_ON_GAIA:
      return 'DontSignInOnGaia';
    default:
      throw new Error('Unknown variation');
  }
}

suite('SignInPromoRefreshTest', function() {
  let signInPromoElement: SignInPromoRefreshElement;
  let testBrowserProxy: TestSignInPromoBrowserProxy;
  let testMojoBrowserProxy: TestIntroMojoBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testBrowserProxy = new TestSignInPromoBrowserProxy();
    testMojoBrowserProxy = new TestIntroMojoBrowserProxy();
    SignInPromoBrowserProxyImpl.setInstance(testBrowserProxy);
    IntroMojoBrowserProxyImpl.setInstance(testMojoBrowserProxy);
    loadTimeData.overrideValues({
      isFirstRunDesktopRevampEnabled: true,
    });
  });

  [Variation.DEFAULT, Variation.DONT_SIGN_IN_IN_TOP_RIGHT_CORNER,
   Variation.DONT_SIGN_IN_ON_GAIA]
      .forEach((variation) => {
        const assertDeclineButton =
            variation !== Variation.DONT_SIGN_IN_ON_GAIA;

        suite(
            'NonManagedDevice' + variationToTestSuffix(variation), function() {
              setup(function() {
                document.body.innerHTML = window.trustedTypes!.emptyHTML;

                loadTimeData.overrideValues({
                  isDeviceManaged: false,
                  signInPromoVariation: variation,
                });

                signInPromoElement =
                    document.createElement('sign-in-promo-refresh');
                document.body.appendChild(signInPromoElement);
                return microtasksFinished();
              });

              test('accept sign-in button clicked', async function() {
                assertSignInButtonsEnabled(
                    signInPromoElement, assertDeclineButton);
                assertEquals(
                    0,
                    testBrowserProxy.handler.getCallCount(
                        'continueWithAccount'));
                signInPromoElement.$.acceptSignInButton.click();
                await microtasksFinished();
                assertSignInButtonsDisabled(
                    signInPromoElement, assertDeclineButton);
                assertEquals(
                    1,
                    testBrowserProxy.handler.getCallCount(
                        'continueWithAccount'));
              });

              test('decline sign-in button clicked', async function() {
                if (!assertDeclineButton) {
                  this.skip();
                }
                assertSignInButtonsEnabled(signInPromoElement);
                assertEquals(
                    0,
                    testBrowserProxy.handler.getCallCount(
                        'continueWithoutAccount'));
                signInPromoElement.$.declineSignInButton.click();
                await microtasksFinished();
                assertSignInButtonsDisabled(signInPromoElement);
                assertEquals(
                    1,
                    testBrowserProxy.handler.getCallCount(
                        'continueWithoutAccount'));
              });

              test(
                  '"reset-intro-buttons" event resets buttons',
                  async function() {
                    assertSignInButtonsEnabled(
                        signInPromoElement, assertDeclineButton);
                    signInPromoElement.$.acceptSignInButton.click();
                    await microtasksFinished();
                    assertSignInButtonsDisabled(
                        signInPromoElement, assertDeclineButton);
                    testBrowserProxy.page.onResetButtons();
                    await microtasksFinished();
                    assertSignInButtonsEnabled(
                        signInPromoElement, assertDeclineButton);
                  });
            });
      });

  [Variation.DEFAULT, Variation.DONT_SIGN_IN_IN_TOP_RIGHT_CORNER,
   Variation.DONT_SIGN_IN_ON_GAIA]
      .forEach((variation) => {
        const assertDeclineButton =
            variation !== Variation.DONT_SIGN_IN_ON_GAIA;

        suite('ManagedDevice' + variationToTestSuffix(variation), function() {
          setup(function() {
            document.body.innerHTML = window.trustedTypes!.emptyHTML;

            loadTimeData.overrideValues({
              isDeviceManaged: true,
              signInPromoVariation: variation,
            });

            signInPromoElement =
                document.createElement('sign-in-promo-refresh');
            document.body.appendChild(signInPromoElement);
            return microtasksFinished();
          });

          test('buttons are disabled if disclaimer is empty', async function() {
            assertSignInButtonsDisabled(
                signInPromoElement, assertDeclineButton);
            assertEquals(
                '', signInPromoElement.$.disclaimerText.textContent.trim());

            testBrowserProxy.resolveDisclaimer('managedDeviceDisclaimer');
            await microtasksFinished();
            assertEquals(
                'managedDeviceDisclaimer',
                signInPromoElement.$.disclaimerText.textContent.trim());
            assertSignInButtonsEnabled(signInPromoElement, assertDeclineButton);
          });
        });
      });

  test('default promo variation', async function() {
    loadTimeData.overrideValues({
      isDeviceManaged: false,
      signInPromoVariation: Variation.DEFAULT,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    signInPromoElement = document.createElement('sign-in-promo-refresh');
    document.body.appendChild(signInPromoElement);
    await microtasksFinished();

    const createAccountDisclaimer = signInPromoElement.shadowRoot.querySelector(
        '#create-account-disclaimer');
    assertFalse(!!createAccountDisclaimer);

    const topRightCornerContainer = signInPromoElement.shadowRoot.querySelector(
        '#top-right-corner-container');
    assertFalse(!!topRightCornerContainer);

    const buttonContainer =
        signInPromoElement.shadowRoot.querySelector('#buttonContainer');
    assertTrue(!!buttonContainer);
    assertEquals(
        signInPromoElement.$.declineSignInButton,
        buttonContainer.querySelector('#declineSignInButton'));
  });

  test('don\'t sign in in top right corner promo variation', async function() {
    loadTimeData.overrideValues({
      isDeviceManaged: false,
      signInPromoVariation: Variation.DONT_SIGN_IN_IN_TOP_RIGHT_CORNER,
      isFirstRunDesktopRevampEnabled: false,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    signInPromoElement = document.createElement('sign-in-promo-refresh');
    document.body.appendChild(signInPromoElement);
    await microtasksFinished();

    const createAccountDisclaimer =
        signInPromoElement.shadowRoot.querySelector(
            '#create-account-disclaimer');
    assertTrue(!!createAccountDisclaimer);

    const topRightCornerContainer =
        signInPromoElement.shadowRoot.querySelector(
            '#top-right-corner-container');
    assertTrue(!!topRightCornerContainer);
    assertFalse(topRightCornerContainer.classList.contains(
        'has-effects-control-button'));
    assertEquals(
        signInPromoElement.$.declineSignInButton,
        topRightCornerContainer.querySelector('#declineSignInButton'));
    assertTrue(signInPromoElement.$.declineSignInButton.classList.contains(
        'tangible-button'));

    const separator = topRightCornerContainer.querySelector('#separator');
    assertFalse(!!separator);

    const buttonContainer =
        signInPromoElement.shadowRoot.querySelector('#buttonContainer');
    assertTrue(!!buttonContainer);
    const declineSignInButtonInButtonContainer =
        buttonContainer.querySelector('#declineSignInButton');
    assertFalse(!!declineSignInButtonInButtonContainer);
  });

  test(
      'don\'t sign in in top right corner promo variation with revamp',
      async function() {
        loadTimeData.overrideValues({
          isDeviceManaged: false,
          signInPromoVariation: Variation.DONT_SIGN_IN_IN_TOP_RIGHT_CORNER,
          isFirstRunDesktopRevampEnabled: true,
        });
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        signInPromoElement = document.createElement('sign-in-promo-refresh');
        document.body.appendChild(signInPromoElement);
        await microtasksFinished();

        const createAccountDisclaimer =
            signInPromoElement.shadowRoot.querySelector(
                '#create-account-disclaimer');
        assertTrue(!!createAccountDisclaimer);

        const topRightCornerContainer =
            signInPromoElement.shadowRoot.querySelector(
                '#top-right-corner-container');
        assertTrue(!!topRightCornerContainer);
        assertTrue(topRightCornerContainer.classList.contains(
            'has-effects-control-button'));
        assertEquals(
            signInPromoElement.$.declineSignInButton,
            topRightCornerContainer.querySelector('#declineSignInButton'));
        assertFalse(signInPromoElement.$.declineSignInButton.classList.contains(
            'tangible-button'));
        assertTrue(signInPromoElement.$.declineSignInButton.classList.contains(
            'no-border'));

        const separator = topRightCornerContainer.querySelector('#separator');
        assertTrue(!!separator);

        const buttonContainer =
            signInPromoElement.shadowRoot.querySelector('#buttonContainer');
        assertTrue(!!buttonContainer);
        const declineSignInButtonInButtonContainer =
            buttonContainer.querySelector('#declineSignInButton');
        assertFalse(!!declineSignInButtonInButtonContainer);
      });

  test('don\'t sign in on Gaia page promo variation', async function() {
    loadTimeData.overrideValues({
      isDeviceManaged: false,
      signInPromoVariation: Variation.DONT_SIGN_IN_ON_GAIA,
    });
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    signInPromoElement = document.createElement('sign-in-promo-refresh');
    document.body.appendChild(signInPromoElement);
    await microtasksFinished();

    const createAccountDisclaimer = signInPromoElement.shadowRoot.querySelector(
        '#create-account-disclaimer');
    assertFalse(!!createAccountDisclaimer);

    const declineSignInButton =
        signInPromoElement.shadowRoot.querySelector('#declineSignInButton');
    assertFalse(!!declineSignInButton);
  });

  test('change animation file depending on the theme', async function() {
    loadTimeData.overrideValues({
      isDeviceManaged: false,
      signInPromoVariation: Variation.DEFAULT,
    });

    testBrowserProxy.setMatchMediaMatches(false);

    signInPromoElement = document.createElement('sign-in-promo-refresh');
    document.body.appendChild(signInPromoElement);
    await microtasksFinished();

    const leftAnimation = signInPromoElement.$.leftAnimation;
    const rightAnimation = signInPromoElement.$.rightAnimation;
    const bottomAnimation = signInPromoElement.$.bottomAnimation;

    assertTrue(!!leftAnimation);
    assertTrue(leftAnimation.animationUrl.includes('light'));
    assertTrue(!!rightAnimation);
    assertTrue(rightAnimation.animationUrl.includes('light'));
    assertTrue(!!bottomAnimation);
    assertTrue(bottomAnimation.animationUrl.includes('light'));

    testBrowserProxy.setMatchMediaMatches(true);
    await microtasksFinished();

    assertTrue(leftAnimation.animationUrl.includes('dark'));
    assertTrue(rightAnimation.animationUrl.includes('dark'));
    assertTrue(bottomAnimation.animationUrl.includes('dark'));
  });

  test(
      'change animation file depending on the theme with revamp disabled',
      async function() {
        loadTimeData.overrideValues({
          isDeviceManaged: false,
          signInPromoVariation: Variation.DEFAULT,
          isFirstRunDesktopRevampEnabled: false,
        });

        testBrowserProxy.setMatchMediaMatches(false);

        signInPromoElement = document.createElement('sign-in-promo-refresh');
        document.body.appendChild(signInPromoElement);
        await microtasksFinished();

        const leftAnimation = signInPromoElement.$.leftAnimation;
        const rightAnimation = signInPromoElement.$.rightAnimation;
        const bottomAnimation = signInPromoElement.$.bottomAnimation;

        assertTrue(!!leftAnimation);
        assertTrue(leftAnimation.animationUrl.includes('light_left_static'));
        assertTrue(!!rightAnimation);
        assertTrue(rightAnimation.animationUrl.includes('light_right_static'));
        assertTrue(!!bottomAnimation);
        assertTrue(
            bottomAnimation.animationUrl.includes('light_bottom_static'));

        testBrowserProxy.setMatchMediaMatches(true);
        await microtasksFinished();

        assertTrue(leftAnimation.animationUrl.includes('dark_left'));
        assertFalse(leftAnimation.animationUrl.includes('static'));
        assertTrue(rightAnimation.animationUrl.includes('dark_right'));
        assertFalse(rightAnimation.animationUrl.includes('static'));
        assertTrue(bottomAnimation.animationUrl.includes('dark_bottom'));
        assertFalse(bottomAnimation.animationUrl.includes('static'));
      });

  test('toggles animations', async function() {
    loadTimeData.overrideValues({
      isDeviceManaged: false,
      signInPromoVariation: Variation.DEFAULT,
      disableAnimations: false,
    });

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    signInPromoElement = document.createElement('sign-in-promo-refresh');
    document.body.appendChild(signInPromoElement);
    await microtasksFinished();

    const leftAnimation = signInPromoElement.$.leftAnimation;
    const rightAnimation = signInPromoElement.$.rightAnimation;
    const bottomAnimation = signInPromoElement.$.bottomAnimation;

    let leftPlay: boolean|null = null;
    leftAnimation.setPlay = (play: boolean) => {
      leftPlay = play;
    };
    let rightPlay: boolean|null = null;
    rightAnimation.setPlay = (play: boolean) => {
      rightPlay = play;
    };
    let bottomPlay: boolean|null = null;
    bottomAnimation.setPlay = (play: boolean) => {
      bottomPlay = play;
    };

    const pageRemote =
        testMojoBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();
    pageRemote.toggleAnimations(false);
    await pageRemote.$.flushForTesting();
    await microtasksFinished();

    assertEquals(false, leftPlay);
    assertEquals(false, rightPlay);
    assertEquals(false, bottomPlay);

    pageRemote.toggleAnimations(true);
    await pageRemote.$.flushForTesting();
    await microtasksFinished();

    assertEquals(true, leftPlay);
    assertEquals(true, rightPlay);
    assertEquals(true, bottomPlay);
  });

  test(
      'does not toggle animations when `disableAnimations` is true',
      async function() {
        loadTimeData.overrideValues({
          isDeviceManaged: false,
          signInPromoVariation: Variation.DEFAULT,
          disableAnimations: true,
        });

        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        signInPromoElement = document.createElement('sign-in-promo-refresh');
        document.body.appendChild(signInPromoElement);
        await microtasksFinished();

        const leftAnimation = signInPromoElement.$.leftAnimation;

        let leftPlay: boolean|null = null;
        leftAnimation.setPlay = (play: boolean) => {
          leftPlay = play;
        };

        const pageRemote =
            testMojoBrowserProxy.callbackRouter.$.bindNewPipeAndPassRemote();
        pageRemote.toggleAnimations(false);
        await pageRemote.$.flushForTesting();
        await microtasksFinished();

        // Verify that setPlay was NOT called because animations are disabled.
        assertEquals(null, leftPlay);
      });
});
