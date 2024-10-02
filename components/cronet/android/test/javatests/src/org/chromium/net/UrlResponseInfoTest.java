// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.net.impl.UrlResponseInfoImpl;

import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Tests for {@link UrlResponseInfo}.
 */
@RunWith(AndroidJUnit4.class)
public class UrlResponseInfoTest {
    /**
     * Test for public API of {@link UrlResponseInfo}.
     */
    @Test
    @SmallTest
    public void testPublicAPI() throws Exception {
        final List<String> urlChain = new ArrayList<String>();
        urlChain.add("chromium.org");
        final int httpStatusCode = 200;
        final String httpStatusText = "OK";
        final List<Map.Entry<String, String>> allHeadersList =
                new ArrayList<Map.Entry<String, String>>();
        allHeadersList.add(new AbstractMap.SimpleImmutableEntry<String, String>(
                "Date", "Fri, 30 Oct 2015 14:26:41 GMT"));
        final boolean wasCached = true;
        final String negotiatedProtocol = "quic/1+spdy/3";
        final String proxyServer = "example.com";
        final long receivedByteCount = 42;

        final UrlResponseInfo info =
                new UrlResponseInfoImpl(urlChain, httpStatusCode, httpStatusText, allHeadersList,
                        wasCached, negotiatedProtocol, proxyServer, receivedByteCount);
        Assert.assertEquals(info.getUrlChain(), urlChain);
        Assert.assertEquals(info.getUrl(), urlChain.get(urlChain.size() - 1));
        try {
            info.getUrlChain().add("example.com");
            Assert.fail("getUrlChain() returned modifyable list.");
        } catch (UnsupportedOperationException e) {
            // Expected.
        }
        Assert.assertEquals(info.getHttpStatusCode(), httpStatusCode);
        Assert.assertEquals(info.getHttpStatusText(), httpStatusText);
        Assert.assertEquals(info.getAllHeadersAsList(), allHeadersList);
        try {
            info.getAllHeadersAsList().add(
                    new AbstractMap.SimpleImmutableEntry<String, String>("X", "Y"));
            Assert.fail("getAllHeadersAsList() returned modifyable list.");
        } catch (UnsupportedOperationException e) {
            // Expected.
        }
        Assert.assertEquals(info.getAllHeaders().size(), allHeadersList.size());
        Assert.assertEquals(info.getAllHeaders().get(allHeadersList.get(0).getKey()).size(), 1);
        Assert.assertEquals(info.getAllHeaders().get(allHeadersList.get(0).getKey()).get(0),
                allHeadersList.get(0).getValue());
        Assert.assertEquals(info.wasCached(), wasCached);
        Assert.assertEquals(info.getNegotiatedProtocol(), negotiatedProtocol);
        Assert.assertEquals(info.getProxyServer(), proxyServer);
        Assert.assertEquals(info.getReceivedByteCount(), receivedByteCount);
    }
}