// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package dev.cobalt.testing;

import static com.google.common.truth.Truth.assertThat;

import com.sun.net.httpserver.HttpServer;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicReference;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Unit tests for {@link WidevineProvisioner}. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WidevineProvisionerTest {

  @Test
  public void buildProvisionUrl_usesProperQuerySeparator() {
    byte[] data = "payload".getBytes(StandardCharsets.UTF_8);
    assertThat(WidevineProvisioner.buildProvisionUrl("https://example.com/create?key=1", data))
        .isEqualTo("https://example.com/create?key=1&signedRequest=payload");
    assertThat(WidevineProvisioner.buildProvisionUrl("https://example.com/create", data))
        .isEqualTo("https://example.com/create?signedRequest=payload");
  }

  @Test
  public void performHttpPost_returnsResponseBodyOn200() throws Exception {
    HttpServer server = HttpServer.create(new InetSocketAddress("127.0.0.1", 0), 0);
    AtomicReference<String> method = new AtomicReference<>();
    byte[] expected = "provisioned-cert".getBytes(StandardCharsets.UTF_8);

    server.createContext(
        "/provision",
        exchange -> {
          method.set(exchange.getRequestMethod());
          exchange.sendResponseHeaders(200, expected.length);
          try (OutputStream os = exchange.getResponseBody()) {
            os.write(expected);
          }
          exchange.close();
        });
    server.start();
    try {
      String url =
          "http://127.0.0.1:" + server.getAddress().getPort() + "/provision?signedRequest=abc";
      assertThat(WidevineProvisioner.performHttpPost(url, 2000)).isEqualTo(expected);
      assertThat(method.get()).isEqualTo("POST");
    } finally {
      server.stop(0);
    }
  }

  @Test
  public void performHttpPost_returnsNullOnHttpError() throws Exception {
    HttpServer server = HttpServer.create(new InetSocketAddress("127.0.0.1", 0), 0);
    server.createContext(
        "/provision",
        exchange -> {
          exchange.sendResponseHeaders(400, -1);
          exchange.close();
        });
    server.start();
    try {
      String url = "http://127.0.0.1:" + server.getAddress().getPort() + "/provision";
      assertThat(WidevineProvisioner.performHttpPost(url, 2000)).isNull();
    } finally {
      server.stop(0);
    }
  }
}
