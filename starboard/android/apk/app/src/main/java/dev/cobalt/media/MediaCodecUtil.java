// Copyright 2017 Google Inc. All Rights Reserved.
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

package dev.cobalt.media;

import static dev.cobalt.media.Log.TAG;

import android.media.MediaCodecInfo;
import android.media.MediaCodecInfo.AudioCapabilities;
import android.media.MediaCodecInfo.CodecCapabilities;
import android.media.MediaCodecInfo.CodecProfileLevel;
import android.media.MediaCodecInfo.VideoCapabilities;
import android.media.MediaCodecList;
import android.util.Log;
import dev.cobalt.util.IsEmulator;
import dev.cobalt.util.UsedByNative;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Utility functions for dealing with MediaCodec related things. */
public class MediaCodecUtil {
  // A low priority black list of codec names that should never be used.
  private static final Set<String> codecBlackList = new HashSet<>();
  // A high priority white list of brands/model that should always attempt to
  // play vp9.
  private static final Map<String, Set<String>> vp9WhiteList = new HashMap<>();
  // Whether we should report vp9 codecs as supported or not.  Will be set
  // based on whether vp9WhiteList contains our brand/model.  If this is set
  // to true, then codecBlackList will be ignored.
  private static boolean isVp9WhiteListed;
  private static final String SECURE_DECODER_SUFFIX = ".secure";
  private static final String VP9_MIME_TYPE = "video/x-vnd.on2.vp9";

  /**
   * A simple "struct" to bundle up the results from findVideoDecoder, as its clients may require
   * the max supported width and height in addition to just the decoder name.
   */
  public static final class FindVideoDecoderResult {
    public String name;
    public VideoCapabilities videoCapabilities;
    public CodecCapabilities codecCapabilities;

    public FindVideoDecoderResult(
        String name, VideoCapabilities videoCapabilities, CodecCapabilities codecCapabilities) {
      this.name = name;
      this.videoCapabilities = videoCapabilities;
      this.codecCapabilities = codecCapabilities;
    }
  }

