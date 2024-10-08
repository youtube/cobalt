/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

/**
 * @file bear.cpp
 * @brief API implementation for IAMF decoder to BEAR renderer Interface.
 * @version 0.1
 * @date Created 03/03/2023
 **/
#define _CRT_SECURE_NO_WARNINGS
#define DLL_EXPORTS

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <memory.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#include <stdio.h>

#include "bear/api.hpp"
#include "bear/variable_block_size.hpp"
#include "ear/ear.hpp"
#include "iamf_bear_api.h"
using namespace bear;
using namespace ear;

#if defined(_WIN32)
// EXPORT_API can be used to define the dllimport storage-class attribute.
#ifdef DLL_EXPORTS
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __declspec(dllimport)
#endif
#else
#define EXPORT_API
#endif

#define N_SOURCE_CHANNELS 16
#define MAX_PATH 260

struct BearAPIImplement {
  Config *m_pConfig[N_SOURCE_CHANNELS];
  Renderer *m_pRenderer[N_SOURCE_CHANNELS];
  DirectSpeakersInput *m_pDirectSpeakersInput[N_SOURCE_CHANNELS];
  float **in[N_SOURCE_CHANNELS];
  char tf_data[MAX_PATH];
};

typedef enum {
  BS2051_A = 0x020,        // 2ch output
  BS2051_B = 0x050,        // 6ch output
  BS2051_C = 0x250,        // 8ch output
  BS2051_D = 0x450,        // 10ch output
  BS2051_E = 0x451,        // 11ch output
  BS2051_F = 0x370,        // 12ch output
  BS2051_G = 0x490,        // 14ch output
  BS2051_H = 0x9A3,        // 24ch output
  BS2051_I = 0x070,        // 8ch output
  BS2051_J = 0x470,        // 12ch output
  IAMF_STEREO = 0x200,     // 2ch input
  IAMF_51 = 0x510,         // 6ch input
  IAMF_512 = 0x512,        // 8ch input
  IAMF_514 = 0x514,        // 10ch input
  IAMF_71 = 0x710,         // 8ch input
  IAMF_714 = 0x714,        // 12ch input
  IAMF_MONO = 0x100,       // 1ch input, AOM only
  IAMF_712 = 0x712,        // 10ch input, AOM only
  IAMF_312 = 0x312,        // 6ch input, AOM only
  IAMF_BINAURAL = 0x1020,  // binaural input/output AOM only
} IAMF_SOUND_SYSTEM;

static int getmodulepath(char *path, int buffsize)
{
  int count = 0, i = 0;
#if defined(_WIN32)
  count = GetModuleFileName(NULL, path, buffsize);
#elif defined(__APPLE__)
  uint32_t size = MAX_PATH;
  _NSGetExecutablePath(path, &size); 
  count = size;
#else
  count = readlink("/proc/self/exe", path, buffsize);
#endif
  for (i = count - 1; i >= 0; --i) {
    if (path[i] == '\\' || path[i] == '/') {
      path[i] = '\0';
      return (strlen(path));
    }
  }
  return (0);
}

extern "C" EXPORT_API void *CreateBearAPI(char *tf_data_path)
{
  int i;
  struct BearAPIImplement *thiz = (struct BearAPIImplement *)malloc(sizeof(struct BearAPIImplement));
  char mod_path[MAX_PATH];

  if (thiz) {
    if (tf_data_path == NULL || strlen(tf_data_path) >= sizeof(thiz->tf_data)) {
      if (getmodulepath(mod_path, sizeof(mod_path)) > 0) {
        strcpy(thiz->tf_data, mod_path);
#if defined(_WIN32)
        strcat(thiz->tf_data, "\\default.tf");
#else
        strcat(thiz->tf_data, "/default.tf");
#endif
      } else {
        free(thiz);
        return (NULL);
      }
    } else {
      strcpy(thiz->tf_data, tf_data_path);
    }
#ifdef __linux__
    if (access(thiz->tf_data, F_OK) == -1) {
      fprintf(stderr, "%s is not found.\n", thiz->tf_data);
      free(thiz);
      return (NULL);
    }
#elif _WIN32
    DWORD attrib = GetFileAttributes(thiz->tf_data);
    if (attrib == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "%s is not found.\n", thiz->tf_data);
        free(thiz);
        return (NULL);
    } else if (attrib & FILE_ATTRIBUTE_DIRECTORY) {
        fprintf(stderr, "%s is not found.\n", thiz->tf_data);
        free(thiz);
        return (NULL);
    }
#endif
    for (i = 0; i < N_SOURCE_CHANNELS; i++) {
      thiz->m_pConfig[i] = NULL;
      thiz->m_pRenderer[i] = NULL;
      thiz->m_pDirectSpeakersInput[i] = NULL;
    }
    return ((void *)thiz);
  } else {
    return (NULL);
  }
}

