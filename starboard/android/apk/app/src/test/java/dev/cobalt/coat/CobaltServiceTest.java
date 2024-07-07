package dev.cobalt.coat;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import org.junit.Before;
import org.junit.Test;

public class CobaltServiceTest {

  CobaltService undertest;

  @Before
  public void setUp() throws Exception {
    undertest = new CobaltService() {
        @Override
        public ResponseToClient receiveFromClient(byte[] data) {
            ResponseToClient response = new ResponseToClient();
            response.data = "pong".getBytes(StandardCharsets.UTF_8);
            return response;
        }

        @Override
        public void close() {
        }

        public void beforeStartOrResume() {
        }

        public void beforeSuspend() {
        }

        public void afterStopped() {
        }
    };
  }

  @Test
  public void receiveFromClient() {
    CobaltService.ResponseToClient resp = undertest.receiveFromClient("ping".getBytes(StandardCharsets.UTF_8));
    assert (Arrays.equals(resp.data, "pong".getBytes(StandardCharsets.UTF_8)));
  }

}