  static {
    String brand = android.os.Build.BRAND;
    String model = android.os.Build.MODEL;
    String version = android.os.Build.VERSION.RELEASE;
    int sdkInt = android.os.Build.VERSION.SDK_INT;

    if (sdkInt >= android.os.Build.VERSION_CODES.N && brand.equals("google")) {
      codecBlackList.add("OMX.Nvidia.vp9.decode");
    }
    if (sdkInt >= android.os.Build.VERSION_CODES.N && brand.equals("LGE")) {
      codecBlackList.add("OMX.qcom.video.decoder.vp9");
    }
    if (version.startsWith("6.0.1")) {
      codecBlackList.add("OMX.Exynos.vp9.dec");
      codecBlackList.add("OMX.Intel.VideoDecoder.VP9.hwr");
      codecBlackList.add("OMX.MTK.VIDEO.DECODER.VP9");
      codecBlackList.add("OMX.qcom.video.decoder.vp9");
    }
    if (version.startsWith("6.0")) {
      codecBlackList.add("OMX.MTK.VIDEO.DECODER.VP9");
      codecBlackList.add("OMX.Nvidia.vp9.decode");
    }
    if (version.startsWith("5.1.1")) {
      codecBlackList.add("OMX.allwinner.video.decoder.vp9");
      codecBlackList.add("OMX.Exynos.vp9.dec");
      codecBlackList.add("OMX.Intel.VideoDecoder.VP9.hwr");
      codecBlackList.add("OMX.MTK.VIDEO.DECODER.VP9");
      codecBlackList.add("OMX.qcom.video.decoder.vp9");
    }
    if (version.startsWith("5.1")) {
      codecBlackList.add("OMX.Exynos.VP9.Decoder");
      codecBlackList.add("OMX.Intel.VideoDecoder.VP9.hwr");
      codecBlackList.add("OMX.MTK.VIDEO.DECODER.VP9");
    }
    if (version.startsWith("5.0")) {
      codecBlackList.add("OMX.allwinner.video.decoder.vp9");
      codecBlackList.add("OMX.Exynos.vp9.dec");
      codecBlackList.add("OMX.Intel.VideoDecoder.VP9.hwr");
      codecBlackList.add("OMX.MTK.VIDEO.DECODER.VP9");
    }

    if (brand.equals("google")) {
      codecBlackList.add("OMX.Intel.VideoDecoder.VP9.hybrid");
    }

    // Black list non hardware media codec names if we aren't running on an emulator.
    if (!IsEmulator.isEmulator()) {
      codecBlackList.add("OMX.ffmpeg.vp9.decoder");
      codecBlackList.add("OMX.Intel.sw_vd.vp9");
    }

    // Black list the Google software vp9 decoder both on hardware and on the emulator.
    // On the emulator it fails with the log: "storeMetaDataInBuffers failed w/ err -1010"
    codecBlackList.add("OMX.google.vp9.decoder");

    vp9WhiteList.put("Amlogic", new HashSet<String>());
    vp9WhiteList.put("Arcadyan", new HashSet<String>());
    vp9WhiteList.put("arcelik", new HashSet<String>());
    vp9WhiteList.put("BNO", new HashSet<String>());
    vp9WhiteList.put("BROADCOM", new HashSet<String>());
    vp9WhiteList.put("broadcom", new HashSet<String>());
    vp9WhiteList.put("Foxconn", new HashSet<String>());
    vp9WhiteList.put("Freebox", new HashSet<String>());
    vp9WhiteList.put("gfiber", new HashSet<String>());
    vp9WhiteList.put("Google", new HashSet<String>());
    vp9WhiteList.put("google", new HashSet<String>());
    vp9WhiteList.put("Hisense", new HashSet<String>());
    vp9WhiteList.put("HUAWEI", new HashSet<String>());
    vp9WhiteList.put("KaonMedia", new HashSet<String>());
    vp9WhiteList.put("LeTV", new HashSet<String>());
    vp9WhiteList.put("LGE", new HashSet<String>());
    vp9WhiteList.put("MediaTek", new HashSet<String>());
    vp9WhiteList.put("MStar", new HashSet<String>());
    vp9WhiteList.put("MTK", new HashSet<String>());
    vp9WhiteList.put("NVIDIA", new HashSet<String>());
    vp9WhiteList.put("PHILIPS", new HashSet<String>());
    vp9WhiteList.put("Philips", new HashSet<String>());
    vp9WhiteList.put("PIXELA CORPORATION", new HashSet<String>());
    vp9WhiteList.put("RCA", new HashSet<String>());
    vp9WhiteList.put("Sagemcom", new HashSet<String>());
    vp9WhiteList.put("samsung", new HashSet<String>());
    vp9WhiteList.put("SHARP", new HashSet<String>());
    vp9WhiteList.put("Skyworth", new HashSet<String>());
    vp9WhiteList.put("Sony", new HashSet<String>());
    vp9WhiteList.put("STMicroelectronics", new HashSet<String>());
    vp9WhiteList.put("SumitomoElectricIndustries", new HashSet<String>());
    vp9WhiteList.put("TCL", new HashSet<String>());
    vp9WhiteList.put("Technicolor", new HashSet<String>());
    vp9WhiteList.put("Vestel", new HashSet<String>());
    vp9WhiteList.put("wnc", new HashSet<String>());
    vp9WhiteList.put("Xiaomi", new HashSet<String>());
    vp9WhiteList.put("ZTE TV", new HashSet<String>());

    vp9WhiteList.get("Amlogic").add("p212");
    vp9WhiteList.get("Arcadyan").add("Bouygtel4K");
    vp9WhiteList.get("Arcadyan").add("HMB2213PW22TS");
    vp9WhiteList.get("Arcadyan").add("IPSetTopBox");
    vp9WhiteList.get("arcelik").add("arcelik_uhd_powermax_at");
    vp9WhiteList.get("BNO").add("QM153E");
    vp9WhiteList.get("broadcom").add("avko");
    vp9WhiteList.get("broadcom").add("banff");
    vp9WhiteList.get("BROADCOM").add("BCM7XXX_TEST_SETTOP");
    vp9WhiteList.get("broadcom").add("cypress");
    vp9WhiteList.get("broadcom").add("dawson");
    vp9WhiteList.get("broadcom").add("elfin");
    vp9WhiteList.get("Foxconn").add("ba101");
    vp9WhiteList.get("Foxconn").add("bd201");
    vp9WhiteList.get("Freebox").add("Freebox Player Mini v2");
    vp9WhiteList.get("gfiber").add("GFHD254");
    vp9WhiteList.get("google").add("avko");
    vp9WhiteList.get("google").add("marlin");
    vp9WhiteList.get("Google").add("Pixel XL");
    vp9WhiteList.get("Google").add("Pixel");
    vp9WhiteList.get("google").add("sailfish");
    vp9WhiteList.get("google").add("sprint");
    vp9WhiteList.get("Hisense").add("HAT4KDTV");
    vp9WhiteList.get("HUAWEI").add("X21");
    vp9WhiteList.get("KaonMedia").add("IC1110");
    vp9WhiteList.get("KaonMedia").add("IC1130");
    vp9WhiteList.get("KaonMedia").add("MCM4000");
    vp9WhiteList.get("KaonMedia").add("PRDMK100T");
    vp9WhiteList.get("KaonMedia").add("SFCSTB2LITE");
    vp9WhiteList.get("LeTV").add("uMax85");
    vp9WhiteList.get("LeTV").add("X4-43Pro");
    vp9WhiteList.get("LeTV").add("X4-55");
    vp9WhiteList.get("LeTV").add("X4-65");
    vp9WhiteList.get("LGE").add("S60CLI");
    vp9WhiteList.get("LGE").add("S60UPA");
    vp9WhiteList.get("LGE").add("S60UPI");
    vp9WhiteList.get("LGE").add("S70CDS");
    vp9WhiteList.get("LGE").add("S70PCI");
    vp9WhiteList.get("LGE").add("SH960C-DS");
    vp9WhiteList.get("LGE").add("SH960C-LN");
    vp9WhiteList.get("LGE").add("SH960S-AT");
    vp9WhiteList.get("MediaTek").add("Archer");
    vp9WhiteList.get("MediaTek").add("augie");
    vp9WhiteList.get("MediaTek").add("kane");
    vp9WhiteList.get("MStar").add("Denali");
    vp9WhiteList.get("MStar").add("Rainier");
    vp9WhiteList.get("MTK").add("Generic Android on sharp_2k15_us_android");
    vp9WhiteList.get("NVIDIA").add("SHIELD Android TV");
    vp9WhiteList.get("NVIDIA").add("SHIELD Console");
    vp9WhiteList.get("NVIDIA").add("SHIELD Portable");
    vp9WhiteList.get("PHILIPS").add("QM151E");
    vp9WhiteList.get("PHILIPS").add("QM161E");
    vp9WhiteList.get("PHILIPS").add("QM163E");
    vp9WhiteList.get("Philips").add("TPM171E");
    vp9WhiteList.get("PIXELA CORPORATION").add("POE-MP4000");
    vp9WhiteList.get("RCA").add("XLDRCAV1");
    vp9WhiteList.get("Sagemcom").add("DNA Android TV");
    vp9WhiteList.get("Sagemcom").add("GigaTV");
    vp9WhiteList.get("Sagemcom").add("M387_QL");
    vp9WhiteList.get("Sagemcom").add("Sagemcom Android STB");
    vp9WhiteList.get("Sagemcom").add("Sagemcom ATV Demo");
    vp9WhiteList.get("Sagemcom").add("Telecable ATV");
    vp9WhiteList.get("samsung").add("c71kw200");
    vp9WhiteList.get("samsung").add("GX-CJ680CL");
    vp9WhiteList.get("samsung").add("SAMSUNG-SM-G890A");
    vp9WhiteList.get("samsung").add("SAMSUNG-SM-G920A");
    vp9WhiteList.get("samsung").add("SAMSUNG-SM-G920AZ");
    vp9WhiteList.get("samsung").add("SAMSUNG-SM-G925A");
    vp9WhiteList.get("samsung").add("SAMSUNG-SM-G928A");
    vp9WhiteList.get("samsung").add("SM-G9200");
    vp9WhiteList.get("samsung").add("SM-G9208");
    vp9WhiteList.get("samsung").add("SM-G9209");
    vp9WhiteList.get("samsung").add("SM-G920A");
    vp9WhiteList.get("samsung").add("SM-G920D");
    vp9WhiteList.get("samsung").add("SM-G920F");
    vp9WhiteList.get("samsung").add("SM-G920FD");
    vp9WhiteList.get("samsung").add("SM-G920FQ");
    vp9WhiteList.get("samsung").add("SM-G920I");
    vp9WhiteList.get("samsung").add("SM-G920K");
    vp9WhiteList.get("samsung").add("SM-G920L");
    vp9WhiteList.get("samsung").add("SM-G920P");
    vp9WhiteList.get("samsung").add("SM-G920R4");
    vp9WhiteList.get("samsung").add("SM-G920R6");
    vp9WhiteList.get("samsung").add("SM-G920R7");
    vp9WhiteList.get("samsung").add("SM-G920S");
    vp9WhiteList.get("samsung").add("SM-G920T");
    vp9WhiteList.get("samsung").add("SM-G920T1");
    vp9WhiteList.get("samsung").add("SM-G920V");
    vp9WhiteList.get("samsung").add("SM-G920W8");
    vp9WhiteList.get("samsung").add("SM-G9250");
    vp9WhiteList.get("samsung").add("SM-G925A");
    vp9WhiteList.get("samsung").add("SM-G925D");
    vp9WhiteList.get("samsung").add("SM-G925F");
    vp9WhiteList.get("samsung").add("SM-G925FQ");
    vp9WhiteList.get("samsung").add("SM-G925I");
    vp9WhiteList.get("samsung").add("SM-G925J");
    vp9WhiteList.get("samsung").add("SM-G925K");
    vp9WhiteList.get("samsung").add("SM-G925L");
    vp9WhiteList.get("samsung").add("SM-G925P");
    vp9WhiteList.get("samsung").add("SM-G925R4");
    vp9WhiteList.get("samsung").add("SM-G925R6");
    vp9WhiteList.get("samsung").add("SM-G925R7");
    vp9WhiteList.get("samsung").add("SM-G925S");
    vp9WhiteList.get("samsung").add("SM-G925T");
    vp9WhiteList.get("samsung").add("SM-G925V");
    vp9WhiteList.get("samsung").add("SM-G925W8");
    vp9WhiteList.get("samsung").add("SM-G925Z");
    vp9WhiteList.get("samsung").add("SM-G9280");
    vp9WhiteList.get("samsung").add("SM-G9287");
    vp9WhiteList.get("samsung").add("SM-G9287C");
    vp9WhiteList.get("samsung").add("SM-G928A");
    vp9WhiteList.get("samsung").add("SM-G928C");
    vp9WhiteList.get("samsung").add("SM-G928F");
    vp9WhiteList.get("samsung").add("SM-G928G");
    vp9WhiteList.get("samsung").add("SM-G928I");
    vp9WhiteList.get("samsung").add("SM-G928K");
    vp9WhiteList.get("samsung").add("SM-G928L");
    vp9WhiteList.get("samsung").add("SM-G928N0");
    vp9WhiteList.get("samsung").add("SM-G928P");
    vp9WhiteList.get("samsung").add("SM-G928S");
    vp9WhiteList.get("samsung").add("SM-G928T");
    vp9WhiteList.get("samsung").add("SM-G928V");
    vp9WhiteList.get("samsung").add("SM-G928W8");
    vp9WhiteList.get("samsung").add("SM-G928X");
    vp9WhiteList.get("samsung").add("SM-G9300");
    vp9WhiteList.get("samsung").add("SM-G9308");
    vp9WhiteList.get("samsung").add("SM-G930A");
    vp9WhiteList.get("samsung").add("SM-G930AZ");
    vp9WhiteList.get("samsung").add("SM-G930F");
    vp9WhiteList.get("samsung").add("SM-G930FD");
    vp9WhiteList.get("samsung").add("SM-G930K");
    vp9WhiteList.get("samsung").add("SM-G930L");
    vp9WhiteList.get("samsung").add("SM-G930P");
    vp9WhiteList.get("samsung").add("SM-G930R4");
    vp9WhiteList.get("samsung").add("SM-G930R6");
    vp9WhiteList.get("samsung").add("SM-G930R7");
    vp9WhiteList.get("samsung").add("SM-G930S");
    vp9WhiteList.get("samsung").add("SM-G930T");
    vp9WhiteList.get("samsung").add("SM-G930T1");
    vp9WhiteList.get("samsung").add("SM-G930U");
    vp9WhiteList.get("samsung").add("SM-G930V");
    vp9WhiteList.get("samsung").add("SM-G930VL");
    vp9WhiteList.get("samsung").add("SM-G930W8");
    vp9WhiteList.get("samsung").add("SM-G9350");
    vp9WhiteList.get("samsung").add("SM-G935A");
    vp9WhiteList.get("samsung").add("SM-G935D");
    vp9WhiteList.get("samsung").add("SM-G935F");
    vp9WhiteList.get("samsung").add("SM-G935FD");
    vp9WhiteList.get("samsung").add("SM-G935J");
    vp9WhiteList.get("samsung").add("SM-G935K");
    vp9WhiteList.get("samsung").add("SM-G935L");
    vp9WhiteList.get("samsung").add("SM-G935P");
    vp9WhiteList.get("samsung").add("SM-G935R4");
    vp9WhiteList.get("samsung").add("SM-G935S");
    vp9WhiteList.get("samsung").add("SM-G935T");
    vp9WhiteList.get("samsung").add("SM-G935U");
    vp9WhiteList.get("samsung").add("SM-G935V");
    vp9WhiteList.get("samsung").add("SM-G935W8");
    vp9WhiteList.get("samsung").add("SM-N9200");
    vp9WhiteList.get("samsung").add("SM-N9208");
    vp9WhiteList.get("samsung").add("SM-N920A");
    vp9WhiteList.get("samsung").add("SM-N920C");
    vp9WhiteList.get("samsung").add("SM-N920F");
    vp9WhiteList.get("samsung").add("SM-N920G");
    vp9WhiteList.get("samsung").add("SM-N920I");
    vp9WhiteList.get("samsung").add("SM-N920K");
    vp9WhiteList.get("samsung").add("SM-N920L");
    vp9WhiteList.get("samsung").add("SM-N920R4");
    vp9WhiteList.get("samsung").add("SM-N920R6");
    vp9WhiteList.get("samsung").add("SM-N920R7");
    vp9WhiteList.get("samsung").add("SM-N920S");
    vp9WhiteList.get("samsung").add("SM-N920T");
    vp9WhiteList.get("samsung").add("SM-N920TP");
    vp9WhiteList.get("samsung").add("SM-N920V");
    vp9WhiteList.get("samsung").add("SM-N920W8");
    vp9WhiteList.get("samsung").add("SM-N920X");
    vp9WhiteList.get("SHARP").add("AN-NP40");
    vp9WhiteList.get("SHARP").add("AQUOS-4KTVJ17");
    vp9WhiteList.get("SHARP").add("AQUOS-4KTVT17");
    vp9WhiteList.get("SHARP").add("AQUOS-4KTVX17");
    vp9WhiteList.get("SHARP").add("LC-U35T");
    vp9WhiteList.get("SHARP").add("LC-UE630X");
    vp9WhiteList.get("SHARP").add("LC-Ux30US");
    vp9WhiteList.get("SHARP").add("LC-XU35T");
    vp9WhiteList.get("SHARP").add("LC-XU930X_830X");
    vp9WhiteList.get("Skyworth").add("globe");
    vp9WhiteList.get("Sony").add("Amai VP9");
    vp9WhiteList.get("Sony").add("BRAVIA 4K 2015");
    vp9WhiteList.get("Sony").add("BRAVIA 4K GB");
    vp9WhiteList.get("STMicroelectronics").add("sti4k");
    vp9WhiteList.get("SumitomoElectricIndustries").add("C02AS");
    vp9WhiteList.get("SumitomoElectricIndustries").add("ST4173");
    vp9WhiteList.get("SumitomoElectricIndustries").add("test_STW2000");
    vp9WhiteList.get("TCL").add("Percee TV");
    vp9WhiteList.get("Technicolor").add("AirTV Player");
    vp9WhiteList.get("Technicolor").add("Bouygtel4K");
    vp9WhiteList.get("Technicolor").add("CM-7600");
    vp9WhiteList.get("Technicolor").add("cooper");
    vp9WhiteList.get("Technicolor").add("Foxtel Now box");
    vp9WhiteList.get("Technicolor").add("pearl");
    vp9WhiteList.get("Technicolor").add("Sapphire");
    vp9WhiteList.get("Technicolor").add("Shortcut");
    vp9WhiteList.get("Technicolor").add("skipper");
    vp9WhiteList.get("Technicolor").add("STING");
    vp9WhiteList.get("Technicolor").add("TIM_BOX");
    vp9WhiteList.get("Technicolor").add("uzx8020chm");
    vp9WhiteList.get("Vestel").add("S7252");
    vp9WhiteList.get("Vestel").add("SmartTV");
    vp9WhiteList.get("wnc").add("c71kw400");
    vp9WhiteList.get("Xiaomi").add("MIBOX3");
    vp9WhiteList.get("ZTE TV").add("AV-ATB100");
    vp9WhiteList.get("ZTE TV").add("B860H");

    isVp9WhiteListed = vp9WhiteList.containsKey(brand) && vp9WhiteList.get(brand).contains(model);
  }

