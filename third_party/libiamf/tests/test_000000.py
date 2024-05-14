import yaml
import inspect
import wave
import argparse

parser = argparse.ArgumentParser(description='test_000000 verification script')
parser.add_argument('--log',  type=str, required=True, help='decoder verification log output')
parser.add_argument('--wav',  type=str, required=True, help='decoder verification wav output')
args = parser.parse_args()

total_nobus = 0
total_length_0 = 0
total_frames = 0
codec_samplerate_0 = 0

obu_list = []
open_sharp = 0
read_s = ""
with open(args.log,"r") as log_data:
    for line in log_data:
        if line == "#0\n":
            open_sharp = 1
        elif line == "##\n":
            open_sharp = 0
            json_o = yaml.load(read_s, Loader=yaml.FullLoader)
            obu_list.append(json_o)
            read_s = ""
            total_nobus += 1
        elif open_sharp == 1:
            read_s += line

pass_count = 0    
if total_nobus != 68: #check total number of obues to be read
    frame = inspect.currentframe()
    print("failure line is #%d."%(frame.f_lineno))
else:
    pass_count += 1

    if "MagicCodeOBU_0" in obu_list[0]:
        pass_count += 1
        obu = obu_list[0]["MagicCodeOBU_0"]
        if obu[0]["ia_code"] == 1767992678: #"iamf"
            pass_count += 1
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
        if obu[1]["version"] == 0: #0
            pass_count += 1
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
        if obu[2]["profile_version"] == 0: #simple profile
            pass_count += 1
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
    else:
        frame = inspect.currentframe() #MagicCodeOBU_0
        print("failure line is #%d."%(frame.f_lineno))

    if "CodecConfigOBU_1" in obu_list[1]:
        pass_count += 1
        obu = obu_list[1]["CodecConfigOBU_1"]
        if obu[0]["codec_config_id"] == 0:
            pass_count += 1
            if "codec_config" in obu[1]:
                pass_count += 1
                obu_1 = obu[1]["codec_config"]
                if obu_1[0]["codec_id"] == 1768973165: #"ipcm"
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
                if obu_1[1]["num_samples_per_frame"] == 128:
                    pass_count += 1
                    num_samples_per_frame_56 = obu_1[1]["num_samples_per_frame"]
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))            
                if obu_1[2]["roll_distance"] == 0:
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
                if "decoder_config" in obu_1[3]:
                    pass_count += 1
                    obu_2 = obu_1[3]["decoder_config"]
                    if obu_2[0]["sample_format_flags"] == 1: #little endian
                        pass_count += 1
                    else:
                        frame = inspect.currentframe()
                        print("failure line is #%d."%(frame.f_lineno))
                    if obu_2[1]["sample_size"] == 16: # int16_t
                        pass_count += 1
                        sample_size_75 = obu_2[1]["sample_size"]
                    else:
                        frame = inspect.currentframe()
                        print("failure line is #%d."%(frame.f_lineno))
                    if obu_2[2]["sample_rate"] == 16000: #16kHz
                        pass_count += 1
                        codec_samplerate_0 = obu_2[2]["sample_rate"]
                    else:
                        frame = inspect.currentframe()
                        print("failure line is #%d."%(frame.f_lineno))
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
            else:
                frame = inspect.currentframe()
                print("failure line is #%d."%(frame.f_lineno))
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
    else:
        frame = inspect.currentframe() #CodecConfigOBU_1
        print("failure line is #%d."%(frame.f_lineno))

    if "AudioElementOBU_2" in obu_list[2]:
        pass_count += 1
        obu = obu_list[2]["AudioElementOBU_2"]
        if obu[0]["audio_element_id"] == 0:
            pass_count += 1
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
        if obu[1]["audio_element_type"] == 0:
            pass_count += 1
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
        if obu[2]["codec_config_id"] == 0:
            pass_count += 1
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
        if obu[3]["num_substreams"] == 1:
            pass_count += 1
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
        if obu[4]["audio_substream_id_0"] == 0:
            pass_count += 1
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
        if obu[5]["num_parameters"] == 0:
            pass_count += 1
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
        if "scalable_channel_layout_config" in obu[6]:
            pass_count += 1
            obu_1 = obu[6]["scalable_channel_layout_config"]
            if obu_1[0]["num_layers"] == 1:
                pass_count += 1
            else:
                frame = inspect.currentframe()
                print("failure line is #%d."%(frame.f_lineno))
            if "channel_audio_layer_configs_0" in obu_1[1]:
                pass_count += 1
                obu_2 = obu_1[1]["channel_audio_layer_configs_0"]
                if obu_2[0]["loudspeaker_layout"] == 1: #141
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
                loudspeaker_layout_141 = obu_2[0]["loudspeaker_layout"]
                if obu_2[1]["output_gain_is_present_flag"] == 0:
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
                if obu_2[2]["recon_gain_is_present_flag"] == 0:
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
                if obu_2[3]["substream_count"] == 1: #157
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
                substream_count_157 = obu_2[3]["substream_count"]
                if obu_2[4]["coupled_substream_count"] == 1: #163
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
                coupled_substream_count_163 = obu_2[4]["coupled_substream_count"]
            else:
                frame = inspect.currentframe()
                print("failure line is #%d."%(frame.f_lineno))
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
    else:
        frame = inspect.currentframe() #AudioElementOBU_2
        print("failure line is #%d."%(frame.f_lineno))

    if "MixPresentationOBU_3" in obu_list[3]:
        pass_count += 1
        obu = obu_list[3]["MixPresentationOBU_3"]
        if obu[0]["mix_presentation_id"] == 42:
            pass_count += 1
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
        if obu[1]["mix_presentation_annotations"]["mix_presentation_friendly_label"] == "test_mix_pres":
            pass_count += 1
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
        if obu[2]["num_sub_mixes"] == 1:
            pass_count += 1
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
        if "sub_mixes_0" in obu[3]:
            pass_count += 1
            obu_1 = obu[3]["sub_mixes_0"]
            if obu_1[0]["num_audio_elements"] == 1:
                pass_count += 1
            else:
                frame = inspect.currentframe()
                print("failure line is #%d."%(frame.f_lineno))
            if "audio_elements_0" in obu_1[1]:
                pass_count += 1
                obu_2 = obu_1[1]["audio_elements_0"]
                if obu_2[0]["audio_element_id"] == 0:
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
                if obu_2[1]["mix_presentation_element_annotations"]["audio_element_friendly_label"] == "test_sub_mix_0_audio_element_0":
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
                if obu_2[2]["rendering_config"] == None:
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
                if "mix_gain" in obu_2[3]["element_mix_config"][0]:
                    pass_count += 1
                    obu_3 = obu_2[3]["element_mix_config"][0]["mix_gain"]
                    if obu_3[0]["parameter_id"] == 0:
                        pass_count += 1
                    else:
                        frame = inspect.currentframe()
                        print("failure line is #%d."%(frame.f_lineno))
                    if obu_3[1]["parameter_rate"] == 0:
                        pass_count += 1
                    else:
                        frame = inspect.currentframe()
                        print("failure line is #%d."%(frame.f_lineno))
                    if obu_3[2]["param_definition_mode"] == 1:
                        pass_count += 1
                    else:
                        frame = inspect.currentframe()
                        print("failure line is #%d."%(frame.f_lineno))
                    if obu_3[3]["default_mix_gain"] == 0:
                        pass_count += 1
                    else:
                        frame = inspect.currentframe()
                        print("failure line is #%d."%(frame.f_lineno))
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
            if "output_mix_gain" in obu_1[2]["output_mix_config"][0]:
                pass_count += 1
                obu_2 = obu_1[2]["output_mix_config"][0]["output_mix_gain"]
                if obu_2[0]["parameter_id"] == 0:
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
                if obu_2[1]["parameter_rate"] == 0:
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
                if obu_2[2]["param_definition_mode"] == 1:
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
                if obu_2[3]["default_mix_gain"] == 0:
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
            if obu_1[3]["num_layouts"] == 1:
                pass_count += 1
            else:
                frame = inspect.currentframe()
                print("failure line is #%d."%(frame.f_lineno))
            if "loudness_layout" in obu_1[4]["layouts_0"][0]:
                pass_count += 1
                obu_2 = obu_1[4]["layouts_0"][0]["loudness_layout"]
                if obu_2[0]["layout_type"] == 2:
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
                if obu_2[1]["sound_system"] == 0:
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
            if "loudness" in obu_1[4]["layouts_0"][1]:
                pass_count += 1
                obu_2 = obu_1[4]["layouts_0"][1]["loudness"]
                if obu_2[0]["info_type"] == 0:
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
                if obu_2[1]["integrated_loudness"] == -13731:
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
                if obu_2[2]["digital_peak"] == -12879:
                    pass_count += 1
                else:
                    frame = inspect.currentframe()
                    print("failure line is #%d."%(frame.f_lineno))
            else:
                frame = inspect.currentframe()
                print("failure line is #%d."%(frame.f_lineno))
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
    else:
        frame = inspect.currentframe() #MixPresentationOBU_3
        print("failure line is #%d."%(frame.f_lineno))

    if "SyncOBU_4" in obu_list[4]:
        pass_count += 1
        obu = obu_list[4]["SyncOBU_4"]
        if obu[0]["global_offset"] == 0:
            pass_count += 1
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
        if obu[1]["num_obu_ids"] == 1:
            pass_count += 1
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
        if "sync_array_0" in obu[2]:
            pass_count += 1
            obu_1 = obu[2]["sync_array_0"]
            if obu_1[0]["obu_id"] == 0:
                pass_count += 1
            else:
                frame = inspect.currentframe()
                print("failure line is #%d."%(frame.f_lineno))
            if obu_1[1]["obu_data_type"] == 0:
                pass_count += 1
            else:
                frame = inspect.currentframe()
                print("failure line is #%d."%(frame.f_lineno))
            if obu_1[2]["reinitialize_decoder"] == 0:
                pass_count += 1
            else:
                frame = inspect.currentframe()
                print("failure line is #%d."%(frame.f_lineno))
            if obu_1[3]["relative_offset"] == 0:
                pass_count += 1
            else:
                frame = inspect.currentframe()
                print("failure line is #%d."%(frame.f_lineno))
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
    else:
        frame = inspect.currentframe() #SyncOBU_4
        print("failure line is #%d."%(frame.f_lineno))

    for i in range(5,67):
        if "AudioFrameOBU_%d"%(i) in obu_list[i]:
            pass_count += 1
            obu = obu_list[i]["AudioFrameOBU_%d"%(i)]
            if obu[0]["audio_substream_id"] == 0:
                pass_count += 1
            else:
                frame = inspect.currentframe()
                print("failure line is #%d."%(frame.f_lineno))
            if obu[1]["size_of(audio_frame)"] == 512:
                pass_count += 1
                size_of_audio_frame_370 = obu[1]["size_of(audio_frame)"]
                total_length_0 += size_of_audio_frame_370
            else:
                frame = inspect.currentframe()
                print("failure line is #%d."%(frame.f_lineno))
        else:
            frame = inspect.currentframe() #AudioFrameOBU_5
            print("failure line is #%d."%(frame.f_lineno))

    if "AudioFrameOBU_67" in obu_list[67]:
        pass_count += 1
        obu = obu_list[67]["AudioFrameOBU_67"]
        if obu[0]["audio_substream_id"] == 0:
            pass_count += 1
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))
        if obu[1]["size_of(audio_frame)"] == 256:
            pass_count += 1
            size_of_audio_frame_389 = obu[1]["size_of(audio_frame)"]
            total_length_0 += size_of_audio_frame_389
        else:
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))

    #0001           :      Stereo      : L/R
                
    if loudspeaker_layout_141 == 1: #0001 Stereo
        nch = coupled_substream_count_163 + substream_count_157
        if nch == 2:
            pass_count += 1
        else:    
            frame = inspect.currentframe()
            print("failure line is #%d."%(frame.f_lineno))

    if (2 * num_samples_per_frame_56 * sample_size_75//8) == size_of_audio_frame_370: #channel per substream == 2
        pass_count += 1
    else:
        frame = inspect.currentframe()
        print("failure line is #%d."%(frame.f_lineno))

    total_nframes = total_length_0//(sample_size_75//8)//2 #(total_byte_length of substream#0 to be read)//(bitdepth//8)//(channel per substream)
    if total_nframes == 8000:
        pass_count += 1
    else:
        frame = inspect.currentframe()
        print("failure line is #%d."%(frame.f_lineno))

    test_wf = wave.open(args.wav, mode="rb")
    test_nch = test_wf.getnchannels()
    test_sampledepth = test_wf.getsampwidth()
    test_samplerate = test_wf.getframerate()
    test_nframes = test_wf.getnframes()
    if int(total_nframes / codec_samplerate_0 * 1000) == int(test_nframes / test_samplerate * 1000): #audio length is mismatched
        pass_count += 1
    else:
        frame = inspect.currentframe()
        print("failure line is #%d."%(frame.f_lineno))
    test_wf.close()

    no_of_check_items = 259
    if pass_count == no_of_check_items:
        print("%d item(s) of %s is(are) passed."%(pass_count, args.log))
    else:
        print("%d item(s) of %s is(are) passed."%(pass_count, args.log))
        print("%d item(s) of %s is(are) failed."%(no_of_check_items-pass_count, args.log))
