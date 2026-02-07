// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
import android.os.Build;
import android.util.Range;
import dev.cobalt.util.IsEmulator;
import dev.cobalt.util.Log;
import dev.cobalt.util.UsedByNative;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/** Utility functions for dealing with MediaCodec related things. */
public class MediaCodecUtil {
  // A low priority deny list of video codec names that should never be used.
  private static final Set<String> videoCodecDenyList = new HashSet<>();
  // A high priority allow list of brands/model that should always attempt to
  // play vp9.
  private static final Map<String, Set<String>> vp9AllowList = new HashMap<>();
  // Whether we should report vp9 codecs as supported or not.  Will be set
  // based on whether vp9AllowList contains our brand/model.  If this is set
  // to true, then videoCodecDenyList will be ignored.
  private static boolean isVp9AllowListed;
  private static final String SECURE_DECODER_SUFFIX = ".secure";
  private static final String VP9_MIME_TYPE = "video/x-vnd.on2.vp9";
  private static final String AV1_MIME_TYPE = "video/av01";

  static {
    if (Build.BRAND.equals("google")) {
      videoCodecDenyList.add("OMX.Nvidia.vp9.decode");
    }
    if (Build.BRAND.equals("LGE")) {
      videoCodecDenyList.add("OMX.qcom.video.decoder.vp9");
    }
    if (Build.VERSION.RELEASE.startsWith("6.0.1")) {
      videoCodecDenyList.add("OMX.Exynos.vp9.dec");
      videoCodecDenyList.add("OMX.Intel.VideoDecoder.VP9.hwr");
      videoCodecDenyList.add("OMX.MTK.VIDEO.DECODER.VP9");
      videoCodecDenyList.add("OMX.qcom.video.decoder.vp9");
    }
    if (Build.VERSION.RELEASE.startsWith("6.0")) {
      videoCodecDenyList.add("OMX.MTK.VIDEO.DECODER.VP9");
      videoCodecDenyList.add("OMX.Nvidia.vp9.decode");
    }
    if (Build.VERSION.RELEASE.startsWith("5.1.1")) {
      videoCodecDenyList.add("OMX.allwinner.video.decoder.vp9");
      videoCodecDenyList.add("OMX.Exynos.vp9.dec");
      videoCodecDenyList.add("OMX.Intel.VideoDecoder.VP9.hwr");
      videoCodecDenyList.add("OMX.MTK.VIDEO.DECODER.VP9");
      videoCodecDenyList.add("OMX.qcom.video.decoder.vp9");
    }
    if (Build.VERSION.RELEASE.startsWith("5.1")) {
      videoCodecDenyList.add("OMX.Exynos.VP9.Decoder");
      videoCodecDenyList.add("OMX.Intel.VideoDecoder.VP9.hwr");
      videoCodecDenyList.add("OMX.MTK.VIDEO.DECODER.VP9");
    }
    if (Build.VERSION.RELEASE.startsWith("5.0")) {
      videoCodecDenyList.add("OMX.allwinner.video.decoder.vp9");
      videoCodecDenyList.add("OMX.Exynos.vp9.dec");
      videoCodecDenyList.add("OMX.Intel.VideoDecoder.VP9.hwr");
      videoCodecDenyList.add("OMX.MTK.VIDEO.DECODER.VP9");
    }

    if (Build.BRAND.equals("google")) {
      videoCodecDenyList.add("OMX.Intel.VideoDecoder.VP9.hybrid");
    }

    // Denylist non hardware media codec names if we aren't running on an emulator.
    if (!IsEmulator.isEmulator()) {
      videoCodecDenyList.add("OMX.ffmpeg.vp9.decoder");
      videoCodecDenyList.add("OMX.Intel.sw_vd.vp9");
      videoCodecDenyList.add("OMX.MTK.VIDEO.DECODER.SW.VP9");
    }

    // Denylist the Google software vp9 decoder both on hardware and on the emulator.
    // On the emulator it fails with the log: "storeMetaDataInBuffers failed w/ err -1010"
    videoCodecDenyList.add("OMX.google.vp9.decoder");

    vp9AllowList.put("Amazon", new HashSet<String>());
    vp9AllowList.put("Amlogic", new HashSet<String>());
    vp9AllowList.put("Arcadyan", new HashSet<String>());
    vp9AllowList.put("arcelik", new HashSet<String>());
    vp9AllowList.put("BNO", new HashSet<String>());
    vp9AllowList.put("BROADCOM", new HashSet<String>());
    vp9AllowList.put("broadcom", new HashSet<String>());
    vp9AllowList.put("Foxconn", new HashSet<String>());
    vp9AllowList.put("Freebox", new HashSet<String>());
    vp9AllowList.put("Funai", new HashSet<String>());
    vp9AllowList.put("gfiber", new HashSet<String>());
    vp9AllowList.put("Google", new HashSet<String>());
    vp9AllowList.put("google", new HashSet<String>());
    vp9AllowList.put("Hisense", new HashSet<String>());
    vp9AllowList.put("HUAWEI", new HashSet<String>());
    vp9AllowList.put("KaonMedia", new HashSet<String>());
    vp9AllowList.put("LeTV", new HashSet<String>());
    vp9AllowList.put("LGE", new HashSet<String>());
    vp9AllowList.put("MediaTek", new HashSet<String>());
    vp9AllowList.put("MStar", new HashSet<String>());
    vp9AllowList.put("MTK", new HashSet<String>());
    vp9AllowList.put("NVIDIA", new HashSet<String>());
    vp9AllowList.put("PHILIPS", new HashSet<String>());
    vp9AllowList.put("Philips", new HashSet<String>());
    vp9AllowList.put("PIXELA CORPORATION", new HashSet<String>());
    vp9AllowList.put("RCA", new HashSet<String>());
    vp9AllowList.put("Sagemcom", new HashSet<String>());
    vp9AllowList.put("samsung", new HashSet<String>());
    vp9AllowList.put("SHARP", new HashSet<String>());
    vp9AllowList.put("Skyworth", new HashSet<String>());
    vp9AllowList.put("Sony", new HashSet<String>());
    vp9AllowList.put("STMicroelectronics", new HashSet<String>());
    vp9AllowList.put("SumitomoElectricIndustries", new HashSet<String>());
    vp9AllowList.put("TCL", new HashSet<String>());
    vp9AllowList.put("Technicolor", new HashSet<String>());
    vp9AllowList.put("Vestel", new HashSet<String>());
    vp9AllowList.put("wnc", new HashSet<String>());
    vp9AllowList.put("Xiaomi", new HashSet<String>());
    vp9AllowList.put("ZTE TV", new HashSet<String>());

    vp9AllowList.get("Amazon").add("AFTS");
    vp9AllowList.get("Amlogic").add("p212");
    vp9AllowList.get("Arcadyan").add("Bouygtel4K");
    vp9AllowList.get("Arcadyan").add("HMB2213PW22TS");
    vp9AllowList.get("Arcadyan").add("IPSetTopBox");
    vp9AllowList.get("arcelik").add("arcelik_uhd_powermax_at");
    vp9AllowList.get("BNO").add("QM153E");
    vp9AllowList.get("broadcom").add("avko");
    vp9AllowList.get("broadcom").add("banff");
    vp9AllowList.get("BROADCOM").add("BCM7XXX_TEST_SETTOP");
    vp9AllowList.get("broadcom").add("cypress");
    vp9AllowList.get("broadcom").add("dawson");
    vp9AllowList.get("broadcom").add("elfin");
    vp9AllowList.get("Foxconn").add("ba101");
    vp9AllowList.get("Foxconn").add("bd201");
    vp9AllowList.get("Freebox").add("Freebox Player Mini v2");
    vp9AllowList.get("Funai").add("PHILIPS 4K TV");
    vp9AllowList.get("gfiber").add("GFHD254");
    vp9AllowList.get("google").add("avko");
    vp9AllowList.get("google").add("marlin");
    vp9AllowList.get("Google").add("Pixel XL");
    vp9AllowList.get("Google").add("Pixel");
    vp9AllowList.get("google").add("sailfish");
    vp9AllowList.get("google").add("sprint");
    vp9AllowList.get("Hisense").add("HAT4KDTV");
    vp9AllowList.get("HUAWEI").add("X21");
    vp9AllowList.get("KaonMedia").add("IC1110");
    vp9AllowList.get("KaonMedia").add("IC1130");
    vp9AllowList.get("KaonMedia").add("MCM4000");
    vp9AllowList.get("KaonMedia").add("PRDMK100T");
    vp9AllowList.get("KaonMedia").add("SFCSTB2LITE");
    vp9AllowList.get("LeTV").add("uMax85");
    vp9AllowList.get("LeTV").add("X4-43Pro");
    vp9AllowList.get("LeTV").add("X4-55");
    vp9AllowList.get("LeTV").add("X4-65");
    vp9AllowList.get("LGE").add("S60CLI");
    vp9AllowList.get("LGE").add("S60UPA");
    vp9AllowList.get("LGE").add("S60UPI");
    vp9AllowList.get("LGE").add("S70CDS");
    vp9AllowList.get("LGE").add("S70PCI");
    vp9AllowList.get("LGE").add("SH960C-DS");
    vp9AllowList.get("LGE").add("SH960C-LN");
    vp9AllowList.get("LGE").add("SH960S-AT");
    vp9AllowList.get("MediaTek").add("Archer");
    vp9AllowList.get("MediaTek").add("augie");
    vp9AllowList.get("MediaTek").add("kane");
    vp9AllowList.get("MStar").add("Denali");
    vp9AllowList.get("MStar").add("Rainier");
    vp9AllowList.get("MTK").add("Generic Android on sharp_2k15_us_android");
    vp9AllowList.get("NVIDIA").add("SHIELD Android TV");
    vp9AllowList.get("NVIDIA").add("SHIELD Console");
    vp9AllowList.get("NVIDIA").add("SHIELD Portable");
    vp9AllowList.get("PHILIPS").add("QM151E");
    vp9AllowList.get("PHILIPS").add("QM161E");
    vp9AllowList.get("PHILIPS").add("QM163E");
    vp9AllowList.get("Philips").add("TPM171E");
    vp9AllowList.get("PIXELA CORPORATION").add("POE-MP4000");
    vp9AllowList.get("RCA").add("XLDRCAV1");
    vp9AllowList.get("Sagemcom").add("DNA Android TV");
    vp9AllowList.get("Sagemcom").add("GigaTV");
    vp9AllowList.get("Sagemcom").add("M387_QL");
    vp9AllowList.get("Sagemcom").add("Sagemcom Android STB");
    vp9AllowList.get("Sagemcom").add("Sagemcom ATV Demo");
    vp9AllowList.get("Sagemcom").add("Telecable ATV");
    vp9AllowList.get("samsung").add("c71kw200");
    vp9AllowList.get("samsung").add("GX-CJ680CL");
    vp9AllowList.get("samsung").add("SAMSUNG-SM-G890A");
    vp9AllowList.get("samsung").add("SAMSUNG-SM-G920A");
    vp9AllowList.get("samsung").add("SAMSUNG-SM-G920AZ");
    vp9AllowList.get("samsung").add("SAMSUNG-SM-G925A");
    vp9AllowList.get("samsung").add("SAMSUNG-SM-G928A");
    vp9AllowList.get("samsung").add("SM-G9200");
    vp9AllowList.get("samsung").add("SM-G9208");
    vp9AllowList.get("samsung").add("SM-G9209");
    vp9AllowList.get("samsung").add("SM-G920A");
    vp9AllowList.get("samsung").add("SM-G920D");
    vp9AllowList.get("samsung").add("SM-G920F");
    vp9AllowList.get("samsung").add("SM-G920FD");
    vp9AllowList.get("samsung").add("SM-G920FQ");
    vp9AllowList.get("samsung").add("SM-G920I");
    vp9AllowList.get("samsung").add("SM-G920K");
    vp9AllowList.get("samsung").add("SM-G920L");
    vp9AllowList.get("samsung").add("SM-G920P");
    vp9AllowList.get("samsung").add("SM-G920R4");
    vp9AllowList.get("samsung").add("SM-G920R6");
    vp9AllowList.get("samsung").add("SM-G920R7");
    vp9AllowList.get("samsung").add("SM-G920S");
    vp9AllowList.get("samsung").add("SM-G920T");
    vp9AllowList.get("samsung").add("SM-G920T1");
    vp9AllowList.get("samsung").add("SM-G920V");
    vp9AllowList.get("samsung").add("SM-G920W8");
    vp9AllowList.get("samsung").add("SM-G9250");
    vp9AllowList.get("samsung").add("SM-G925A");
    vp9AllowList.get("samsung").add("SM-G925D");
    vp9AllowList.get("samsung").add("SM-G925F");
    vp9AllowList.get("samsung").add("SM-G925FQ");
    vp9AllowList.get("samsung").add("SM-G925I");
    vp9AllowList.get("samsung").add("SM-G925J");
    vp9AllowList.get("samsung").add("SM-G925K");
    vp9AllowList.get("samsung").add("SM-G925L");
    vp9AllowList.get("samsung").add("SM-G925P");
    vp9AllowList.get("samsung").add("SM-G925R4");
    vp9AllowList.get("samsung").add("SM-G925R6");
    vp9AllowList.get("samsung").add("SM-G925R7");
    vp9AllowList.get("samsung").add("SM-G925S");
    vp9AllowList.get("samsung").add("SM-G925T");
    vp9AllowList.get("samsung").add("SM-G925V");
    vp9AllowList.get("samsung").add("SM-G925W8");
    vp9AllowList.get("samsung").add("SM-G925Z");
    vp9AllowList.get("samsung").add("SM-G9280");
    vp9AllowList.get("samsung").add("SM-G9287");
    vp9AllowList.get("samsung").add("SM-G9287C");
    vp9AllowList.get("samsung").add("SM-G928A");
    vp9AllowList.get("samsung").add("SM-G928C");
    vp9AllowList.get("samsung").add("SM-G928F");
    vp9AllowList.get("samsung").add("SM-G928G");
    vp9AllowList.get("samsung").add("SM-G928I");
    vp9AllowList.get("samsung").add("SM-G928K");
    vp9AllowList.get("samsung").add("SM-G928L");
    vp9AllowList.get("samsung").add("SM-G928N0");
    vp9AllowList.get("samsung").add("SM-G928P");
    vp9AllowList.get("samsung").add("SM-G928S");
    vp9AllowList.get("samsung").add("SM-G928T");
    vp9AllowList.get("samsung").add("SM-G928V");
    vp9AllowList.get("samsung").add("SM-G928W8");
    vp9AllowList.get("samsung").add("SM-G928X");
    vp9AllowList.get("samsung").add("SM-G9300");
    vp9AllowList.get("samsung").add("SM-G9308");
    vp9AllowList.get("samsung").add("SM-G930A");
    vp9AllowList.get("samsung").add("SM-G930AZ");
    vp9AllowList.get("samsung").add("SM-G930F");
    vp9AllowList.get("samsung").add("SM-G930FD");
    vp9AllowList.get("samsung").add("SM-G930K");
    vp9AllowList.get("samsung").add("SM-G930L");
    vp9AllowList.get("samsung").add("SM-G930P");
    vp9AllowList.get("samsung").add("SM-G930R4");
    vp9AllowList.get("samsung").add("SM-G930R6");
    vp9AllowList.get("samsung").add("SM-G930R7");
    vp9AllowList.get("samsung").add("SM-G930S");
    vp9AllowList.get("samsung").add("SM-G930T");
    vp9AllowList.get("samsung").add("SM-G930T1");
    vp9AllowList.get("samsung").add("SM-G930U");
    vp9AllowList.get("samsung").add("SM-G930V");
    vp9AllowList.get("samsung").add("SM-G930VL");
    vp9AllowList.get("samsung").add("SM-G930W8");
    vp9AllowList.get("samsung").add("SM-G9350");
    vp9AllowList.get("samsung").add("SM-G935A");
    vp9AllowList.get("samsung").add("SM-G935D");
    vp9AllowList.get("samsung").add("SM-G935F");
    vp9AllowList.get("samsung").add("SM-G935FD");
    vp9AllowList.get("samsung").add("SM-G935J");
    vp9AllowList.get("samsung").add("SM-G935K");
    vp9AllowList.get("samsung").add("SM-G935L");
    vp9AllowList.get("samsung").add("SM-G935P");
    vp9AllowList.get("samsung").add("SM-G935R4");
    vp9AllowList.get("samsung").add("SM-G935S");
    vp9AllowList.get("samsung").add("SM-G935T");
    vp9AllowList.get("samsung").add("SM-G935U");
    vp9AllowList.get("samsung").add("SM-G935V");
    vp9AllowList.get("samsung").add("SM-G935W8");
    vp9AllowList.get("samsung").add("SM-N9200");
    vp9AllowList.get("samsung").add("SM-N9208");
    vp9AllowList.get("samsung").add("SM-N920A");
    vp9AllowList.get("samsung").add("SM-N920C");
    vp9AllowList.get("samsung").add("SM-N920F");
    vp9AllowList.get("samsung").add("SM-N920G");
    vp9AllowList.get("samsung").add("SM-N920I");
    vp9AllowList.get("samsung").add("SM-N920K");
    vp9AllowList.get("samsung").add("SM-N920L");
    vp9AllowList.get("samsung").add("SM-N920R4");
    vp9AllowList.get("samsung").add("SM-N920R6");
    vp9AllowList.get("samsung").add("SM-N920R7");
    vp9AllowList.get("samsung").add("SM-N920S");
    vp9AllowList.get("samsung").add("SM-N920T");
    vp9AllowList.get("samsung").add("SM-N920TP");
    vp9AllowList.get("samsung").add("SM-N920V");
    vp9AllowList.get("samsung").add("SM-N920W8");
    vp9AllowList.get("samsung").add("SM-N920X");
    vp9AllowList.get("SHARP").add("AN-NP40");
    vp9AllowList.get("SHARP").add("AQUOS-4KTVJ17");
    vp9AllowList.get("SHARP").add("AQUOS-4KTVT17");
    vp9AllowList.get("SHARP").add("AQUOS-4KTVX17");
    vp9AllowList.get("SHARP").add("LC-U35T");
    vp9AllowList.get("SHARP").add("LC-UE630X");
    vp9AllowList.get("SHARP").add("LC-Ux30US");
    vp9AllowList.get("SHARP").add("LC-XU35T");
    vp9AllowList.get("SHARP").add("LC-XU930X_830X");
    vp9AllowList.get("Skyworth").add("globe");
    vp9AllowList.get("Sony").add("Amai VP9");
    vp9AllowList.get("Sony").add("BRAVIA 4K 2015");
    vp9AllowList.get("Sony").add("BRAVIA 4K GB");
    vp9AllowList.get("STMicroelectronics").add("sti4k");
    vp9AllowList.get("SumitomoElectricIndustries").add("C02AS");
    vp9AllowList.get("SumitomoElectricIndustries").add("ST4173");
    vp9AllowList.get("SumitomoElectricIndustries").add("test_STW2000");
    vp9AllowList.get("TCL").add("Percee TV");
    vp9AllowList.get("Technicolor").add("AirTV Player");
    vp9AllowList.get("Technicolor").add("Bouygtel4K");
    vp9AllowList.get("Technicolor").add("CM-7600");
    vp9AllowList.get("Technicolor").add("cooper");
    vp9AllowList.get("Technicolor").add("Foxtel Now box");
    vp9AllowList.get("Technicolor").add("pearl");
    vp9AllowList.get("Technicolor").add("Sapphire");
    vp9AllowList.get("Technicolor").add("Shortcut");
    vp9AllowList.get("Technicolor").add("skipper");
    vp9AllowList.get("Technicolor").add("STING");
    vp9AllowList.get("Technicolor").add("TIM_BOX");
    vp9AllowList.get("Technicolor").add("uzx8020chm");
    vp9AllowList.get("Vestel").add("S7252");
    vp9AllowList.get("Vestel").add("SmartTV");
    vp9AllowList.get("wnc").add("c71kw400");
    vp9AllowList.get("Xiaomi").add("MIBOX3");
    vp9AllowList.get("ZTE TV").add("AV-ATB100");
    vp9AllowList.get("ZTE TV").add("B860H");

    isVp9AllowListed =
        vp9AllowList.containsKey(Build.BRAND)
            && vp9AllowList.get(Build.BRAND).contains(Build.MODEL);
  }