  private MediaCodecUtil() {}

  /**
   * Returns whether a given combination of (frame width x frame height) frames at bitrate and fps
   * has a decoder with mime type.
   *
   * <p>Setting any of the int parameters to 0 indicates that they shouldn't be considered.
   */
  @SuppressWarnings("unused")
  @UsedByNative
  public static boolean hasVideoDecoderFor(
      String mimeType, boolean secure, int frameWidth, int frameHeight, int bitrate, int fps) {
    return !findVideoDecoder(mimeType, secure, frameWidth, frameHeight, bitrate, fps)
        .name
        .equals("");
  }

  /**
   * Returns whether an audio decoder that supports mimeType at bitrate. Setting bitrate to 0
   * indicates that it should not be considered.
   */
  @SuppressWarnings("unused")
  @UsedByNative
  public static boolean hasAudioDecoderFor(String mimeType, int bitrate) {
    return !findAudioDecoder(mimeType, bitrate).equals("");
  }

  /** Determine whether the system has a decoder capable of playing HDR VP9. */
  @SuppressWarnings("unused")
  @UsedByNative
  public static boolean hasHdrCapableVp9Decoder() {
    // VP9Profile* values were not added until API level 24.  See
    // https://developer.android.com/reference/android/media/MediaCodecInfo.CodecProfileLevel.html.
    if (android.os.Build.VERSION.SDK_INT < 24) {
      return false;
    }

    // Ideally we would just add this as a parameter to |findVideoDecoder|,
    // however the shared starboard player implementation of
    // |CanPlayMimeAndKeySystem| will call
    // |SbMediaIsTransferCharacteristicsSupported| as a separate function.  We
    // make the reasonable assumption that no system will have two hardware
    // VP9 decoders with one capable of playing HDR at lower
    // resolutions/bitrates, and another not capable of playing HDR at higher
    // resolutions/bitrates.
    FindVideoDecoderResult findVideoDecoderResult =
        findVideoDecoder(VP9_MIME_TYPE, false, 0, 0, 0, 0);
    return isHdrCapableVp9Decoder(findVideoDecoderResult);
  }

