// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   name: string,
 *   value: string
 * }}
 */
var Metric;

/**
 * @typedef {{
 *   name: string,
 *   metrics: !Array<!Metric>
 * }}
 */
var UkmEntry;

/**
 * @typedef {{
 *   url: string,
 *   id: string,
 *   entries: !Array<UkmEntry>,
 * }}
 */
var UkmDataSource;

/**
 * The Ukm data sent from the browser.
 * @typedef {{
 *   state: boolean,
 *   client_id: string,
 *   session_id: string,
 *   sources: !Array<!UkmDataSource>,
 * }}
 */
var UkmData;

/**
 * Fetches data from the Ukm service and updates the DOM to display it as a
 * list.
 */
function updateUkmData() {
  cr.sendWithPromise('requestUkmData').then((/** @type {UkmData} */ data) => {
    $('state').innerText = data.state ? 'True' : 'False';
    $('clientid').innerText = data.client_id;
    $('sessionid').innerText = data.session_id;

    let sourceDiv = $('sources');
    for (const source of data.sources) {
      const sourceElement = document.createElement('h3');
      if (source.url !== undefined)
        sourceElement.innerText = `Id: ${source.id} Url: ${source.url}`;
      else
        sourceElement.innerText = `Id: ${source.id}`;
      sourceDiv.appendChild(sourceElement);

      for (const entry of source.entries) {
        const entryElement = document.createElement('h4');
        entryElement.innerText = `Entry: ${entry.name}`;
        sourceDiv.appendChild(entryElement);

        if (entry.metrics === undefined)
          continue;
        for (const metric of entry.metrics) {
          const metricElement = document.createElement('h5');
          metricElement.innerText =
              `Metric: ${metric.name} Value: ${metric.value}`;
          sourceDiv.appendChild(metricElement);
        }
      }
    }
  });
}

document.addEventListener('DOMContentLoaded', updateUkmData);