  private MediaCodecUtil() {}

  /** A wrapper class of codec capability infos. */
  @UsedByNative
  public static class CodecCapabilityInfo {
    CodecCapabilityInfo(MediaCodecInfo codecInfo, String mimeType) {
      this.codecInfo = codecInfo;
      this.mimeType = mimeType;
      this.decoderName = codecInfo.getName();
      this.codecCapabilities = codecInfo.getCapabilitiesForType(mimeType);
      this.audioCapabilities = this.codecCapabilities.getAudioCapabilities();
      this.videoCapabilities = this.codecCapabilities.getVideoCapabilities();
    }

    @UsedByNative public MediaCodecInfo codecInfo;
    @UsedByNative public String mimeType;
    @UsedByNative public String decoderName;
    @UsedByNative public CodecCapabilities codecCapabilities;
    @UsedByNative public AudioCapabilities audioCapabilities;
    @UsedByNative public VideoCapabilities videoCapabilities;

    @UsedByNative
    public boolean isSecureRequired() {
      // MediaCodecList is supposed to feed us names of decoders that do NOT end in ".secure".  We
      // are then supposed to check if FEATURE_SecurePlayback is supported, and if it is and we
      // want a secure codec, we append ".secure" ourselves, and then pass that to
      // MediaCodec.createDecoderByName.  Some devices, do not follow this spec, and show us
      // decoders that end in ".secure".  Empirically, FEATURE_SecurePlayback has still been
      // correct when this happens.
      if (this.decoderName.endsWith(SECURE_DECODER_SUFFIX)) {
        // If a decoder name ends with ".secure", then we don't want to use it for clear content.
        return true;
      }
      return this.codecCapabilities.isFeatureRequired(
          MediaCodecInfo.CodecCapabilities.FEATURE_SecurePlayback);
    }