  /** Determine whether findVideoDecoderResult is capable of playing HDR VP9 */
  public static boolean isHdrCapableVp9Decoder(FindVideoDecoderResult findVideoDecoderResult) {
    if (!findVideoDecoderResult.name.equals(VP9_MIME_TYPE)) {
      return false;
    }

    CodecCapabilities codecCapabilities = findVideoDecoderResult.codecCapabilities;
    CodecProfileLevel[] codecProfileLevels = codecCapabilities.profileLevels;
    if (codecProfileLevels == null) {
      return false;
    }
    for (CodecProfileLevel codecProfileLevel : codecProfileLevels) {
      if (codecProfileLevel.profile == CodecProfileLevel.VP9Profile2HDR
          || codecProfileLevel.profile == CodecProfileLevel.VP9Profile3HDR) {
        return true;
      }
    }

    return false;
  }

  /**
   * The same as hasVideoDecoderFor, only return the name of the video decoder if it is found, and
   * "" otherwise.
   */
  public static FindVideoDecoderResult findVideoDecoder(
      String mimeType, boolean secure, int frameWidth, int frameHeight, int bitrate, int fps) {
    Log.v(
        TAG,
        String.format(
            "Searching for video decoder with parameters "
                + "mimeType: %s, secure: %b, frameWidth: %d, frameHeight %d, bitrate: %d, fps: %d",
            mimeType, secure, frameWidth, frameHeight, bitrate, fps));
    Log.v(
        TAG,
        String.format(
            "brand: %s, model: %s, version: %s, sdkInt: %d, isVp9WhiteListed: %b",
            android.os.Build.BRAND,
            android.os.Build.MODEL,
            android.os.Build.VERSION.RELEASE,
            android.os.Build.VERSION.SDK_INT,
            isVp9WhiteListed));

    // Note: MediaCodecList is sorted by the framework such that the best decoders come first.
    for (MediaCodecInfo info : new MediaCodecList(MediaCodecList.ALL_CODECS).getCodecInfos()) {
      if (info.isEncoder()) {
        continue;
      }
      for (String supportedType : info.getSupportedTypes()) {
        if (!supportedType.equalsIgnoreCase(mimeType)) {
          continue;
        }

        String name = info.getName();
        if (!isVp9WhiteListed && codecBlackList.contains(name)) {
          Log.v(TAG, String.format("Rejecting %s, reason: codec is black listed", name));
          continue;
        }
        // MediaCodecList is supposed to feed us names of decoders that do NOT end in ".secure".  We
        // are then supposed to check if FEATURE_SecurePlayback is supported, and it if is and we
        // want a secure codec, we append ".secure" ourselves, and then pass that to
        // MediaCodec.createDecoderByName.  Some devices, do not follow this spec, and show us
        // decoders that end in ".secure".  Empirically, FEATURE_SecurePlayback has still been
        // correct when this happens.
        if (name.endsWith(SECURE_DECODER_SUFFIX)) {
          // If we want a secure decoder, then make sure the version without ".secure" isn't
          // blacklisted.
          String nameWithoutSecureSuffix =
              name.substring(0, name.length() - SECURE_DECODER_SUFFIX.length());
          if (secure && !isVp9WhiteListed && codecBlackList.contains(nameWithoutSecureSuffix)) {
            Log.v(
                TAG,
                String.format("Rejecting %s, reason: offpsec blacklisted secure decoder", name));
            continue;
          }
          // If we don't want a secure decoder, then don't bother messing around with this thing.
          if (!secure) {
            Log.v(
                TAG,
                String.format(
                    "Rejecting %s, reason: want !secure decoder and ends with .secure", name));
            continue;
          }
        }

        CodecCapabilities codecCapabilities = info.getCapabilitiesForType(supportedType);
        if (secure
            && !codecCapabilities.isFeatureSupported(
                MediaCodecInfo.CodecCapabilities.FEATURE_SecurePlayback)) {
          Log.v(
              TAG,
              String.format(
                  "Rejecting %s, reason: want secure decoder and !FEATURE_SecurePlayback", name));
          continue;
        }

        // VideoCapabilties is not implemented correctly on this device.
        if (android.os.Build.VERSION.SDK_INT < 23
            && android.os.Build.MODEL.equals("MIBOX3")
            && name.equals("OMX.amlogic.vp9.decoder.awesome")
            && (frameWidth > 1920 || frameHeight > 1080)) {
          Log.v(TAG, "Skipping >1080p OMX.amlogic.vp9.decoder.awesome on mibox.");
          continue;
        }

        VideoCapabilities videoCapabilities = codecCapabilities.getVideoCapabilities();
        if (frameWidth != 0 && !videoCapabilities.getSupportedWidths().contains(frameWidth)) {
          Log.v(
              TAG,
              String.format(
                  "Rejecting %s, reason: supported widths %s does not contain %d",
                  name, videoCapabilities.getSupportedWidths().toString(), frameWidth));
          continue;
        }
        if (frameHeight != 0 && !videoCapabilities.getSupportedHeights().contains(frameHeight)) {
          Log.v(
              TAG,
              String.format(
                  "Rejecting %s, reason: supported heights %s does not contain %d",
                  name, videoCapabilities.getSupportedHeights().toString(), frameHeight));
          continue;
        }
        if (bitrate != 0 && !videoCapabilities.getBitrateRange().contains(bitrate)) {
          Log.v(
              TAG,
              String.format(
                  "Rejecting %s, reason: bitrate range %s does not contain %d",
                  name, videoCapabilities.getBitrateRange().toString(), bitrate));
          continue;
        }
        if (fps != 0 && !videoCapabilities.getSupportedFrameRates().contains(fps)) {
          Log.v(
              TAG,
              String.format(
                  "Rejecting %s, reason: supported frame rates %s does not contain %d",
                  name, videoCapabilities.getSupportedFrameRates().toString(), fps));
          continue;
        }
        if (frameWidth != 0 && frameHeight != 0 && fps != 0
            && !videoCapabilities.getSupportedFrameRatesFor(frameWidth, frameHeight)
                .contains((double) fps)) {
          Log.v(TAG,
              String.format(
                  "Rejecting %s, reason: supported frame rates %s "
                      + "for size (%d x %d) does not contain %d",
                  name,
                  videoCapabilities.getSupportedFrameRatesFor(frameWidth, frameHeight).toString(),
                  frameWidth, frameHeight, fps));
          continue;
        }

        String resultName =
            (secure && !name.endsWith(SECURE_DECODER_SUFFIX))
                ? (name + SECURE_DECODER_SUFFIX)
                : name;
        Log.v(TAG, String.format("Found suitable decoder, %s", name));
        return new FindVideoDecoderResult(resultName, videoCapabilities, codecCapabilities);
      }
    }
    return new FindVideoDecoderResult("", null, null);
  }

