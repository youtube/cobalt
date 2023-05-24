package dev.cobalt.account;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

@RunWith(BlockJUnit4ClassRunner.class)
public class AccessTokenTest {

  @Test
  public void getTokenValue() {
    String token = "token";
    AccessToken undertest = new AccessToken(token, 0);
    assertThat(undertest.getTokenValue()).isEqualTo(token);
  }

  @Test
  public void getExpirySeconds() {
    long seconds = 1000;
    AccessToken undertest = new AccessToken("", seconds);
    assertThat(undertest.getExpirySeconds()).isEqualTo(seconds);
  }
}