    @UsedByNative
    public boolean isSecureSupported() {
      return this.codecCapabilities.isFeatureSupported(
          MediaCodecInfo.CodecCapabilities.FEATURE_SecurePlayback);
    }

    @UsedByNative
    public boolean isTunnelModeRequired() {
      return this.codecCapabilities.isFeatureRequired(
          MediaCodecInfo.CodecCapabilities.FEATURE_TunneledPlayback);
    }

    @UsedByNative
    public boolean isTunnelModeSupported() {
      return this.codecCapabilities.isFeatureSupported(
          MediaCodecInfo.CodecCapabilities.FEATURE_TunneledPlayback);
    }

    @UsedByNative
    public boolean isSoftware() {
      return isSoftwareDecoder(this.codecInfo);
    }

    @UsedByNative
    public boolean isHdrCapable() {
      return isHdrCapableVideoDecoder(this.mimeType, this.codecCapabilities);
    }
  }

  /** Returns an array of CodecCapabilityInfo for all available decoder. */
  @SuppressWarnings("unused")
  @UsedByNative
  public static CodecCapabilityInfo[] getAllCodecCapabilityInfos() {
    List<CodecCapabilityInfo> codecCapabilityInfos = new ArrayList<>();

    for (MediaCodecInfo codecInfo : new MediaCodecList(MediaCodecList.ALL_CODECS).getCodecInfos()) {
      // We don't use encoder.
      if (codecInfo.isEncoder()) {
        continue;
      }

      // Filter blacklisted video decoders.
      String name = codecInfo.getName();
      if (!isVp9AllowListed && isCodecDenyListed(name)) {
        Log.v(TAG, "Rejecting %s, reason: codec is on deny list", name);
        continue;
      }
      if (name.endsWith(SECURE_DECODER_SUFFIX)) {
        // If we want a secure decoder, then make sure the version without ".secure" isn't
        // denylisted.
        String nameWithoutSecureSuffix =
            name.substring(0, name.length() - SECURE_DECODER_SUFFIX.length());
        if (!isVp9AllowListed && isCodecDenyListed(nameWithoutSecureSuffix)) {
          String format = "Rejecting %s, reason: offpsec denylisted secure decoder";
          Log.v(TAG, format, name);
          continue;
        }
      }

      for (String mimeType : codecInfo.getSupportedTypes()) {
        codecCapabilityInfos.add(new CodecCapabilityInfo(codecInfo, mimeType));
      }
    }
    CodecCapabilityInfo[] array = new CodecCapabilityInfo[codecCapabilityInfos.size()];
    return codecCapabilityInfos.toArray(array);
  }

