package dev.cobalt.account;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

@RunWith(BlockJUnit4ClassRunner.class)
public class NoopUserAuthorizerTest {
  NoopUserAuthorizer undertest;

  @Before
  public void setUp() throws Exception {
    undertest = new NoopUserAuthorizer();
  }

  @Test
  public void authorizeUser() {
    assertThat(undertest.authorizeUser()).isEqualTo(null);
  }

  @Test
  public void deauthorizeUser() {
    assertThat(undertest.deauthorizeUser()).isEqualTo(false);
  }

  @Test
  public void refreshAuthorization() {
    assertThat(undertest.refreshAuthorization()).isEqualTo(null);
  }
}