extern "C" EXPORT_API void DestroyBearAPI(void *pv_thiz)
{
  int i;
  BearAPIImplement *thiz = (BearAPIImplement *)pv_thiz;

  if (pv_thiz) {
    for (i = 0; i < N_SOURCE_CHANNELS; i++) {
      if (thiz->m_pConfig[i]) delete thiz->m_pConfig[i];
      if (thiz->m_pConfig[i]) delete thiz->m_pRenderer[i];
      if (thiz->m_pConfig[i]) delete thiz->m_pDirectSpeakersInput[i];
    }
    return free(pv_thiz);
  }
}

extern "C" EXPORT_API int ConfigureBearDirectSpeakerChannel(void *pv_thiz,
                                                            int layout,
                                                            size_t nsample_per_frame,
                                                            int sample_rate)
{
  int source_id = -1;
  int i, j;
  BearAPIImplement *thiz = (BearAPIImplement *)pv_thiz;
  ear::Layout earlayout;
  int channels;

  Layout layout010 = Layout{"0+1+0",
                            std::vector<Channel>{
                                Channel{"M+000",
                                        PolarPosition{0.0, 0.0},
                                        PolarPosition{0.0, 0.0},
                                        std::make_pair(0.0, 0.0),
                                        std::make_pair(0.0, 0.0),
                                        false},
                            }};

  Layout layout230 = Layout{"2+3+0",
                            std::vector<Channel>{
                                Channel{"M+030",
                                        PolarPosition{30.0, 0.0},
                                        PolarPosition{30.0, 0.0},
                                        std::make_pair(30.0, 30.0),
                                        std::make_pair(0.0, 0.0),
                                        false},
                                Channel{"M-030",
                                        PolarPosition{-30.0, 0.0},
                                        PolarPosition{-30.0, 0.0},
                                        std::make_pair(-30.0, -30.0),
                                        std::make_pair(0.0, 0.0),
                                        false},
                                Channel{"M+000",
                                        PolarPosition{0.0, 0.0},
                                        PolarPosition{0.0, 0.0},
                                        std::make_pair(0.0, 0.0),
                                        std::make_pair(0.0, 0.0),
                                        false},
                                Channel{"LFE1",
                                        PolarPosition{45.0, -30.0},
                                        PolarPosition{45.0, -30.0},
                                        std::make_pair(-180.0, 180.0),
                                        std::make_pair(-90.0, 90.0),
                                        true},
                                Channel{"U+030",
                                        PolarPosition{30.0, 30.0},
                                        PolarPosition{30.0, 30.0},
                                        std::make_pair(30.0, 45.0),
                                        std::make_pair(30.0, 55.0),
                                        false},
                                Channel{"U-030",
                                        PolarPosition{-30.0, 30.0},
                                        PolarPosition{-30.0, 30.0},
                                        std::make_pair(-45.0, -30.0),
                                        std::make_pair(30.0, 55.0),
                                        false},
                            }};

  Layout layout270 = Layout{"2+7+0",
                            std::vector<Channel>{
                                Channel{"M+030",
                                        PolarPosition{30.0, 0.0},
                                        PolarPosition{30.0, 0.0},
                                        std::make_pair(30.0, 45.0),
                                        std::make_pair(0.0, 0.0),
                                        false},
                                Channel{"M-030",
                                        PolarPosition{-30.0, 0.0},
                                        PolarPosition{-30.0, 0.0},
                                        std::make_pair(-45.0, -30.0),
                                        std::make_pair(0.0, 0.0),
                                        false},
                                Channel{"M+000",
                                        PolarPosition{0.0, 0.0},
                                        PolarPosition{0.0, 0.0},
                                        std::make_pair(0.0, 0.0),
                                        std::make_pair(0.0, 0.0),
                                        false},
                                Channel{"LFE1",
                                        PolarPosition{45.0, -30.0},
                                        PolarPosition{45.0, -30.0},
                                        std::make_pair(-180.0, 180.0),
                                        std::make_pair(-90.0, 90.0),
                                        true},
                                Channel{"M+090",
                                        PolarPosition{90.0, 0.0},
                                        PolarPosition{90.0, 0.0},
                                        std::make_pair(85.0, 110.0),
                                        std::make_pair(0.0, 0.0),
                                        false},
                                Channel{"M-090",
                                        PolarPosition{-90.0, 0.0},
                                        PolarPosition{-90.0, 0.0},
                                        std::make_pair(-110.0, -85.0),
                                        std::make_pair(0.0, 0.0),
                                        false},
                                Channel{"M+135",
                                        PolarPosition{135.0, 0.0},
                                        PolarPosition{135.0, 0.0},
                                        std::make_pair(120.0, 150.0),
                                        std::make_pair(0.0, 0.0),
                                        false},
                                Channel{"M-135",
                                        PolarPosition{-135.0, 0.0},
                                        PolarPosition{-135.0, 0.0},
                                        std::make_pair(-150.0, -120.0),
                                        std::make_pair(0.0, 0.0),
                                        false},
                                Channel{"U+045",
                                        PolarPosition{45.0, 30.0},
                                        PolarPosition{45.0, 30.0},
                                        std::make_pair(30.0, 45.0),
                                        std::make_pair(30.0, 55.0),
                                        false},
                                Channel{"U-045",
                                        PolarPosition{-45.0, 30.0},
                                        PolarPosition{-45.0, 30.0},
                                        std::make_pair(-45.0, -30.0),
                                        std::make_pair(30.0, 55.0),
                                        false},
                            }};
  if (pv_thiz) {
    for (i = 0; i < N_SOURCE_CHANNELS; i++) {
      if (!thiz->m_pConfig[i]) break;
    }
    if (i < N_SOURCE_CHANNELS) {
      thiz->m_pConfig[i] = new Config();
      switch (layout) {
        case IAMF_STEREO:
          earlayout = ear::getLayout("0+2+0");
          channels = 2;
          break;
        case IAMF_51:
          earlayout = ear::getLayout("0+5+0");
          channels = 6;
          break;
        case IAMF_512:
          earlayout = ear::getLayout("2+5+0");
          channels = 8;
          break;
        case IAMF_514:
          earlayout = ear::getLayout("4+5+0");
          channels = 10;
          break;
        case IAMF_71:
          earlayout = ear::getLayout("0+7+0");
          channels = 8;
          break;
        case IAMF_714:
          earlayout = ear::getLayout("4+7+0");
          channels = 12;
          break;
        case IAMF_MONO:
          earlayout = layout010;  // extended
          channels = 1;
          break;
        case IAMF_712:
          earlayout = layout270;  // extended
          channels = 10;
          break;
        case IAMF_312:
          earlayout = layout230;  // extended
          channels = 6;
          break;
      }

      thiz->m_pConfig[i]->set_num_direct_speakers_channels(channels);
      thiz->m_pConfig[i]->set_period_size(nsample_per_frame);
      thiz->m_pConfig[i]->set_sample_rate(sample_rate);
      thiz->m_pConfig[i]->set_data_path(thiz->tf_data);

      thiz->m_pRenderer[i] = new Renderer(*thiz->m_pConfig[i]);
      thiz->m_pDirectSpeakersInput[i] = new DirectSpeakersInput();

      for (j = 0; j < earlayout.channels().size(); j++) {
        double Azi = earlayout.channels().at(j).polarPosition().azimuth;
        double El = earlayout.channels().at(j).polarPosition().elevation;
        double Dist = earlayout.channels().at(j).polarPosition().distance;

        thiz->m_pDirectSpeakersInput[i]->type_metadata.position = ear::PolarSpeakerPosition(Azi, El, Dist);
        thiz->m_pRenderer[i]->add_direct_speakers_block(j, *thiz->m_pDirectSpeakersInput[i]);
      }

      source_id = i;
    }
  }

  return (source_id);
}