  /** Returns whether the codec is denylisted. */
  public static boolean isCodecDenyListed(String codecName) {
    return videoCodecDenyList.contains(codecName);
  }

  /** Simply returns SECURE_DECODER_SUFFIX to allow access to it. */
  public static String getSecureDecoderSuffix() {
    return SECURE_DECODER_SUFFIX;
  }

  /** Determine whether codecCapabilities is capable of playing HDR. */
  public static boolean isHdrCapableVideoDecoder(
      String mimeType, CodecCapabilities codecCapabilities) {
    // AV1ProfileMain10HDR10 value was not added until API level 29.  See
    // https://developer.android.com/reference/android/media/MediaCodecInfo.CodecProfileLevel.html.
    if (mimeType.equals(AV1_MIME_TYPE) && Build.VERSION.SDK_INT < 29) {
      return false;
    }

    if (codecCapabilities == null) {
      return false;
    }
    CodecProfileLevel[] codecProfileLevels = codecCapabilities.profileLevels;
    if (codecProfileLevels == null) {
      return false;
    }
    for (CodecProfileLevel codecProfileLevel : codecProfileLevels) {
      if (mimeType.equals(VP9_MIME_TYPE)) {
        if (codecProfileLevel.profile == CodecProfileLevel.VP9Profile2HDR
            || codecProfileLevel.profile == CodecProfileLevel.VP9Profile3HDR) {
          return true;
        }
      } else if (mimeType.equals(AV1_MIME_TYPE)) {
        if (codecProfileLevel.profile == CodecProfileLevel.AV1ProfileMain10HDR10) {
          return true;
        }
      }
    }

    return false;
  }