  /**
   * The same as hasAudioDecoderFor, only return the name of the audio decoder if it is found, and
   * "" otherwise.
   */
  public static String findAudioDecoder(String mimeType, int bitrate) {
    // Note: MediaCodecList is sorted by the framework such that the best decoders come first.
    for (MediaCodecInfo info : new MediaCodecList(MediaCodecList.ALL_CODECS).getCodecInfos()) {
      if (info.isEncoder()) {
        continue;
      }
      for (String supportedType : info.getSupportedTypes()) {
        if (!supportedType.equalsIgnoreCase(mimeType)) {
          continue;
        }
        String name = info.getName();
        AudioCapabilities audioCapabilities =
            info.getCapabilitiesForType(supportedType).getAudioCapabilities();
        if (bitrate != 0 && !audioCapabilities.getBitrateRange().contains(bitrate)) {
          continue;
        }
        return name;
      }
    }
    return "";
  }

  /**
   * Debug utility function that can be locally added to dump information about all decoders on a
   * particular system.
   */
  @SuppressWarnings("unused")
  private static void dumpAllDecoders() {
    for (MediaCodecInfo info : new MediaCodecList(MediaCodecList.ALL_CODECS).getCodecInfos()) {
      if (info.isEncoder()) {
        continue;
      }
      for (String supportedType : info.getSupportedTypes()) {
        String name = info.getName();
        CodecCapabilities codecCapabilities = info.getCapabilitiesForType(supportedType);
        Log.v(TAG, "==================================================");
        Log.v(TAG, String.format("name: %s", name));
        Log.v(TAG, String.format("supportedType: %s", supportedType));
        Log.v(
            TAG, String.format("codecBlackList.contains(name): %b", codecBlackList.contains(name)));
        Log.v(
            TAG,
            String.format(
                "FEATURE_SecurePlayback: %b",
                codecCapabilities.isFeatureSupported(
                    MediaCodecInfo.CodecCapabilities.FEATURE_SecurePlayback)));
        VideoCapabilities videoCapabilities = codecCapabilities.getVideoCapabilities();
        if (videoCapabilities != null) {
          Log.v(
              TAG,
              String.format(
                  "videoCapabilities.getSupportedWidths(): %s",
                  videoCapabilities.getSupportedWidths().toString()));
          Log.v(
              TAG,
              String.format(
                  "videoCapabilities.getSupportedHeights(): %s",
                  videoCapabilities.getSupportedHeights().toString()));
          Log.v(
              TAG,
              String.format(
                  "videoCapabilities.getBitrateRange(): %s",
                  videoCapabilities.getBitrateRange().toString()));
          Log.v(
              TAG,
              String.format(
                  "videoCapabilities.getSupportedFrameRates(): %s",
                  videoCapabilities.getSupportedFrameRates().toString()));
        }
        Log.v(TAG, "==================================================");
        Log.v(TAG, "");
      }
    }
  }
}
