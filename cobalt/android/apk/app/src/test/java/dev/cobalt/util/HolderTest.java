package dev.cobalt.util;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

/** HolderTest. */
@RunWith(BlockJUnit4ClassRunner.class)
public class HolderTest {
  Holder undertest = new Holder();

  @Test
  public void test() {
    Object toHold = new Object();
    undertest.set(toHold);
    assertThat(undertest.get()).isEqualTo(toHold);
  }
}