  /** Return true if and only if info belongs to a software decoder. */
  public static boolean isSoftwareDecoder(MediaCodecInfo codecInfo) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
      if (!codecInfo.isHardwareAccelerated() || codecInfo.isSoftwareOnly()) {
        return true;
      }
    }
    String name = codecInfo.getName().toLowerCase(Locale.ROOT);
    // This is taken from libstagefright/OMXCodec.cpp for pre codec2.
    if (name.startsWith("omx.google.")) {
      return true;
    }

    // Codec2 names sw decoders this way.
    // See hardware/google/av/codec2/vndk/C2Store.cpp.
    if (name.startsWith("c2.google.") || name.startsWith("c2.android.")) {
      return true;
    }

    return false;
  }

  /**
   * The same as hasVideoDecoderFor, returns the name of the video decoder if it is found, or ""
   * otherwise.
   *
   * <p>NOTE: This code path is called repeatedly by the player to determine the decoding
   * capabilities of the device. To ensure speedy playback the code below should be kept performant.
   */
  @UsedByNative
  public static String findVideoDecoder(
      String mimeType,
      boolean mustSupportSecure,
      boolean mustSupportHdr,
      boolean mustSupportSoftwareCodec,
      boolean mustSupportTunnelMode,
      int decoderCacheTtlMs,
      int frameWidth,
      int frameHeight,
      int bitrate,
      int fps) {
    String decoderInfo =
        String.format(
            Locale.US,
            "Searching for video decoder with parameters mimeType: %s, secure: %b, frameWidth:"
                + " %d, frameHeight: %d, bitrate: %d, fps: %d, mustSupportHdr: %b,"
                + " mustSupportSoftwareCodec: %b, mustSupportTunnelMode: %b",
            mimeType,
            mustSupportSecure,
            frameWidth,
            frameHeight,
            bitrate,
            fps,
            mustSupportHdr,
            mustSupportSoftwareCodec,
            mustSupportTunnelMode);
    Log.v(TAG, decoderInfo);
    String deviceInfo =
        String.format(
            Locale.US,
            "brand: %s, model: %s, version: %s, API level: %d, isVp9AllowListed: %b",
            Build.BRAND,
            Build.MODEL,
            Build.VERSION.RELEASE,
            Build.VERSION.SDK_INT,
            isVp9AllowListed);
    Log.v(TAG, deviceInfo);

    for (VideoDecoderCache.CachedDecoder decoder :
        VideoDecoderCache.getCachedDecoders(mimeType, decoderCacheTtlMs)) {
      String name = decoder.info.getName();
      if (!isVp9AllowListed && isCodecDenyListed(name)) {
        Log.v(TAG, "Rejecting " + name + ", reason: codec is on deny list");
        continue;
      }

      if (!mustSupportSoftwareCodec && isSoftwareDecoder(decoder.info)) {
        Log.v(TAG, "Rejecting " + name + ", reason: require hardware accelerated codec");
        continue;
      }

      if (mustSupportSoftwareCodec && !isSoftwareDecoder(decoder.info)) {
        Log.v(TAG, "Rejecting " + name + ", reason: require software codec");
        continue;
      }

      // MediaCodecList is supposed to feed us names of decoders that do NOT end in ".secure".  We
      // are then supposed to check if FEATURE_SecurePlayback is supported, and if it is and we
      // want a secure codec, we append ".secure" ourselves, and then pass that to
      // MediaCodec.createDecoderByName.  Some devices, do not follow this spec, and show us
      // decoders that end in ".secure".  Empirically, FEATURE_SecurePlayback has still been
      // correct when this happens.
      if (name.endsWith(SECURE_DECODER_SUFFIX)) {
        // If we don't want a secure decoder, then don't bother messing around with this thing.
        if (!mustSupportSecure) {
          Log.v(TAG, "Rejecting " + name + ", reason: want !secure decoder and ends with .secure");
          continue;
        }
        // If we want a secure decoder, then make sure the version without ".secure" isn't
        // denylisted.
        String nameWithoutSecureSuffix =
            name.substring(0, name.length() - SECURE_DECODER_SUFFIX.length());
        if (!isVp9AllowListed && isCodecDenyListed(nameWithoutSecureSuffix)) {
          Log.v(TAG, "Rejecting " + name + ", reason: denylisted secure decoder");
        }
      }

      boolean requiresSecurePlayback =
          decoder.codecCapabilities.isFeatureRequired(
              MediaCodecInfo.CodecCapabilities.FEATURE_SecurePlayback);
      boolean supportsSecurePlayback =
          decoder.codecCapabilities.isFeatureSupported(
              MediaCodecInfo.CodecCapabilities.FEATURE_SecurePlayback);
      if (mustSupportSecure && supportsSecurePlayback
          || !mustSupportSecure && requiresSecurePlayback) {
        String message =
            String.format(
                Locale.US,
                "Rejecting %s, reason: secure decoder requested: %b, "
                    + "codec FEATURE_SecurePlayback supported: %b, required: %b",
                name,
                mustSupportSecure,
                supportsSecurePlayback,
                requiresSecurePlayback);
        Log.v(TAG, message);
        continue;
      }

      boolean requiresTunneledPlayback =
          decoder.codecCapabilities.isFeatureRequired(
              MediaCodecInfo.CodecCapabilities.FEATURE_TunneledPlayback);
      boolean supportsTunneledPlayback =
          decoder.codecCapabilities.isFeatureSupported(
              MediaCodecInfo.CodecCapabilities.FEATURE_TunneledPlayback);
      if (mustSupportTunnelMode && !supportsTunneledPlayback
          || !mustSupportTunnelMode && requiresTunneledPlayback) {
        String message =
            String.format(
                Locale.US,
                "Rejecting %s, reason: tunneled playback requested: %b, "
                    + "codec FEATURE_TunneledPlayback supported: %b, required: %b",
                name,
                mustSupportTunnelMode,
                supportsTunneledPlayback,
                requiresTunneledPlayback);
        Log.v(TAG, message);
        continue;
      }

      VideoCapabilities videoCapabilities = decoder.videoCapabilities;
      Range<Integer> supportedWidths = decoder.supportedWidths;
      Range<Integer> supportedHeights = decoder.supportedHeights;

      if (frameWidth != 0 && frameHeight != 0) {
        if (!videoCapabilities.isSizeSupported(frameWidth, frameHeight)) {
          String format = "Rejecting %s, reason: width %s is not compatible with height %d";
          Log.v(TAG, format, name, frameWidth, frameHeight);
          continue;
        }
      } else if (frameWidth != 0) {
        if (!supportedWidths.contains(frameWidth)) {
          String format = "Rejecting %s, reason: supported widths %s does not contain %d";
          Log.v(TAG, format, name, supportedWidths.toString(), frameWidth);
          continue;
        }
      } else if (frameHeight != 0) {
        if (!supportedHeights.contains(frameHeight)) {
          String format = "Rejecting %s, reason: supported heights %s does not contain %d";
          Log.v(TAG, format, name, supportedHeights.toString(), frameHeight);
          continue;
        }
      }

      Range<Integer> bitrates = videoCapabilities.getBitrateRange();
      if (bitrate != 0 && !bitrates.contains(bitrate)) {
        String format = "Rejecting %s, reason: bitrate range %s does not contain %d";
        Log.v(TAG, format, name, bitrates.toString(), bitrate);
        continue;
      }

      Range<Integer> supportedFrameRates = videoCapabilities.getSupportedFrameRates();
      if (fps != 0) {
        if (frameHeight != 0 && frameWidth != 0) {
          if (!videoCapabilities.areSizeAndRateSupported(frameWidth, frameHeight, fps)) {
            String format = "Rejecting %s, reason: supported frame rates %s does not contain %d";
            Log.v(TAG, format, name, supportedFrameRates.toString(), fps);
            continue;
          }
        } else {
          // At least one of frameHeight or frameWidth is 0
          if (!supportedFrameRates.contains(fps)) {
            String format = "Rejecting %s, reason: supported frame rates %s does not contain %d";
            Log.v(TAG, format, name, supportedFrameRates.toString(), fps);
            continue;
          }
        }
      }

      if (mustSupportHdr && !isHdrCapableVideoDecoder(mimeType, decoder.codecCapabilities)) {
        Log.v(TAG, "Rejecting " + name + ", reason: codec does not support HDR");
        continue;
      }

      String resultName =
          (mustSupportSecure && !name.endsWith(SECURE_DECODER_SUFFIX))
              ? (name + SECURE_DECODER_SUFFIX)
              : name;
      Log.v(TAG, "Found suitable decoder, " + name);
      return resultName;
    }
    return "";
  }

  /**
   * The same as hasAudioDecoderFor, only return the name of the audio decoder if it is found, and
   * "" otherwise.
   */
  @UsedByNative
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
        CodecCapabilities codecCapabilities = info.getCapabilitiesForType(supportedType);
        AudioCapabilities audioCapabilities = codecCapabilities.getAudioCapabilities();
        Range<Integer> bitrateRange =
            Range.create(0, audioCapabilities.getBitrateRange().getUpper());
        if (!bitrateRange.contains(bitrate)) {
          continue;
        }
        return name;
      }
    }
    return "";
  }
}