extern "C" EXPORT_API int SetBearDirectSpeakerChannel(void *pv_thiz, int source_id, float **in)
{
  BearAPIImplement *thiz = (BearAPIImplement *)pv_thiz;
  if (0 <= source_id && source_id < N_SOURCE_CHANNELS) {
    if (pv_thiz && thiz->m_pConfig[source_id] != NULL) {
      thiz->in[source_id] = in;
      return (0);
    }
  }
  return (-1);
}

extern "C" EXPORT_API void DestroyBearChannel(void *pv_thiz, int source_id)
{
  BearAPIImplement *thiz = (BearAPIImplement *)pv_thiz;

  if (pv_thiz) {
    if (0 <= source_id && source_id < N_SOURCE_CHANNELS) {
      if (thiz->m_pConfig[source_id]) {
        if (thiz->m_pConfig[source_id]) delete thiz->m_pConfig[source_id];
        if (thiz->m_pConfig[source_id]) delete thiz->m_pRenderer[source_id];
        if (thiz->m_pConfig[source_id]) delete thiz->m_pDirectSpeakersInput[source_id];
        thiz->m_pConfig[source_id] = NULL;
        thiz->m_pRenderer[source_id] = NULL;
        thiz->m_pDirectSpeakersInput[source_id] = NULL;
        thiz->in[source_id] = NULL;
      }
    }
  }
}

extern "C" EXPORT_API int GetBearRenderedAudio(void *pv_thiz, int source_id, float **out)
{
  BearAPIImplement *thiz = (BearAPIImplement *)pv_thiz;

  if (pv_thiz) {
    if (0 <= source_id && source_id < N_SOURCE_CHANNELS && thiz->in[source_id] != NULL) {
      if (thiz->m_pConfig[source_id]) {
        thiz->m_pRenderer[source_id]->process(NULL, thiz->in[source_id], NULL, out);
        return (0);
      }
    }
  }
  return (-1);
}
