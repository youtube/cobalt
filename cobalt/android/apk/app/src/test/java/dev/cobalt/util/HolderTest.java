package dev.cobalt.util;

import static com.google.common.truth.Truth.assertThat;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

/** HolderTest. */
@RunWith(BlockJUnit4ClassRunner.class)
public class HolderTest {
  Holder mUndertest = new Holder();

  @Test
  public void test() {
    Object toHold = new Object();
    mUndertest.set(toHold);
    assertThat(mUndertest.get()).isEqualTo(toHold);
  }
}
