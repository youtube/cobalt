#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>
#include <random>
#include "opus.h"
#include "opus_multistream.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const int kSampleRate = 48000;
const int kFrameSize = 960; // 20ms at 48kHz
const int kIterations = 100000;

// Generates a complex valid Opus packet by encoding a synthesized signal
void GenerateComplexOpusPacket(std::vector<unsigned char>& packet, int channels) {
  int error;
  OpusEncoder* enc = opus_encoder_create(kSampleRate, channels, OPUS_APPLICATION_AUDIO, &error);
  if (error != OPUS_OK) {
    std::cerr << "Failed to create encoder: " << error << std::endl;
    return;
  }
  
  // Set high bitrate to ensure complex encoding
  opus_encoder_ctl(enc, OPUS_SET_BITRATE(channels == 2 ? 128000 : 64000));
  
  int num_samples = kFrameSize * channels;
  std::vector<float> input_pcm(num_samples);
  std::default_random_engine generator(12345); // Fixed seed for reproducibility
  std::uniform_real_distribution<float> distribution(-0.1f, 0.1f);
  
  for (int i = 0; i < kFrameSize; ++i) {
    float t = (float)i / kSampleRate;
    if (channels == 1) {
      // Mono: multi-tone + noise
      input_pcm[i] = std::sin(2.0 * M_PI * 440.0 * t) + 
                     0.5 * std::sin(2.0 * M_PI * 880.0 * t) + 
                     0.2 * std::sin(2.0 * M_PI * 1200.0 * t) +
                     distribution(generator);
    } else {
      // Stereo: different signals on left/right to ensure coupling is active
      input_pcm[2 * i] = std::sin(2.0 * M_PI * 440.0 * t) + 
                         0.5 * std::sin(2.0 * M_PI * 880.0 * t) + 
                         distribution(generator); // Left
      input_pcm[2 * i + 1] = std::sin(2.0 * M_PI * 660.0 * t) + 
                             0.5 * std::sin(2.0 * M_PI * 990.0 * t) + 
                             distribution(generator); // Right
    }
  }
  
  packet.resize(kFrameSize * channels * sizeof(float));
  
  int bytes_encoded = opus_encode_float(enc, input_pcm.data(), kFrameSize, packet.data(), packet.size());
  if (bytes_encoded < 0) {
    std::cerr << "Encoding failed: " << bytes_encoded << std::endl;
    packet.clear();
  } else {
    packet.resize(bytes_encoded);
    std::cout << "Generated complex packet of size " << bytes_encoded << " bytes for " << channels << " channels." << std::endl;
  }
  
  opus_encoder_destroy(enc);
}

double BenchmarkSingleStream(int channels, const std::vector<unsigned char>& packet) {
  int error;
  OpusDecoder* dec = opus_decoder_create(kSampleRate, channels, &error);
  if (error != OPUS_OK) {
    std::cerr << "Failed to create decoder: " << error << std::endl;
    return -1.0;
  }
  std::vector<float> pcm(kFrameSize * channels);
  double dummy_sum = 0;
  
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < kIterations; ++i) {
    int ret = opus_decode_float(dec, packet.data(), packet.size(), pcm.data(), kFrameSize, 0);
    if (ret > 0) {
      dummy_sum += pcm[0];
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  
  std::chrono::duration<double, std::milli> duration = end - start;
  double ns_per_call = (duration.count() / kIterations) * 1000000.0;
  std::cout << "Single-stream (" << channels << "ch): " 
            << ns_per_call << " ns/call (" 
            << duration.count() << " ms total, dummy_sum: " << dummy_sum << ")" << std::endl;
            
  opus_decoder_destroy(dec);
  return ns_per_call;
}

double BenchmarkMultiStream(int channels, const std::vector<unsigned char>& packet) {
  int error;
  unsigned char mapping[2] = {0, 1};
  int streams = 1;
  int coupled_streams = (channels == 2) ? 1 : 0;
  
  OpusMSDecoder* dec = opus_multistream_decoder_create(
      kSampleRate, channels, streams, coupled_streams, mapping, &error);
  if (error != OPUS_OK) {
    std::cerr << "Failed to create MS decoder: " << error << std::endl;
    return -1.0;
  }
  std::vector<float> pcm(kFrameSize * channels);
  double dummy_sum = 0;
  
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < kIterations; ++i) {
    int ret = opus_multistream_decode_float(dec, packet.data(), packet.size(), pcm.data(), kFrameSize, 0);
    if (ret > 0) {
      dummy_sum += pcm[0];
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  
  std::chrono::duration<double, std::milli> duration = end - start;
  double ns_per_call = (duration.count() / kIterations) * 1000000.0;
  std::cout << "Multi-stream (" << channels << "ch):  " 
            << ns_per_call << " ns/call (" 
            << duration.count() << " ms total, dummy_sum: " << dummy_sum << ")" << std::endl;
            
  opus_multistream_decoder_destroy(dec);
  return ns_per_call;
}

int main() {
  std::vector<unsigned char> mono_packet;
  std::vector<unsigned char> stereo_packet;

  std::cout << "Generating complex packets..." << std::endl;
  GenerateComplexOpusPacket(mono_packet, 1);
  GenerateComplexOpusPacket(stereo_packet, 2);

  if (mono_packet.empty() || stereo_packet.empty()) {
    std::cerr << "Failed to generate test packets." << std::endl;
    return 1;
  }

  std::cout << "\nRunning benchmark with " << kIterations << " iterations..." << std::endl;
  
  // Warm up
  BenchmarkSingleStream(1, mono_packet);
  
  std::cout << "\n--- Mono Tests ---" << std::endl;
  double mono_single = BenchmarkSingleStream(1, mono_packet);
  double mono_multi = BenchmarkMultiStream(1, mono_packet);
  if (mono_single > 0 && mono_multi > 0) {
    double mono_diff = mono_multi - mono_single;
    double mono_percent = (mono_diff / mono_multi) * 100.0;
    std::cout << "Overhead saved (Mono): " << mono_diff << " ns/call (" << mono_percent << "%)" << std::endl;
  }
  
  std::cout << "\n--- Stereo Tests ---" << std::endl;
  double stereo_single = BenchmarkSingleStream(2, stereo_packet);
  double stereo_multi = BenchmarkMultiStream(2, stereo_packet);
  if (stereo_single > 0 && stereo_multi > 0) {
    double stereo_diff = stereo_multi - stereo_single;
    double stereo_percent = (stereo_diff / stereo_multi) * 100.0;
    std::cout << "Overhead saved (Stereo): " << stereo_diff << " ns/call (" << stereo_percent << "%)" << std::endl;
  }

  return 0;
}
