// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert} from '//resources/js/assert_ts.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {ImageQuery, MethodType, PromoAction, PromoType} from './companion.mojom-webui.js';
import {CompanionProxy, CompanionProxyImpl} from './companion_proxy.js';

/**
 * Method arguments to be passed as part of the JSON message object to be sent
 * across the postmessage boundary.
 * Keep this file in sync with
 * google3/java/com/google/lens/web/interfaces/standalone/companionweb/service/companion_parent_communication_service.ts
 */
enum ParamType {
  // Arguments for iframe -> browser communication.
  // Mandatory arguments.
  METHOD_TYPE = 'type',

  // Arguments for MethodType.kOnCqCandidatesAvailable.
  CQ_TEXT_DIRECTIVES = 'cqTextDirectives',

  // Optional arguments.
  // Arguments for MethodType.kOnExpsOptInStatusAvailable.
  IS_EXPS_OPTED_IN = 'isExpsOptedIn',

  // Arguments for MethodType.kOnPromoAction.
  PROMO_ACTION = 'promoAction',
  PROMO_TYPE = 'promoType',

  // Arguments for MethodType.kOnPhAction.
  PH_ACTION = 'phAction',

  // Arguments for MethodType.kOnOpenInNewTabButtonURLChanged.
  URL_FOR_OPEN_IN_NEW_TAB = 'urlForOpenInNewTab',

  // Arguments for MethodType.kRecordUiSurfaceShown.
  UI_SURFACE = 'ui_surface',

  // Arguments for MethodType.kRecordUiSurfaceShown.
  CHILD_ELEMENT_COUNT = 'child_element_count',

  // Arguments for browser -> iframe communication.
  COMPANION_UPDATE_PARAMS = 'companion_update_params',

  // Arguments for sending text find results from browser to iframe.
  CQ_TEXT_FIND_RESULTS = 'cqTextFindResults',
}

const companionProxy: CompanionProxy = CompanionProxyImpl.getInstance();

// Validation check for incoming enums from the iframe postMessage().
function validatePromoArguments(promoType: any, promoAction: any): boolean {
  const isValidType = Object.values(PromoType).includes(promoType);
  const isValidAction = Object.values(PromoAction).includes(promoAction);
  return isValidType && isValidAction;
}

function initialize() {
  // For the initial navigation, we update our iframe src to pass new
  // URL.
  companionProxy.callbackRouter.loadCompanionPage.addListener((newUrl: Url) => {
    const frame = document.body.querySelector('iframe');
    assert(frame);
    frame.src = newUrl.url;
  });

  // For subsequent navigations, we send a post message.
  companionProxy.callbackRouter.updateCompanionPage.addListener(
      (companionUpdateProto: string) => {
        const companionOrigin =
            new URL(loadTimeData.getString('companion_origin')).origin;
        const message =
            {[ParamType.COMPANION_UPDATE_PARAMS]: companionUpdateProto};

        const frame = document.body.querySelector('iframe');
        assert(frame);
        if (frame.contentWindow) {
          frame.contentWindow.postMessage(message, companionOrigin);
        }
      });

  // On image queries, we need to send a POST to the iframe using a form in the
  // WebUI.
  companionProxy.callbackRouter.onImageQuery.addListener(
      (imageQuery: ImageQuery) => {
        const queryForm = document.body.querySelector('form');
        const imageDataInput =
            document.getElementById('image-data') as HTMLInputElement;
        const imageUrlInput =
            document.getElementById('image-src-url') as HTMLInputElement;
        const widthInput =
            document.getElementById('image-width') as HTMLInputElement;
        const heightInput =
            document.getElementById('image-height') as HTMLInputElement;
        const downscaledDimensionsInput =
            document.getElementById('image-downscaled-dimensions') as
            HTMLInputElement;
        assert(queryForm);
        assert(imageDataInput);
        assert(imageUrlInput);
        assert(widthInput);
        assert(heightInput);
        assert(downscaledDimensionsInput);
        queryForm.setAttribute('action', imageQuery.uploadUrl.url);
        // The original Uint8Array that gets passed does not have an array
        // buffer due to how it is initialized. Thus, we have to create a
        // Uint8Array with the same data as |imageBytes| in order to properly
        // create a blob from it.
        const imageBytesWithBuffer = new Uint8Array(imageQuery.imageBytes);
        const blob =
            new Blob([imageBytesWithBuffer], {type: imageQuery.contentType});
        const file =
            new File([blob], 'filename.jpg', {type: imageQuery.contentType});

        // Create a DataTransfer to create a file list for the images we want to
        // query.
        const container = new DataTransfer();
        container.items.add(file);

        // Assign all values on the form and submit to initiate request.
        imageDataInput.files = container.files;
        imageUrlInput.value = imageQuery.imageUrl.url;
        widthInput.value = String(imageQuery.width);
        heightInput.value = String(imageQuery.height);
        downscaledDimensionsInput.value =
            `${imageQuery.downscaledWidth},${imageQuery.downscaledHeight}`;
        queryForm.submit();
        queryForm.reset();
      });

  companionProxy.callbackRouter.onCqFindTextResultsAvailable.addListener(
      (textDirectives: string[], results: boolean[]) => {
        const companionOrigin =
            new URL(loadTimeData.getString('companion_origin')).origin;
        const message = {
          [ParamType.CQ_TEXT_DIRECTIVES]: textDirectives,
          [ParamType.CQ_TEXT_FIND_RESULTS]: results,
        };

        const frame = document.body.querySelector('iframe');
        assert(frame);
        if (frame.contentWindow) {
          frame.contentWindow.postMessage(message, companionOrigin);
        }
      });

  companionProxy.handler.showUI();
}

// Handler for postMessage() calls from the embedded iframe.
function onCompanionMessageEvent(event: MessageEvent) {
  // Because the |companion_origin| string has a trailing slash that can cause
  // failures when doing a string comparison, convert the string to a URL and
  // compare the origin to prevent failures when origins are the same but
  // strings differ.
  const validOrigin =
      new URL(loadTimeData.getString('companion_origin')).origin;
  if (validOrigin !== event.origin) {
    return;
  }

  const data = event.data;
  const methodType = data[ParamType.METHOD_TYPE];
  if (methodType === MethodType.kOnRegionSearchClicked) {
    companionProxy.handler.onRegionSearchClicked();
  } else if (methodType === MethodType.kOnPromoAction) {
    const promoType = data[ParamType.PROMO_TYPE];
    const promoAction = data[ParamType.PROMO_ACTION];
    if (validatePromoArguments(promoType, promoAction)) {
      companionProxy.handler.onPromoAction(promoType, promoAction);
    }
  } else if (methodType === MethodType.kOnExpsOptInStatusAvailable) {
    companionProxy.handler.onExpsOptInStatusAvailable(
        data[ParamType.IS_EXPS_OPTED_IN]);
  } else if (methodType === MethodType.kOnOpenInNewTabButtonURLChanged) {
    const openInNewTabUrl = new Url();
    openInNewTabUrl.url = data[ParamType.URL_FOR_OPEN_IN_NEW_TAB];
    companionProxy.handler.onOpenInNewTabButtonURLChanged(openInNewTabUrl);
  } else if (methodType === MethodType.kRecordUiSurfaceShown) {
    companionProxy.handler.recordUiSurfaceShown(
        data[ParamType.UI_SURFACE], data[ParamType.CHILD_ELEMENT_COUNT]);
  } else if (methodType === MethodType.kRecordUiSurfaceClicked) {
    companionProxy.handler.recordUiSurfaceClicked(data[ParamType.UI_SURFACE]);
  } else if (methodType === MethodType.kOnCqCandidatesAvailable) {
    companionProxy.handler.onCqCandidatesAvailable(
        data[ParamType.CQ_TEXT_DIRECTIVES]);
  }
}

window.addEventListener('message', onCompanionMessageEvent, false);
document.addEventListener('DOMContentLoaded', initialize);
